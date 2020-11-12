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

#include "base/metrics/histogram_macros.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/trust_tokens.mojom-blink.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_iframe_element.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/feature_policy/document_policy_parser.h"
#include "third_party/blink/renderer/core/feature_policy/feature_policy_parser.h"
#include "third_party/blink/renderer/core/feature_policy/iframe_policy.h"
#include "third_party/blink/renderer/core/fetch/trust_token_issuance_authorization.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/sandbox_flags.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/trust_token_attribute_parsing.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_iframe.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

HTMLIFrameElement::HTMLIFrameElement(Document& document)
    : HTMLFrameElementBase(html_names::kIFrameTag, document),
      collapsed_by_client_(false),
      sandbox_(MakeGarbageCollected<HTMLIFrameElementSandbox>(this)),
      referrer_policy_(network::mojom::ReferrerPolicy::kDefault) {}

void HTMLIFrameElement::Trace(Visitor* visitor) const {
  visitor->Trace(sandbox_);
  visitor->Trace(policy_);
  HTMLFrameElementBase::Trace(visitor);
  Supplementable<HTMLIFrameElement>::Trace(visitor);
}

HTMLIFrameElement::~HTMLIFrameElement() = default;

const AttrNameToTrustedType& HTMLIFrameElement::GetCheckedAttributeTypes()
    const {
  DEFINE_STATIC_LOCAL(AttrNameToTrustedType, attribute_map,
                      ({{"srcdoc", SpecificTrustedType::kHTML}}));
  return attribute_map;
}

void HTMLIFrameElement::SetCollapsed(bool collapse) {
  if (collapsed_by_client_ == collapse)
    return;

  collapsed_by_client_ = collapse;

  // This is always called in response to an IPC, so should not happen in the
  // middle of a style recalc.
  DCHECK(!GetDocument().InStyleRecalc());

  // Trigger style recalc to trigger layout tree re-attachment.
  SetNeedsStyleRecalc(kLocalStyleChange, StyleChangeReasonForTracing::Create(
                                             style_change_reason::kFrame));
}

DOMTokenList* HTMLIFrameElement::sandbox() const {
  return sandbox_.Get();
}

DOMFeaturePolicy* HTMLIFrameElement::featurePolicy() {
  if (!policy_ && GetExecutionContext()) {
    policy_ = MakeGarbageCollected<IFramePolicy>(
        GetExecutionContext(), GetFramePolicy().container_policy,
        GetOriginForFeaturePolicy());
  }
  return policy_.Get();
}

bool HTMLIFrameElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == html_names::kWidthAttr || name == html_names::kHeightAttr ||
      name == html_names::kAlignAttr || name == html_names::kFrameborderAttr)
    return true;
  return HTMLFrameElementBase::IsPresentationAttribute(name);
}

void HTMLIFrameElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kWidthAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kWidth, value);
  } else if (name == html_names::kHeightAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kHeight, value);
  } else if (name == html_names::kAlignAttr) {
    ApplyAlignmentAttributeToStyle(value, style);
  } else if (name == html_names::kFrameborderAttr) {
    // LocalFrame border doesn't really match the HTML4 spec definition for
    // iframes. It simply adds a presentational hint that the border should be
    // off if set to zero.
    if (!value.ToInt()) {
      // Add a rule that nulls out our border width.
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kBorderWidth, 0,
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
  if (name == html_names::kNameAttr) {
    auto* document = DynamicTo<HTMLDocument>(GetDocument());
    if (document && IsInDocumentTree()) {
      document->RemoveNamedItem(name_);
      document->AddNamedItem(value);
    }
    AtomicString old_name = name_;
    name_ = value;
    if (name_ != old_name)
      FrameOwnerPropertiesChanged();
  } else if (name == html_names::kSandboxAttr) {
    sandbox_->DidUpdateAttributeValue(params.old_value, value);
    bool feature_policy_for_sandbox =
        RuntimeEnabledFeatures::FeaturePolicyForSandboxEnabled();

    network::mojom::blink::WebSandboxFlags current_flags =
        network::mojom::blink::WebSandboxFlags::kNone;
    if (!value.IsNull()) {
      using network::mojom::blink::WebSandboxFlags;
      WebSandboxFlags ignored_flags =
          !RuntimeEnabledFeatures::StorageAccessAPIEnabled()
              ? WebSandboxFlags::kStorageAccessByUserActivation
              : WebSandboxFlags::kNone;

      auto parsed = network::ParseWebSandboxPolicy(sandbox_->value().Utf8(),
                                                   ignored_flags);
      current_flags = parsed.flags;
      if (!parsed.error_message.empty()) {
        GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kError,
            WebString::FromUTF8(
                "Error while parsing the 'sandbox' attribute: " +
                parsed.error_message)));
      }
    }
    SetAllowedToDownload(
        (current_flags & network::mojom::blink::WebSandboxFlags::kDownloads) ==
        network::mojom::blink::WebSandboxFlags::kNone);
    // With FeaturePolicyForSandbox, sandbox flags are represented as part of
    // the container policies. However, not all sandbox flags are yet converted
    // and for now the residue will stay around in the stored flags.
    // (see https://crbug.com/812381).
    network::mojom::blink::WebSandboxFlags sandbox_to_set = current_flags;
    sandbox_flags_converted_to_feature_policies_ =
        network::mojom::blink::WebSandboxFlags::kNone;
    if (feature_policy_for_sandbox &&
        current_flags != network::mojom::blink::WebSandboxFlags::kNone) {
      // Residue sandbox which will not be mapped to feature policies.
      sandbox_to_set =
          GetSandboxFlagsNotImplementedAsFeaturePolicy(current_flags);
      // The part of sandbox which will be mapped to feature policies.
      sandbox_flags_converted_to_feature_policies_ =
          current_flags & ~sandbox_to_set;
    }
    SetSandboxFlags(sandbox_to_set);
    if (RuntimeEnabledFeatures::FeaturePolicyForSandboxEnabled())
      UpdateContainerPolicy();

    UseCounter::Count(GetDocument(), WebFeature::kSandboxViaIFrame);
  } else if (name == html_names::kReferrerpolicyAttr) {
    referrer_policy_ = network::mojom::ReferrerPolicy::kDefault;
    if (!value.IsNull()) {
      SecurityPolicy::ReferrerPolicyFromString(
          value, kSupportReferrerPolicyLegacyKeywords, &referrer_policy_);
      UseCounter::Count(GetDocument(),
                        WebFeature::kHTMLIFrameElementReferrerPolicyAttribute);
    }
  } else if (name == html_names::kAllowfullscreenAttr) {
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
  } else if (name == html_names::kAllowpaymentrequestAttr) {
    bool old_allow_payment_request = allow_payment_request_;
    allow_payment_request_ = !value.IsNull();
    if (allow_payment_request_ != old_allow_payment_request) {
      FrameOwnerPropertiesChanged();
      UpdateContainerPolicy();
    }
  } else if (name == html_names::kCspAttr) {
    if (base::FeatureList::IsEnabled(network::features::kOutOfBlinkCSPEE)) {
      if (value.Contains('\n') || value.Contains('\r') || value.Contains(',')) {
        required_csp_ = g_null_atom;
        GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kError,
            "'csp' attribute is invalid: " + value));
        return;
      }
      if (required_csp_ != value) {
        required_csp_ = value;
        CSPAttributeChanged();
        UseCounter::Count(GetDocument(), WebFeature::kIFrameCSPAttribute);
      }
    } else {
      if (!ContentSecurityPolicy::IsValidCSPAttr(
              value.GetString(), GetDocument().RequiredCSP().GetString())) {
        required_csp_ = g_null_atom;
        GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kError,
            "'csp' attribute is not a valid policy: " + value));
        return;
      }
      if (required_csp_ != value) {
        required_csp_ = value;
        FrameOwnerPropertiesChanged();
        UseCounter::Count(GetDocument(), WebFeature::kIFrameCSPAttribute);
      }
    }
  } else if (name == html_names::kAllowAttr) {
    if (allow_ != value) {
      allow_ = value;
      UpdateContainerPolicy();
      if (!value.IsEmpty()) {
        UseCounter::Count(GetDocument(),
                          WebFeature::kFeaturePolicyAllowAttribute);
      }
      if (value.Contains(',')) {
        Deprecation::CountDeprecation(
            GetDocument().GetExecutionContext(),
            WebFeature::kCommaSeparatorInAllowAttribute);
      }
    }
  } else if (name == html_names::kDisallowdocumentaccessAttr &&
             RuntimeEnabledFeatures::DisallowDocumentAccessEnabled()) {
    UseCounter::Count(GetDocument(), WebFeature::kDisallowDocumentAccess);
    SetDisallowDocumentAccesss(!value.IsNull());
    // We don't need to call tell the client frame properties
    // changed since this attribute only stays inside the renderer.
  } else if (name == html_names::kPolicyAttr) {
    if (required_policy_ != value) {
      required_policy_ = value;
      UpdateRequiredPolicy();
    }
  } else if (name == html_names::kTrusttokenAttr) {
    trust_token_ = value;
  } else {
    // Websites picked up a Chromium article that used this non-specified
    // attribute which ended up changing shape after the specification process.
    // This error message and use count will help developers to move to the
    // proper solution.
    // To avoid polluting the console, this is being recorded only once per
    // page.
    if (name == "gesture" && value == "media" && GetDocument().Loader() &&
        !GetDocument().Loader()->GetUseCounterHelper().HasRecordedMeasurement(
            WebFeature::kHTMLIFrameElementGestureMedia)) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kHTMLIFrameElementGestureMedia);
      GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::ConsoleMessageSource::kOther,
          mojom::ConsoleMessageLevel::kWarning,
          "<iframe gesture=\"media\"> is not supported. "
          "Use <iframe allow=\"autoplay\">, "
          "https://goo.gl/ximf56"));
    }

    if (name == html_names::kSrcAttr)
      LogUpdateAttributeIfIsolatedWorldAndInDocument("iframe", params);
    HTMLFrameElementBase::ParseAttribute(params);
  }
}

