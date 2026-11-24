// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/patch.h"

#include "base/check.h"
#include "base/memory/stack_allocated.h"
#include "base/no_destructor.h"
#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/dom/active_template_mutation_scope.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/mutation_observer_interest_group.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/throw_on_dynamic_markup_insertion_count_incrementer.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/parser/external_patch_loader.h"
#include "third_party/blink/renderer/core/html/parser/html_construction_site.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {

namespace {

class [[nodiscard]] NodeRemovalScope {
  STACK_ALLOCATED();

 public:
  explicit NodeRemovalScope(Document& document) : document_(document) {}

  void Remove(Node* node) { nodes_to_remove.push_back(node); }

  void SetActiveTemplate(ActiveTemplateInvocation* invocation,
                         ContainerNode* parent,
                         Node* start_marker) {
    invocation_ = invocation;
    parent_ = parent;
    start_marker_ = start_marker;
  }

  ~NodeRemovalScope() {
    ThrowOnDynamicMarkupInsertionCountIncrementer
        throw_on_dynamic_markup_insertions(&document_);
    if (!invocation_) {
      for (Node* node : nodes_to_remove) {
        node->remove();
      }
      return;
    }

    bool emitted_start_on_parent = false;
    for (Node* node : nodes_to_remove) {
      if (node->parentNode() == parent_) {
        ActiveTemplateMutationScope template_scope(
            V8MutationRecordActiveTemplateStatus::Enum::kStart, invocation_);
        node->remove();
        emitted_start_on_parent |= template_scope.WasUsed();
      } else {
        node->remove();
      }
    }
    if (!emitted_start_on_parent && parent_) {
      MutationObserverInterestGroup::EnqueueActiveTemplateRecord(
          *parent_, V8MutationRecordActiveTemplateStatus::Enum::kStart,
          invocation_, start_marker_,
          start_marker_ ? start_marker_->nextSibling() : nullptr);
    }
  }

 private:
  Document& document_;
  HeapVector<Member<Node>> nodes_to_remove;
  ActiveTemplateInvocation* invocation_ = nullptr;
  ContainerNode* parent_ = nullptr;
  Node* start_marker_ = nullptr;
};

}  // namespace

Patch* Patch::Prepare(ContainerNode* scope,
                      const AtomicString& marker_name,
                      HTMLTemplateElement* template_element,
                      StreamingSanitizer* parent_sanitizer,
                      ParserContentPolicy content_policy) {
  if (marker_name.empty()) {
    return nullptr;
  }

  while (auto* parent_template = DynamicTo<HTMLTemplateElement>(scope)) {
    if (auto* parent_patch = parent_template->GetPatch()) {
      if (!parent_patch->IsExternal()) {
        scope = parent_patch->parent_;
        continue;
      }
    }
    scope = parent_template->InsertionTarget();
  }

  const bool has_external_src =
      template_element &&
      RuntimeEnabledFeatures::DeclarativeFragmentEnabled() &&
      !template_element->FastGetAttribute(html_names::kSrcAttr).empty();
  const bool track_active_template =
      RuntimeEnabledFeatures::MutationObserverActiveTemplateEnabled() &&
      !has_external_src;

  ContainerNode* parent = nullptr;
  Node* start_marker = nullptr;
  Node* end_marker = nullptr;
  bool had_end_marker = false;
  ActiveTemplateInvocation* invocation = nullptr;

  if (scope == scope->GetDocument().body()) {
    scope = scope->GetDocument().documentElement();
  }

  static base::NoDestructor<AtomicString> kNamePseudoAttr(
      AtomicString::FromLiteral<"name">());
  static base::NoDestructor<AtomicString> kMarkerTarget(
      AtomicString::FromLiteral<"marker">());
  static base::NoDestructor<AtomicString> kStartTarget(
      AtomicString::FromLiteral<"start">());
  static base::NoDestructor<AtomicString> kEndTarget(
      AtomicString::FromLiteral<"end">());

  for (Node& descendant : NodeTraversal::DescendantsOf(*scope)) {
    auto* processing_instruction = DynamicTo<ProcessingInstruction>(descendant);
    if (!processing_instruction ||
        (processing_instruction->GetAttributeValue(
             *kNamePseudoAttr, g_empty_atom) != marker_name)) {
      continue;
    }
    if (processing_instruction->target() == *kMarkerTarget) {
      parent = processing_instruction->parentNode();
      start_marker = processing_instruction;
      end_marker = processing_instruction;
      had_end_marker = true;
      if (track_active_template) {
        invocation = ActiveTemplateInvocation::Create(template_element, parent);
        if (parent && invocation) {
          MutationObserverInterestGroup::EnqueueActiveTemplateRecord(
              *parent, V8MutationRecordActiveTemplateStatus::Enum::kStart,
              invocation, start_marker->previousSibling(), start_marker);
        }
      }
      break;
    }

    if (processing_instruction->target() != *kStartTarget) {
      continue;
    }

    ContainerNode* pi_parent = processing_instruction->parentNode();
    int marker_depth = 0;
    NodeRemovalScope remove_scope(scope->GetDocument());

    for (Node* node = processing_instruction->nextSibling(); node;
         node = node->nextSibling()) {
      if (ProcessingInstruction* next_processing_instruction =
              DynamicTo<ProcessingInstruction>(*node)) {
        if (next_processing_instruction->target() == *kStartTarget) {
          marker_depth++;
        } else if (next_processing_instruction->target() == *kEndTarget) {
          if (marker_depth == 0) {
            parent = pi_parent;
            start_marker = processing_instruction;
            end_marker = next_processing_instruction;
            had_end_marker = true;
            break;
          }
          marker_depth--;
        }
      }

      remove_scope.Remove(node);
    }

    if (!parent) {
      // No end PI found.
      parent = pi_parent;
      start_marker = processing_instruction;
      end_marker = nullptr;
    }

    if (track_active_template && parent) {
      invocation = ActiveTemplateInvocation::Create(template_element, parent);
      remove_scope.SetActiveTemplate(invocation, parent, start_marker);
    }
    break;
  }

  if (!parent) {
    return nullptr;
  }

  return MakeGarbageCollected<Patch>(
      base::PassKey<Patch>(), parent, start_marker, end_marker, had_end_marker,
      template_element, invocation, parent_sanitizer, content_policy);
}

