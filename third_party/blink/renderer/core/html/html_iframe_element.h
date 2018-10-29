/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2004, 2006, 2008, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_IFRAME_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_IFRAME_ELEMENT_H_

#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_iframe_element_sandbox.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {
class Policy;

class CORE_EXPORT HTMLIFrameElement final
    : public HTMLFrameElementBase,
      public Supplementable<HTMLIFrameElement> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(HTMLIFrameElement);

 public:
  DECLARE_NODE_FACTORY(HTMLIFrameElement);
  void Trace(blink::Visitor*) override;
  ~HTMLIFrameElement() override;
  DOMTokenList* sandbox() const;
  // Support JS introspection of frame policy (e.g. feature policy)
  Policy* policy();

  // Returns attributes that should be checked against Trusted Types
  const HashSet<AtomicString>& GetCheckedAttributeNames() const override;

  ParsedFeaturePolicy ConstructContainerPolicy(
      Vector<String>* /* messages */) const override;

  FrameOwnerElementType OwnerType() const final {
    return FrameOwnerElementType::kIframe;
  }

 private:
  explicit HTMLIFrameElement(Document&);

  void SetCollapsed(bool) override;

  void ParseAttribute(const AttributeModificationParams&) override;
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void CollectStyleForPresentationAttribute(
      const QualifiedName&,
      const AtomicString&,
      MutableCSSPropertyValueSet*) override;

  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;

  bool LayoutObjectIsNeeded(const ComputedStyle&) const override;
  LayoutObject* CreateLayoutObject(const ComputedStyle&) override;

  bool IsInteractiveContent() const override;

  ReferrerPolicy ReferrerPolicyAttribute() override;

  // FrameOwner overrides:
  bool AllowFullscreen() const override { return allow_fullscreen_; }
  bool AllowPaymentRequest() const override { return allow_payment_request_; }
  AtomicString RequiredCsp() const override { return required_csp_; }

  AtomicString name_;
  AtomicString required_csp_;
  AtomicString allow_;
  bool allow_fullscreen_;
  bool allow_payment_request_;
  bool collapsed_by_client_;
  Member<HTMLIFrameElementSandbox> sandbox_;
  Member<Policy> policy_;

  ReferrerPolicy referrer_policy_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_IFRAME_ELEMENT_H_
