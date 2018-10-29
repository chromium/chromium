/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Ericsson AB. All rights reserved.
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
 */

#include "third_party/blink/renderer/core/html/html_iframe_element.h"

#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_html.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_iframe_element.h"
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/feature_policy/iframe_policy.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_iframe.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

using namespace HTMLNames;

inline HTMLIFrameElement::HTMLIFrameElement(Document& document)
    : HTMLFrameElementBase(iframeTag, document),
      collapsed_by_client_(false),
      sandbox_(HTMLIFrameElementSandbox::Create(this)),
      referrer_policy_(kReferrerPolicyDefault) {}

DEFINE_NODE_FACTORY(HTMLIFrameElement)

void HTMLIFrameElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(sandbox_);
  visitor->Trace(policy_);
  HTMLFrameElementBase::Trace(visitor);
  Supplementable<HTMLIFrameElement>::Trace(visitor);
}

HTMLIFrameElement::~HTMLIFrameElement() = default;

const HashSet<AtomicString>& HTMLIFrameElement::GetCheckedAttributeNames()
    const {
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, attribute_set,
                      ({"src", "srcdoc"}));
  return attribute_set;
}

void HTMLIFrameElement::SetCollapsed(bool collapse) {
  if (collapsed_by_client_ == collapse)
    return;

  collapsed_by_client_ = collapse;

  // This is always called in response to an IPC, so should not happen in the
  // middle of a style recalc.
  DCHECK(!GetDocument().InStyleRecalc());
  LazyReattachIfAttached();
}

DOMTokenList* HTMLIFrameElement::sandbox() const {
  return sandbox_.Get();
}

Policy* HTMLIFrameElement::policy() {
  if (!policy_) {
    policy_ = new IFramePolicy(&GetDocument(), ContainerPolicy(),
                               GetOriginForFeaturePolicy());
  }
  return policy_.Get();
}

bool HTMLIFrameElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == widthAttr || name == heightAttr || name == alignAttr ||
      name == frameborderAttr)
    return true;
  return HTMLFrameElementBase::IsPresentationAttribute(name);
}

void HTMLIFrameElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == widthAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyWidth, value);
  } else if (name == heightAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyHeight, value);
  } else if (name == alignAttr) {
    ApplyAlignmentAttributeToStyle(value, style);
  } else if (name == frameborderAttr) {
    // LocalFrame border doesn't really match the HTML4 spec definition for
    // iframes. It simply adds a presentational hint that the border should be
    // off if set to zero.
    if (!value.ToInt()) {
      // Add a rule that nulls out our border width.
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyBorderWidth, 0,
          CSSPrimitiveValue::UnitType::kPixels);
    }
  } else {
    HTMLFrameElementBase::CollectStyleForPresentationAttribute(name, value,
                                                               style);
  }
}

void HTMLIFrameElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  const AtomicString& value = params.new_value;
  if (name == nameAttr) {
    if (IsInDocumentTree() && GetDocument().IsHTMLDocument()) {
      HTMLDocument& document = ToHTMLDocument(GetDocument());
      document.RemoveNamedItem(name_);
      document.AddNamedItem(value);
    }
    AtomicString old_name = name_;
    name_ = value;
    if (name_ != old_name)
      FrameOwnerPropertiesChanged();
  } else if (name == sandboxAttr) {
    sandbox_->DidUpdateAttributeValue(params.old_value, value);
    String invalid_tokens;
    SetSandboxFlags(value.IsNull() ? kSandboxNone
                                   : ParseSandboxPolicy(sandbox_->TokenSet(),
                                                        invalid_tokens));
    if (!invalid_tokens.IsNull()) {
      GetDocument().AddConsoleMessage(ConsoleMessage::Create(
          kOtherMessageSource, kErrorMessageLevel,
          "Error while parsing the 'sandbox' attribute: " + invalid_tokens));
    }
    UseCounter::Count(GetDocument(), WebFeature::kSandboxViaIFrame);
  } else if (name == referrerpolicyAttr) {
    referrer_policy_ = kReferrerPolicyDefault;
    if (!value.IsNull()) {
      SecurityPolicy::ReferrerPolicyFromString(
          value, kSupportReferrerPolicyLegacyKeywords, &referrer_policy_);
      UseCounter::Count(GetDocument(),
                        WebFeature::kHTMLIFrameElementReferrerPolicyAttribute);
    }
  } else if (name == allowfullscreenAttr) {
    bool old_allow_fullscreen = allow_fullscreen_;
    allow_fullscreen_ = !value.IsNull();
    if (allow_fullscreen_ != old_allow_fullscreen) {
      // TODO(iclelland): Remove this use counter when the allowfullscreen
      // attribute state is snapshotted on document creation. crbug.com/682282
      if (allow_fullscreen_ && ContentFrame()) {
        UseCounter::Count(
            GetDocument(),
            WebFeature::
                kHTMLIFrameElementAllowfullscreenAttributeSetAfterContentLoad);
      }
      FrameOwnerPropertiesChanged();
      UpdateContainerPolicy();
    }
  } else if (name == allowpaymentrequestAttr) {
    bool old_allow_payment_request = allow_payment_request_;
    allow_payment_request_ = !value.IsNull();
    if (allow_payment_request_ != old_allow_payment_request) {
      FrameOwnerPropertiesChanged();
      UpdateContainerPolicy();
    }
  } else if (name == cspAttr) {
    if (!ContentSecurityPolicy::IsValidCSPAttr(
            value.GetString(), GetDocument().RequiredCSP().GetString())) {
      required_csp_ = g_null_atom;
      GetDocument().AddConsoleMessage(ConsoleMessage::Create(
          kOtherMessageSource, kErrorMessageLevel,
          "'csp' attribute is not a valid policy: " + value));
      return;
    }
    if (required_csp_ != value) {
      required_csp_ = value;
      FrameOwnerPropertiesChanged();
    }
  } else if (name == allowAttr) {
    if (allow_ != value) {
      allow_ = value;
      Vector<String> messages;
      UpdateContainerPolicy(&messages);
      if (!messages.IsEmpty()) {
        for (const String& message : messages) {
          GetDocument().AddConsoleMessage(ConsoleMessage::Create(
              kOtherMessageSource, kWarningMessageLevel, message));
        }
      }
      if (!value.IsEmpty()) {
        UseCounter::Count(GetDocument(),
                          WebFeature::kFeaturePolicyAllowAttribute);
      }
    }
  } else {
    // Websites picked up a Chromium article that used this non-specified
    // attribute which ended up changing shape after the specification process.
    // This error message and use count will help developers to move to the
    // proper solution.
    // To avoid polluting the console, this is being recorded only once per
    // page.
    if (name == "gesture" && value == "media" && GetDocument().Loader() &&
        !GetDocument().Loader()->GetUseCounter().HasRecordedMeasurement(
            WebFeature::kHTMLIFrameElementGestureMedia)) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kHTMLIFrameElementGestureMedia);
      GetDocument().AddConsoleMessage(
          ConsoleMessage::Create(kOtherMessageSource, kWarningMessageLevel,
                                 "<iframe gesture=\"media\"> is not supported. "
                                 "Use <iframe allow=\"autoplay\">, "
                                 "https://goo.gl/ximf56"));
    }

    if (name == srcAttr)
      LogUpdateAttributeIfIsolatedWorldAndInDocument("iframe", params);
    HTMLFrameElementBase::ParseAttribute(params);
  }
}