DocumentPolicyFeatureState HTMLIFrameElement::ConstructRequiredPolicy() const {
  if (!RuntimeEnabledFeatures::DocumentPolicyNegotiationEnabled(
          GetExecutionContext()))
    return {};

  if (!required_policy_.IsEmpty()) {
    UseCounter::Count(
        GetDocument(),
        mojom::blink::WebFeature::kDocumentPolicyIframePolicyAttribute);
  }

  PolicyParserMessageBuffer logger;
  DocumentPolicy::ParsedDocumentPolicy new_required_policy =
      DocumentPolicyParser::Parse(required_policy_, logger)
          .value_or(DocumentPolicy::ParsedDocumentPolicy{});

  for (const auto& message : logger.GetMessages()) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther, message.level,
        message.content));
  }

  if (!new_required_policy.endpoint_map.empty()) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "Iframe policy attribute cannot specify reporting endpoint."));
  }

  for (const auto& policy_entry : new_required_policy.feature_state) {
    mojom::blink::DocumentPolicyFeature feature = policy_entry.first;
    if (!GetDocument().DocumentPolicyFeatureObserved(feature)) {
      UMA_HISTOGRAM_ENUMERATION(
          "Blink.UseCounter.DocumentPolicy.PolicyAttribute", feature);
    }
  }
  return new_required_policy.feature_state;
}

ParsedFeaturePolicy HTMLIFrameElement::ConstructContainerPolicy() const {
  if (!GetExecutionContext())
    return ParsedFeaturePolicy();

  scoped_refptr<const SecurityOrigin> src_origin = GetOriginForFeaturePolicy();
  scoped_refptr<const SecurityOrigin> self_origin =
      GetExecutionContext()->GetSecurityOrigin();

  PolicyParserMessageBuffer logger;

  // Start with the allow attribute
  ParsedFeaturePolicy container_policy = FeaturePolicyParser::ParseAttribute(
      allow_, self_origin, src_origin, logger, GetExecutionContext());

  // Next, process sandbox flags. These all only take effect if a corresponding
  // policy does *not* exist in the allow attribute's value.
  if (RuntimeEnabledFeatures::FeaturePolicyForSandboxEnabled()) {
    // If the frame is sandboxed at all, then warn if feature policy attributes
    // will override the sandbox attributes.
    if ((sandbox_flags_converted_to_feature_policies_ &
         network::mojom::blink::WebSandboxFlags::kNavigation) !=
        network::mojom::blink::WebSandboxFlags::kNone) {
      for (const auto& pair : SandboxFlagsWithFeaturePolicies()) {
        if ((sandbox_flags_converted_to_feature_policies_ & pair.first) !=
                network::mojom::blink::WebSandboxFlags::kNone &&
            IsFeatureDeclared(pair.second, container_policy)) {
          logger.Warn(String::Format(
              "Allow and Sandbox attributes both mention '%s'. Allow will take "
              "precedence.",
              GetNameForFeature(pair.second).Utf8().c_str()));
        }
      }
    }
    ApplySandboxFlagsToParsedFeaturePolicy(
        sandbox_flags_converted_to_feature_policies_, container_policy);
  }

  // Finally, process the allow* attribuets. Like sandbox attributes, they only
  // take effect if the corresponding feature is not present in the allow
  // attribute's value.

  // If allowfullscreen attribute is present and no fullscreen policy is set,
  // enable the feature for all origins.
  if (AllowFullscreen()) {
    bool policy_changed = AllowFeatureEverywhereIfNotPresent(
        mojom::blink::FeaturePolicyFeature::kFullscreen, container_policy);
    if (!policy_changed) {
      logger.Warn(
          "Allow attribute will take precedence over 'allowfullscreen'.");
    }
  }
  // If the allowpaymentrequest attribute is present and no 'payment' policy is
  // set, enable the feature for all origins.
  if (AllowPaymentRequest()) {
    bool policy_changed = AllowFeatureEverywhereIfNotPresent(
        mojom::blink::FeaturePolicyFeature::kPayment, container_policy);
    if (!policy_changed) {
      logger.Warn(
          "Allow attribute will take precedence over 'allowpaymentrequest'.");
    }
  }

  // Update the JavaScript policy object associated with this iframe, if it
  // exists.
  if (policy_)
    policy_->UpdateContainerPolicy(container_policy, src_origin);

  for (const auto& message : logger.GetMessages()) {
    GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther, message.level,
            message.content),
        /* discard_duplicates */ true);
  }

  return container_policy;
}

