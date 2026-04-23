// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/patch.h"

#include "base/memory/stack_allocated.h"
#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/parser/html_construction_site.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

namespace {

class NodeRemovalScope {
  STACK_ALLOCATED();

 public:
  void Remove(Node* node) { nodes_to_remove.push_back(node); }

  ~NodeRemovalScope() {
    for (Node* node : nodes_to_remove) {
      node->remove();
    }
  }

 private:
  HeapVector<Member<Node>> nodes_to_remove;
};

}  // namespace

Patch* Patch::Prepare(ContainerNode* scope, const AtomicString& marker_name) {
  if (!RuntimeEnabledFeatures::DocumentPatchingEnabled() ||
      marker_name.IsNull() || marker_name.empty()) {
    return nullptr;
  }

  if (auto* parent_template = DynamicTo<HTMLTemplateElement>(scope)) {
    scope = parent_template->InsertionTarget();
  } else if (scope == scope->GetDocument().body()) {
    scope = scope->GetDocument().documentElement();
  }

  DEFINE_STATIC_LOCAL(AtomicString, kNamePseudoAttr, ("name"));
  DEFINE_STATIC_LOCAL(AtomicString, kMarkerTarget, ("marker"));
  DEFINE_STATIC_LOCAL(AtomicString, kStartTarget, ("start"));
  DEFINE_STATIC_LOCAL(AtomicString, kEndTarget, ("end"));

  for (Node& descendant : NodeTraversal::DescendantsOf(*scope)) {
    auto* processing_instruction = DynamicTo<ProcessingInstruction>(descendant);
    if (!processing_instruction ||
        (processing_instruction->GetAttributeValue(
             kNamePseudoAttr, g_empty_atom) != marker_name)) {
      continue;
    }
    if (processing_instruction->target() == kMarkerTarget) {
      return MakeGarbageCollected<Patch>(
          base::PassKey<Patch>(), processing_instruction->parentNode(),
          processing_instruction, processing_instruction);
    }

    if (processing_instruction->target() != kStartTarget) {
      continue;
    }

    ContainerNode* parent = processing_instruction->parentNode();
    int marker_depth = 0;
    NodeRemovalScope remove_scope;

    for (Node* node = processing_instruction->nextSibling(); node;
         node = node->nextSibling()) {
      if (ProcessingInstruction* next_processing_instruction =
              DynamicTo<ProcessingInstruction>(*node)) {
        if (next_processing_instruction->target() == kStartTarget) {
          marker_depth++;
        } else if (next_processing_instruction->target() == kEndTarget) {
          if (marker_depth == 0) {
            return MakeGarbageCollected<Patch>(base::PassKey<Patch>(), parent,
                                               processing_instruction,
                                               next_processing_instruction);
          }
          marker_depth--;
        }
      }

      remove_scope.Remove(node);
    }

    // No end PI found.
    return MakeGarbageCollected<Patch>(base::PassKey<Patch>(), parent,
                                       processing_instruction, nullptr);
  }

  // No start/marker PI found.
  return nullptr;
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

  // In normal parsing, positional style invalidation and other effects happen
  // when finishehd parsing. When patching, we need to run the same logic when
  // the patch finalizes.
  if (Element* element = DynamicTo<Element>(*parent_)) {
    element->DidFinishParsingChildren();
  }
}

void Patch::Trace(Visitor* visitor) const {
  visitor->Trace(parent_);
  visitor->Trace(start_marker_);
  visitor->Trace(end_marker_);
}

}  // namespace blink