ParsedFeaturePolicy HTMLIFrameElement::ConstructContainerPolicy(
    Vector<String>* messages) const {
  scoped_refptr<const SecurityOrigin> src_origin = GetOriginForFeaturePolicy();
  scoped_refptr<const SecurityOrigin> self_origin =
      GetDocument().GetSecurityOrigin();
  ParsedFeaturePolicy container_policy = ParseFeaturePolicyAttribute(
      allow_, self_origin, src_origin, messages, &GetDocument());

  // If allowfullscreen attribute is present and no fullscreen policy is set,
  // enable the feature for all origins.
  if (AllowFullscreen()) {
    bool policy_changed = AllowFeatureEverywhereIfNotPresent(
        mojom::FeaturePolicyFeature::kFullscreen, container_policy);
    if (!policy_changed && messages) {
      messages->push_back(
          "Allow attribute will take precedence over 'allowfullscreen'.");
    }
  }
  // If the allowpaymentrequest attribute is present and no 'payment' policy is
  // set, enable the feature for all origins.
  if (AllowPaymentRequest()) {
    bool policy_changed = AllowFeatureEverywhereIfNotPresent(
        mojom::FeaturePolicyFeature::kPayment, container_policy);
    if (!policy_changed && messages) {
      messages->push_back(
          "Allow attribute will take precedence over 'allowpaymentrequest'.");
    }
  }

  // Update Policy associated with this iframe, if exists.
  if (policy_)
    policy_->UpdateContainerPolicy(container_policy, src_origin);

  return container_policy;
}

bool HTMLIFrameElement::LayoutObjectIsNeeded(const ComputedStyle& style) const {
  return ContentFrame() && !collapsed_by_client_ &&
         HTMLElement::LayoutObjectIsNeeded(style);
}

LayoutObject* HTMLIFrameElement::CreateLayoutObject(const ComputedStyle&) {
  return new LayoutIFrame(this);
}

Node::InsertionNotificationRequest HTMLIFrameElement::InsertedInto(
    ContainerNode& insertion_point) {
  InsertionNotificationRequest result =
      HTMLFrameElementBase::InsertedInto(insertion_point);

  if (insertion_point.IsInDocumentTree() && GetDocument().IsHTMLDocument()) {
    ToHTMLDocument(GetDocument()).AddNamedItem(name_);

    if (!ContentSecurityPolicy::IsValidCSPAttr(
            required_csp_, GetDocument().RequiredCSP().GetString())) {
      if (!required_csp_.IsEmpty()) {
        GetDocument().AddConsoleMessage(ConsoleMessage::Create(
            kOtherMessageSource, kErrorMessageLevel,
            "'csp' attribute is not a valid policy: " + required_csp_));
      }
      if (required_csp_ != GetDocument().RequiredCSP()) {
        required_csp_ = GetDocument().RequiredCSP();
        FrameOwnerPropertiesChanged();
      }
    }
  }
  LogAddElementIfIsolatedWorldAndInDocument("iframe", srcAttr);
  return result;
}

void HTMLIFrameElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLFrameElementBase::RemovedFrom(insertion_point);
  if (insertion_point.IsInDocumentTree() && GetDocument().IsHTMLDocument())
    ToHTMLDocument(GetDocument()).RemoveNamedItem(name_);
}

bool HTMLIFrameElement::IsInteractiveContent() const {
  return true;
}

ReferrerPolicy HTMLIFrameElement::ReferrerPolicyAttribute() {
  return referrer_policy_;
}

}  // namespace blink