bool HTMLIFrameElement::LayoutObjectIsNeeded(const ComputedStyle& style) const {
  return ContentFrame() && !collapsed_by_client_ &&
         HTMLElement::LayoutObjectIsNeeded(style);
}

LayoutObject* HTMLIFrameElement::CreateLayoutObject(const ComputedStyle&,
                                                    LegacyLayout) {
  return new LayoutIFrame(this);
}

Node::InsertionNotificationRequest HTMLIFrameElement::InsertedInto(
    ContainerNode& insertion_point) {
  InsertionNotificationRequest result =
      HTMLFrameElementBase::InsertedInto(insertion_point);

  auto* html_doc = DynamicTo<HTMLDocument>(GetDocument());
  if (html_doc && insertion_point.IsInDocumentTree()) {
    html_doc->AddNamedItem(name_);
    if (!base::FeatureList::IsEnabled(network::features::kOutOfBlinkCSPEE) &&
        !ContentSecurityPolicy::IsValidCSPAttr(
            required_csp_, GetDocument().RequiredCSP().GetString())) {
      if (!required_csp_.IsEmpty()) {
        GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kOther,
            mojom::ConsoleMessageLevel::kError,
            "'csp' attribute is not a valid policy: " + required_csp_));
      }
      if (required_csp_ != GetDocument().RequiredCSP()) {
        required_csp_ = GetDocument().RequiredCSP();
        FrameOwnerPropertiesChanged();
        UseCounter::Count(GetDocument(), WebFeature::kIFrameCSPAttribute);
      }
    }
  }
  LogAddElementIfIsolatedWorldAndInDocument("iframe", html_names::kSrcAttr);
  return result;
}

void HTMLIFrameElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLFrameElementBase::RemovedFrom(insertion_point);
  auto* html_doc = DynamicTo<HTMLDocument>(GetDocument());
  if (html_doc && insertion_point.IsInDocumentTree())
    html_doc->RemoveNamedItem(name_);
}

bool HTMLIFrameElement::IsInteractiveContent() const {
  return true;
}

network::mojom::ReferrerPolicy HTMLIFrameElement::ReferrerPolicyAttribute() {
  return referrer_policy_;
}

network::mojom::blink::TrustTokenParamsPtr
HTMLIFrameElement::ConstructTrustTokenParams() const {
  if (!trust_token_)
    return nullptr;

  JSONParseError parse_error;
  std::unique_ptr<JSONValue> parsed_attribute =
      ParseJSON(trust_token_, &parse_error);
  if (!parsed_attribute) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kError,
        "iframe trusttoken attribute was invalid JSON: " + parse_error.message +
            String::Format(" (line %d, col %d)", parse_error.line,
                           parse_error.column)));
    return nullptr;
  }

  network::mojom::blink::TrustTokenParamsPtr parsed_params =
      internal::TrustTokenParamsFromJson(std::move(parsed_attribute));
  if (!parsed_params) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kError,
        "Couldn't parse iframe trusttoken attribute (was it missing a "
        "field?)"));
    return nullptr;
  }

  // Trust token redemption and signing (but not issuance) require that the
  // trust-token-redemption feature policy be present.
  bool operation_requires_feature_policy =
      parsed_params->type ==
          network::mojom::blink::TrustTokenOperationType::kRedemption ||
      parsed_params->type ==
          network::mojom::blink::TrustTokenOperationType::kSigning;

  if (operation_requires_feature_policy &&
      (!GetExecutionContext()->IsFeatureEnabled(
          mojom::blink::FeaturePolicyFeature::kTrustTokenRedemption))) {
    GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kError,
            "Trust Tokens: Attempted redemption or signing without the "
            "trust-token-redemption Feature Policy feature present."));
    return nullptr;
  }

  if (parsed_params->type ==
          network::mojom::blink::TrustTokenOperationType::kIssuance &&
      !IsTrustTokenIssuanceAvailableInExecutionContext(
          *GetExecutionContext())) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kError,
        "Trust Tokens issuance is disabled except in "
        "contexts with the TrustTokens Origin Trial enabled."));
    return nullptr;
  }

  return parsed_params;
}

}  // namespace blink
