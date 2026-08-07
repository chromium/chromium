// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_ANCHOR_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_ANCHOR_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/rel_list.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"
#include "third_party/blink/renderer/core/url/dom_origin_utils.h"
#include "third_party/blink/renderer/core/url/url_utils.h"

namespace blink {

class MouseEvent;

class CORE_EXPORT MathMLAnchorElement : public MathMLElement,
                                        public UrlUtils,
                                        public DOMOriginUtils {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit MathMLAnchorElement(Document&);

  ElementType GetElementType() const final {
    return ElementType::kMathMLAnchorElement;
  }

  bool IsGroupingElement() const override { return true; }

  uint32_t GetLinkRelations() const { return link_relations_; }

  void Trace(Visitor*) const override;

  bool HasActivationBehavior() const override;
  void DefaultEventHandler(Event&) override;
  bool IsInteractiveContent() const { return true; }

  // DOMOriginUtils overrides:
  DOMOrigin* GetDOMOrigin(LocalDOMWindow*) const final;
  // UrlUtils overrides:
  KURL Url() const override;
  void SetUrl(const KURL&) override;

  String Input() const override;

  void ParseAttribute(const AttributeModificationParams&) override;
  bool IsURLAttribute(const Attribute&) const override;

  LayoutObject* CreateLayoutObject(const ComputedStyle&) override;
  FocusableState SupportsFocus(UpdateBehavior) const override;
  int DefaultTabIndex() const override;

  RelList* relList() const { return rel_list_.Get(); }

 private:
  void HandleClick(MouseEvent&);

  bool IsLiveLink() const override { return IsLink(); }

  Member<RelList> rel_list_;
  unsigned link_relations_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_ANCHOR_ELEMENT_H_
