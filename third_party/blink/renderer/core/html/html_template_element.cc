/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/html_template_element.h"

#include <cstddef>

#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_cloning_data.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/template_content_document_fragment.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

HTMLTemplateElement::HTMLTemplateElement(Document& document)
    : HTMLElement(html_names::kTemplateTag, document) {
  UseCounter::Count(document, WebFeature::kHTMLTemplateElement);
}

HTMLTemplateElement::~HTMLTemplateElement() = default;

DocumentFragment* HTMLTemplateElement::content() const {
  CHECK(!override_insertion_target_);
  if (!content_ && GetExecutionContext()) {
    content_ = MakeGarbageCollected<TemplateContentDocumentFragment>(
        GetDocument().EnsureTemplateDocument(),
        const_cast<HTMLTemplateElement*>(this));
  }

  return content_.Get();
}

// https://html.spec.whatwg.org/C/#the-template-element:concept-node-clone-ext
void HTMLTemplateElement::CloneNonAttributePropertiesFrom(
    const Element& source,
    NodeCloningData& data) {
  if (!data.Has(CloneOption::kIncludeDescendants) || !GetExecutionContext()) {
    return;
  }
  auto& html_template_element = To<HTMLTemplateElement>(source);
  if (html_template_element.content()) {
    content()->CloneChildNodesFrom(*html_template_element.content(), data,
                                   /*fallback_registry*/ nullptr);
  }
}

void HTMLTemplateElement::DidMoveToNewDocument(Document& old_document) {
  HTMLElement::DidMoveToNewDocument(old_document);
  if (!content_ || !GetExecutionContext()) {
    return;
  }
  GetDocument().EnsureTemplateDocument().AdoptIfNeeded(*content_);
}

void HTMLTemplateElement::Trace(Visitor* visitor) const {
  visitor->Trace(content_);
  visitor->Trace(override_insertion_target_);
  visitor->Trace(insertion_start_marker_);
  visitor->Trace(insertion_end_marker_);
  HTMLElement::Trace(visitor);
}

bool HTMLTemplateElement::BeginPatch(ContainerNode& node) {
  if (!RuntimeEnabledFeatures::DocumentPatchingEnabled()) {
    return false;
  }

  const AtomicString& for_attr = FastGetAttribute(html_names::kForAttr);
  if (for_attr.IsNull() || for_attr.empty()) {
    return false;
  }

  ContainerNode* root = &node;

  if (auto* parent_template = DynamicTo<HTMLTemplateElement>(node)) {
    root = parent_template->InsertionTarget();
  }

  if (root == GetDocument().body()) {
    root = GetDocument().documentElement();
  }

  ContainerNode* marker_host = nullptr;

  if (ShadowRoot* as_shadow = DynamicTo<ShadowRoot>(root)) {
    if (as_shadow->marker().Contains(for_attr)) {
      marker_host = as_shadow;
    }
  }

  if (!marker_host) {
    for (Node& descendant : NodeTraversal::InclusiveDescendantsOf(*root)) {
      Element* element = DynamicTo<Element>(descendant);
      if (!element) {
        continue;
      }
      DOMTokenList* marker_attribute = element->GetMarker();
      if (marker_attribute && marker_attribute->contains(for_attr)) {
        marker_host = element;
        break;
      }
    }
  }

  if (!marker_host) {
    return false;
  }

  CHECK(!insertion_start_marker_);
  CHECK(!insertion_end_marker_);

  for (Node* child = marker_host->firstChild(); child;
       child = child->nextSibling()) {
    if (!child->IsProcessingInstruction()) {
      continue;
    }
    ProcessingInstruction* processing_instruction =
        To<ProcessingInstruction>(child);
    if (processing_instruction->GetAttribute("name") != for_attr) {
      continue;
    }

    const String& current_target = processing_instruction->target();
    if (EqualIgnoringASCIICase(current_target, "marker")) {
      override_insertion_target_ = marker_host;
      insertion_start_marker_ = processing_instruction;
      insertion_end_marker_ = processing_instruction;
      return true;
    }

    if (EqualIgnoringASCIICase(current_target, "end") &&
        insertion_start_marker_) {
      CHECK(override_insertion_target_);
      CHECK(!insertion_end_marker_);
      insertion_end_marker_ = processing_instruction;
      break;
    }

    if (EqualIgnoringASCIICase(current_target, "start") &&
        !insertion_start_marker_) {
      CHECK(!override_insertion_target_);
      override_insertion_target_ = marker_host;
      insertion_start_marker_ = processing_instruction;
    }
  }

  if (!override_insertion_target_) {
    return false;
  }

  CHECK(insertion_start_marker_);

  for (Node* child = insertion_start_marker_->nextSibling();
       child && (child != insertion_end_marker_);
       child = insertion_start_marker_->nextSibling()) {
    marker_host->RemoveChild(child);
  }

  return true;
}

ContainerNode* HTMLTemplateElement::InsertionTarget() const {
  return override_insertion_target_ ? override_insertion_target_.Get()
                                    : content();
}

Node* HTMLTemplateElement::InsertionNextChild() const {
  if (!insertion_end_marker_) {
    return nullptr;
  }

  CHECK(override_insertion_target_);
  return insertion_end_marker_->parentNode() == override_insertion_target_
             ? insertion_end_marker_
             : nullptr;
}

void HTMLTemplateElement::FinishParsingChildren() {
  if (!insertion_start_marker_) {
    return;
  }
  CHECK(override_insertion_target_);
  CHECK(RuntimeEnabledFeatures::DocumentPatchingEnabled());
  ContainerNode* start_parent = insertion_start_marker_->parentNode();
  if (start_parent) {
    start_parent->ParserRemoveChild(*insertion_start_marker_);
  }
  if (insertion_end_marker_ &&
      insertion_end_marker_ != insertion_start_marker_) {
    ContainerNode* end_parent = insertion_end_marker_->parentNode();
    if (end_parent) {
      end_parent->ParserRemoveChild(*insertion_end_marker_);
    }
  }
}

}  // namespace blink
