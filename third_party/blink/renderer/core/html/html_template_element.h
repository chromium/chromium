/*
 * Copyright (c) 2012, Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_TEMPLATE_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_TEMPLATE_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

class DocumentFragment;
class TemplateContentDocumentFragment;

// TODO(crbug.com/1379513, crbug.com/1396384) Only three of these should be
// needed at a time, depending on the state of the StreamingDeclarativeShadowDOM
// feature and whether the `shadowroot` or `shadowrootmode` attribute is used.
// For a given template, either kNone/kOpen/kClosed or
// kNone/kStreamingOpen/kStreamingClosed are used.
enum class DeclarativeShadowRootType {
  kNone,
  kOpen,
  kStreamingOpen,
  kClosed,
  kStreamingClosed,
};

class CORE_EXPORT HTMLTemplateElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLTemplateElement(Document&);
  ~HTMLTemplateElement() override;

  bool HasNonInBodyInsertionMode() const override { return true; }

  void Trace(Visitor*) const override;

  DocumentFragment* content() const;

  // This gives direct access to ContentInternal, and should *only*
  // be used by HTMLConstructionSite.
  DocumentFragment* TemplateContentForHTMLConstructionSite() const {
    if (declarative_shadow_root_) {
      DCHECK(RuntimeEnabledFeatures::StreamingDeclarativeShadowDOMEnabled());
      return declarative_shadow_root_.Get();
    }
    return ContentInternal();
  }

  // TODO(crbug.com/1396384) Eventually remove this.
  bool IsNonStreamingDeclarativeShadowRoot() const;
  // TODO(crbug.com/1396384) Eventually remove this.
  DocumentFragment* DeclarativeShadowContent() const;

  void SetDeclarativeShadowRootType(DeclarativeShadowRootType val) {
    declarative_shadow_root_type_ = val;
  }
  DeclarativeShadowRootType GetDeclarativeShadowRootType() const {
    return declarative_shadow_root_type_;
  }
  bool IsDeclarativeShadowRoot() const {
    return declarative_shadow_root_type_ != DeclarativeShadowRootType::kNone;
  }

  void SetDeclarativeShadowRoot(ShadowRoot& shadow) {
    DCHECK(RuntimeEnabledFeatures::StreamingDeclarativeShadowDOMEnabled());
    DCHECK(declarative_shadow_root_type_ ==
               DeclarativeShadowRootType::kStreamingOpen ||
           declarative_shadow_root_type_ ==
               DeclarativeShadowRootType::kStreamingClosed);
    declarative_shadow_root_ = &shadow;
  }

 private:
  void CloneNonAttributePropertiesFrom(const Element&,
                                       CloneChildrenFlag) override;
  void DidMoveToNewDocument(Document& old_document) override;

  DocumentFragment* ContentInternal() const;

  mutable Member<TemplateContentDocumentFragment> content_;

  Member<ShadowRoot> declarative_shadow_root_;
  DeclarativeShadowRootType declarative_shadow_root_type_;
};

ALWAYS_INLINE bool HTMLTemplateElement::IsNonStreamingDeclarativeShadowRoot()
    const {
  switch (declarative_shadow_root_type_) {
    case DeclarativeShadowRootType::kNone:
      return false;
    case DeclarativeShadowRootType::kOpen:
    case DeclarativeShadowRootType::kClosed:
      DCHECK(!declarative_shadow_root_);
      return true;
    case DeclarativeShadowRootType::kStreamingOpen:
    case DeclarativeShadowRootType::kStreamingClosed:
      DCHECK(RuntimeEnabledFeatures::StreamingDeclarativeShadowDOMEnabled());
      return false;
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_TEMPLATE_ELEMENT_H_
