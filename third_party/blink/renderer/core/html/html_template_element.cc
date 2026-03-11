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

#include "base/memory/stack_allocated.h"
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
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
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

namespace {
struct PatchResult {
  STACK_ALLOCATED();

 public:
  ContainerNode* host = nullptr;
  ProcessingInstruction* start_marker = nullptr;
  ProcessingInstruction* end_marker = nullptr;
};

// TODO(nrosenthal): move this from HTMLTemplateElement to the parser.
std::optional<PatchResult> BeginPatchInternal(ContainerNode* scope,
                                              const AtomicString& for_attr) {
  if (!RuntimeEnabledFeatures::DocumentPatchingEnabled() || for_attr.IsNull() ||
      for_attr.empty()) {
    return std::nullopt;
  }

  AtomicString host_name = for_attr;
  AtomicString marker_name = g_empty_atom;

  auto index_of_hash = for_attr.find('#');
  if (index_of_hash != AtomicString::npos) {
    const String as_string = for_attr.GetString();
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
    if (as_shadow->marker().Contains(host_name)) {
      host = as_shadow;
    }
  }

  if (!host) {
    for (Node& descendant : NodeTraversal::InclusiveDescendantsOf(*scope)) {
      Element* element = DynamicTo<Element>(descendant);
      if (!element) {
        continue;
      }
      DOMTokenList* marker_attribute = element->GetMarker();
      if (marker_attribute && marker_attribute->contains(host_name)) {
        host = element;
        break;
      }
    }
  }

  if (!host) {
    return std::nullopt;
  }

  int marker_depth = 0;
  ProcessingInstruction* start_marker = nullptr;
  ProcessingInstruction* end_marker = nullptr;
  for (Node& child : NodeTraversal::ChildrenOf(*host)) {
    auto* processing_instruction = DynamicTo<ProcessingInstruction>(child);
    if (!processing_instruction) {
      continue;
    }

    AtomicString current_target(processing_instruction->target().LowerASCII());
    DEFINE_STATIC_LOCAL(AtomicString, kNamePseudoAttr, ("name"));
    DEFINE_STATIC_LOCAL(AtomicString, kStart, ("start"));
    DEFINE_STATIC_LOCAL(AtomicString, kEnd, ("end"));
    DEFINE_STATIC_LOCAL(AtomicString, kMarker, ("marker"));

    auto is_name_matching = [&]() {
      return processing_instruction->GetAttributeValue(
                 kNamePseudoAttr, g_empty_atom) == marker_name;
    };

    if (current_target == kMarker && !start_marker && is_name_matching()) {
      return {{.host = host,
               .start_marker = processing_instruction,
               .end_marker = processing_instruction}};
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
    return std::nullopt;
  }

  DCHECK(EqualIgnoringAsciiCase(start_marker->target(), "start"));
  DCHECK(!end_marker || EqualIgnoringAsciiCase(end_marker->target(), "end"));

  for (Node* child = start_marker->nextSibling();
       child && (child != end_marker); child = start_marker->nextSibling()) {
    host->RemoveChild(child);
  }

  return {
      {.host = host, .start_marker = start_marker, .end_marker = end_marker}};
}
}  // namespace

bool HTMLTemplateElement::BeginPatch(ContainerNode& scope) {
  auto result =
      BeginPatchInternal(&scope, FastGetAttribute(html_names::kForAttr));
  if (!result) {
    return false;
  }

  CHECK(RuntimeEnabledFeatures::DocumentPatchingEnabled());
  CHECK(result->host);
  CHECK(result->start_marker);

  override_insertion_target_ = result->host;
  insertion_start_marker_ = result->start_marker;
  insertion_end_marker_ = result->end_marker;
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