void Patch::Apply(HTMLConstructionSite::InsertionLocation& location) {
  location.parent = parent_;
  location.next_child = end_marker_ && end_marker_->parentNode() == parent_
                            ? end_marker_
                            : nullptr;
}

void Patch::DidFinishParsingChildren(HTMLTemplateElement* template_element) {
  if (!loader_) {
    Finalize(template_element);
  }
}

void Patch::DidRemoveTemplateElement() {
  if (loader_) {
    loader_->Cancel();
  }
}

Patch::Patch(base::PassKey<Patch> key,
             ContainerNode* parent,
             Node* start_marker,
             Node* end_marker,
             bool had_end_marker,
             HTMLTemplateElement* template_element,
             ActiveTemplateInvocation* invocation,
             StreamingSanitizer* parent_sanitizer,
             ParserContentPolicy content_policy)
    : parent_(parent),
      start_marker_(start_marker),
      end_marker_(end_marker),
      invocation_(invocation),
      had_end_marker_(had_end_marker),
      was_connected_(parent && parent->isConnected()) {
  const AtomicString& src_attr =
      (template_element && RuntimeEnabledFeatures::DeclarativeFragmentEnabled())
          ? template_element->FastGetAttribute(html_names::kSrcAttr)
          : g_null_atom;
  if (!src_attr.empty()) {
    StreamingSanitizer* sanitizer =
        EqualIgnoringAsciiCase(
            template_element->FastGetAttribute(html_names::kSanitizeAttr),
            "unsafe")
            ? parent_sanitizer
            : StreamingSanitizer::SafeFor(parent_sanitizer);
    // A safe sanitizer strips scripts anyway, so there is no need to mark them
    // as parser-inserted. Never upgrade a policy that disallows scripting.
    if (sanitizer && sanitizer->IsSafe() &&
        ScriptingContentIsAllowed(content_policy)) {
      content_policy = kAllowScriptingContent;
    }
    loader_ = MakeGarbageCollected<ExternalPatchLoader>(
        this, template_element, src_attr, sanitizer, content_policy);
  }
}

void Patch::Finalize(HTMLTemplateElement* template_element) {
  const bool closed_normally =
      !template_element || template_element->ClosedNormally();
  // `end_marker_` is never cleared, so it is set whenever `had_end_marker_` is.
  DCHECK(!had_end_marker_ || end_marker_);
  const bool marker_displaced =
      had_end_marker_ && end_marker_->parentNode() != parent_;
  const bool target_detached =
      was_connected_ && (!parent_ || !parent_->isConnected());
  const V8MutationRecordActiveTemplateStatus::Enum end_status =
      (closed_normally && !marker_displaced && !target_detached)
          ? V8MutationRecordActiveTemplateStatus::Enum::kEnd
          : V8MutationRecordActiveTemplateStatus::Enum::kAbort;

  bool emitted_end_record_on_parent = false;
  if (start_marker_) {
    if (ContainerNode* start_parent = start_marker_->parentNode()) {
      if (invocation_ && start_parent == parent_) {
        ActiveTemplateMutationScope template_scope(end_status, invocation_);
        start_parent->ParserRemoveChild(*start_marker_);
        emitted_end_record_on_parent = true;
      } else {
        start_parent->ParserRemoveChild(*start_marker_);
      }
    }
  }
  if (end_marker_ && end_marker_ != start_marker_) {
    if (ContainerNode* end_parent = end_marker_->parentNode()) {
      if (invocation_ && end_parent == parent_) {
        ActiveTemplateMutationScope template_scope(end_status, invocation_);
        end_parent->ParserRemoveChild(*end_marker_);
        emitted_end_record_on_parent = true;
      } else {
        end_parent->ParserRemoveChild(*end_marker_);
      }
    }
  }

  if (!emitted_end_record_on_parent && parent_ && invocation_) {
    MutationObserverInterestGroup::EnqueueActiveTemplateRecord(
        *parent_, end_status, invocation_);
  }

  if (template_element && template_element->parentNode()) {
    template_element->parentNode()->ParserRemoveChild(*template_element);
  }

  // In normal parsing, positional style invalidation and other effects happen
  // when finished parsing. When patching, we need to run the same logic when
  // the patch finalizes.
  if (Element* element = DynamicTo<Element>(*parent_)) {
    element->DidFinishParsingChildren();
  }
}

void Patch::Trace(Visitor* visitor) const {
  visitor->Trace(parent_);
  visitor->Trace(start_marker_);
  visitor->Trace(end_marker_);
  visitor->Trace(invocation_);
  visitor->Trace(loader_);
}

}  // namespace blink
