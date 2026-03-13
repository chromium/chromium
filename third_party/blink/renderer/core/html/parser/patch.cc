// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/patch.h"

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/parser/html_construction_site.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {
Patch* Patch::Prepare(ContainerNode* scope, const AtomicString& target) {
  if (!RuntimeEnabledFeatures::DocumentPatchingEnabled() || target.IsNull() ||
      target.empty()) {
    return nullptr;
  }

  AtomicString host_name = target;
  AtomicString marker_name = g_empty_atom;

  auto index_of_hash = target.find('#');
  if (index_of_hash != AtomicString::npos) {
    const String as_string = target.GetString();
    host_name = AtomicString(as_string.Left(index_of_hash));
    marker_name = AtomicString(as_string.Substring(index_of_hash + 1));
  }

  Document& document = scope->GetDocument();

  if (auto* parent_template = DynamicTo<HTMLTemplateElement>(scope)) {
    // TODO(nrosenthal): <template for> inside <template for>.
    scope = parent_template->InsertionTarget();
  } else if (scope == document.body()) {
    scope = document.documentElement();
  }

  ContainerNode* host = nullptr;

  if (ShadowRoot* as_shadow = DynamicTo<ShadowRoot>(scope)) {
    if (as_shadow->marker() == host_name) {
      host = as_shadow;
    }
  }

  if (!host) {
    for (Node& descendant : NodeTraversal::InclusiveDescendantsOf(*scope)) {
      if (Element* element = DynamicTo<Element>(descendant)) {
        if (element->marker() == host_name) {
          host = element;
          break;
        }
      }
    }
  }

  if (!host) {
    return nullptr;
  }

  int marker_depth = 0;
  ProcessingInstruction* start_marker = nullptr;
  ProcessingInstruction* end_marker = nullptr;
  for (Node& child : NodeTraversal::ChildrenOf(*host)) {
    auto* processing_instruction = DynamicTo<ProcessingInstruction>(child);
    if (!processing_instruction) {
      continue;
    }

    AtomicString current_target(
        processing_instruction->target().ToAsciiLower());
    DEFINE_STATIC_LOCAL(AtomicString, kNamePseudoAttr, ("name"));
    DEFINE_STATIC_LOCAL(AtomicString, kStart, ("start"));
    DEFINE_STATIC_LOCAL(AtomicString, kEnd, ("end"));
    DEFINE_STATIC_LOCAL(AtomicString, kMarker, ("marker"));

    auto is_name_matching = [&]() {
      return processing_instruction->GetAttributeValue(
                 kNamePseudoAttr, g_empty_atom) == marker_name;
    };

    if (current_target == kMarker && !start_marker && is_name_matching()) {
      return MakeGarbageCollected<Patch>(base::PassKey<Patch>(), host,
                                         processing_instruction,
                                         processing_instruction);
    }

    if (current_target == kStart) {
      if (start_marker) {
        marker_depth++;
      } else if (is_name_matching()) {
        start_marker = processing_instruction;
      }
    } else if (current_target == kEnd && start_marker) {
      if (marker_depth == 0) {
        end_marker = processing_instruction;
        break;
      }
      marker_depth--;
    }
  }

  if (!start_marker) {
    return nullptr;
  }

  DCHECK(EqualIgnoringAsciiCase(start_marker->target(), "start"));
  DCHECK(!end_marker || EqualIgnoringAsciiCase(end_marker->target(), "end"));

  for (Node* child = start_marker->nextSibling();
       child && (child != end_marker); child = start_marker->nextSibling()) {
    host->RemoveChild(child);
  }

  return MakeGarbageCollected<Patch>(base::PassKey<Patch>(), host, start_marker,
                                     end_marker);
}

void Patch::Apply(HTMLConstructionSiteTask& task) {
  task.parent = parent_;
  task.next_child = end_marker_ && end_marker_->parentNode() == parent_
                        ? end_marker_
                        : nullptr;
}

void Patch::Finalize() {
  CHECK(RuntimeEnabledFeatures::DocumentPatchingEnabled());
  if (ContainerNode* start_parent = start_marker_->parentNode()) {
    start_parent->ParserRemoveChild(*start_marker_);
  }
  if (end_marker_ && end_marker_ != start_marker_) {
    if (ContainerNode* end_parent = end_marker_->parentNode()) {
      end_parent->ParserRemoveChild(*end_marker_);
    }
  }
}

void Patch::Trace(Visitor* visitor) const {
  visitor->Trace(parent_);
  visitor->Trace(start_marker_);
  visitor->Trace(end_marker_);
}

}  // namespace blink
