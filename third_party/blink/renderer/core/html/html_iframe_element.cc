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
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_iframe_element.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/client_hints_util.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/trust_token_attribute_parsing.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_iframe.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/permissions_policy/document_policy_parser.h"
#include "third_party/blink/renderer/core/permissions_policy/iframe_policy.h"
#include "third_party/blink/renderer/core/permissions_policy/permissions_policy_parser.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {
// Cut down |value| if too long . This is used to convert the HTML attributes
// to report to the browser.
String ConvertToReportValue(const AtomicString& value) {
  if (value.IsNull()) {
    // If the value is null, report null so that it can be distinguishable from
    // an empty string.
    return String();
  }
  static constexpr size_t kMaxLengthToReport = 1024;
  return value.GetString().Left(kMaxLengthToReport);
}

}  // namespace

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
  if (collapsed_by_client_ == collapse) {
    return;
  }

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
        GetOriginForPermissionsPolicy());
  }
  return policy_.Get();
}

bool HTMLIFrameElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == html_names::kWidthAttr || name == html_names::kHeightAttr ||
      name == html_names::kAlignAttr || name == html_names::kFrameborderAttr) {
    return true;
  }
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
      for (CSSPropertyID property_id :
           {CSSPropertyID::kBorderTopWidth, CSSPropertyID::kBorderBottomWidth,
            CSSPropertyID::kBorderLeftWidth,
            CSSPropertyID::kBorderRightWidth}) {
        AddPropertyToPresentationAttributeStyle(
            style, property_id, 0, CSSPrimitiveValue::UnitType::kPixels);
      }
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
  // This is only set to true for values needed by the browser.
  bool should_call_did_change_attributes = false;
  if (name == html_names::kNameAttr) {
    auto* document = DynamicTo<HTMLDocument>(GetDocument());
    if (document && IsInDocumentTree()) {
      document->RemoveNamedItem(name_);
      document->AddNamedItem(value);
    }
    AtomicString old_name = name_;
    name_ = value;
    if (name_ != old_name) {
      FrameOwnerPropertiesChanged();
      should_call_did_change_attributes = true;
    }
    if (name_.Contains('\n')) {
      UseCounter::Count(GetDocument(), WebFeature::kFrameNameContainsNewline);
    }
    if (name_.Contains('<')) {
      UseCounter::Count(GetDocument(), WebFeature::kFrameNameContainsBrace);
    }
    if (name_.Contains('\n') && name_.Contains('<')) {
      UseCounter::Count(GetDocument(), WebFeature::kDanglingMarkupInWindowName);
      if (!name_.EndsWith('>')) {
        UseCounter::Count(GetDocument(),
                          WebFeature::kDanglingMarkupInWindowNameNotEndsWithGT);
        if (!name_.EndsWith('\n')) {
          UseCounter::Count(
              GetDocument(),
              WebFeature::kDanglingMarkupInWindowNameNotEndsWithNewLineOrGT);
        }
      }
    }
  } else if (name == html_names::kSandboxAttr) {
    sandbox_->DidUpdateAttributeValue(params.old_value, value);

    network::mojom::blink::WebSandboxFlags current_flags =
        network::mojom::blink::WebSandboxFlags::kNone;
    if (!value.IsNull()) {
      using network::mojom::blink::WebSandboxFlags;
      auto parsed = network::ParseWebSandboxPolicy(sandbox_->value().Utf8(),
                                                   WebSandboxFlags::kNone);
      current_flags = parsed.flags;
      if (!parsed.error_message.empty()) {
        GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kError,
            "Error while parsing the 'sandbox' attribute: " +
                String::FromUTF8(parsed.error_message)));
      }
    }
    SetSandboxFlags(current_flags);
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
    static const size_t kMaxLengthCSPAttribute = 4096;
    if (value && (value.Contains('\n') || value.Contains('\r') ||
                  !MatchesTheSerializedCSPGrammar(value.GetString()))) {
      // TODO(antoniosartori): It would be safer to block loading iframes with
      // invalid 'csp' attribute.
      required_csp_ = g_null_atom;
      GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kOther,
          mojom::blink::ConsoleMessageLevel::kError,
          "'csp' attribute is invalid: " + value));
    } else if (value && value.length() > kMaxLengthCSPAttribute) {
      // TODO(antoniosartori): It would be safer to block loading iframes with
      // invalid 'csp' attribute.
      required_csp_ = g_null_atom;
      GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kOther,
          mojom::blink::ConsoleMessageLevel::kError,
          String::Format("'csp' attribute too long. The max length for the "
                         "'csp' attribute is %zu bytes.",
                         kMaxLengthCSPAttribute)));
    } else if (required_csp_ != value) {
      required_csp_ = value;
      should_call_did_change_attributes = true;
      UseCounter::Count(GetDocument(), WebFeature::kIFrameCSPAttribute);
    }
  } else if (name == html_names::kBrowsingtopicsAttr) {
    if (GetExecutionContext() &&
        RuntimeEnabledFeatures::TopicsAPIEnabled(GetExecutionContext()) &&
        GetExecutionContext()->IsSecureContext()) {
      bool old_browsing_topics = !params.old_value.IsNull();
      bool new_browsing_topics = !params.new_value.IsNull();

      if (new_browsing_topics) {
        UseCounter::Count(GetDocument(),
                          WebFeature::kIframeBrowsingTopicsAttribute);
        UseCounter::Count(GetDocument(), WebFeature::kTopicsAPIAll);
      }

      if (new_browsing_topics != old_browsing_topics) {
        should_call_did_change_attributes = true;
      }
    }
  } else if (name == html_names::kAdauctionheadersAttr &&
             GetExecutionContext()) {
    if (!GetExecutionContext()->IsSecureContext()) {
      GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kOther,
          mojom::blink::ConsoleMessageLevel::kError,
          String("adAuctionHeaders: Protected Audience APIs "
                 "are only available in secure contexts.")));
    } else {
      if (params.new_value.IsNull() != params.old_value.IsNull()) {
        should_call_did_change_attributes = true;
      }
      if (!params.new_value.IsNull()) {
        UseCounter::Count(GetDocument(),
                          WebFeature::kSharedStorageAPI_Iframe_Attribute);
      }
    }
  } else if (name == html_names::kSharedstoragewritableAttr &&
             GetExecutionContext() &&
             RuntimeEnabledFeatures::SharedStorageAPIM118Enabled(
                 GetExecutionContext())) {
    if (!GetExecutionContext()->IsSecureContext()) {
      GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kOther,
          mojom::blink::ConsoleMessageLevel::kError,
          String("sharedStorageWritable: sharedStorage operations "
                 "are only available in secure contexts.")));
    } else {
      if (params.new_value.IsNull() != params.old_value.IsNull()) {
        should_call_did_change_attributes = true;
      }
      if (!params.new_value.IsNull()) {
        UseCounter::Count(GetDocument(),
                          WebFeature::kSharedStorageAPI_Iframe_Attribute);
      }
    }
  } else if (name == html_names::kCredentiallessAttr &&
             RuntimeEnabledFeatures::AnonymousIframeEnabled()) {
    bool new_value = !value.IsNull();
    if (credentialless_ != new_value) {
      credentialless_ = new_value;
      should_call_did_change_attributes = true;
    }
  } else if (name == html_names::kAllowAttr) {
    if (allow_ != value) {
      allow_ = value;
      UpdateContainerPolicy();
      if (!value.empty()) {
        UseCounter::Count(GetDocument(),
                          WebFeature::kFeaturePolicyAllowAttribute);
      }
    }
  } else if (name == html_names::kPolicyAttr) {
    if (required_policy_ != value) {
      required_policy_ = value;
      UpdateRequiredPolicy();
    }
  } else if (name == html_names::kPrivatetokenAttr) {
    UseCounter::Count(GetDocument(), WebFeature::kTrustTokenIframe);
    trust_token_ = value;
  } else {
    // Websites picked up a Chromium article that used this non-specified
    // attribute which ended up changing shape after the specification process.
    // This error message and use count will help developers to move to the
    // proper solution.
    // To avoid polluting the console, this is being recorded only once per
    // page.
    if (name == AtomicString("gesture") && value == AtomicString("media") &&
        GetDocument().Loader() &&
        !GetDocument().Loader()->GetUseCounter().IsCounted(
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

    if (name == html_names::kSrcAttr) {
      LogUpdateAttributeIfIsolatedWorldAndInDocument("iframe", params);
      if (src_ != value) {
        src_ = value;
        should_call_did_change_attributes = true;
      }
    }
    if (name == html_names::kIdAttr && id_ != value) {
      id_ = value;
      should_call_did_change_attributes = true;
    }
    if (name == html_names::kNameAttr && name_ != value) {
      name_ = value;
      should_call_did_change_attributes = true;
    }
    HTMLFrameElementBase::ParseAttribute(params);
  }
  if (should_call_did_change_attributes) {
    // This causes IPC to the browser. Only call it once per parsing.
    DidChangeAttributes();
  }
}

DocumentPolicyFeatureState HTMLIFrameElement::ConstructRequiredPolicy() const {
  if (!RuntimeEnabledFeatures::DocumentPolicyNegotiationEnabled(
          GetExecutionContext())) {
    return {};
  }

  if (!required_policy_.empty()) {
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

ParsedPermissionsPolicy HTMLIFrameElement::ConstructContainerPolicy() const {
  if (!GetExecutionContext()) {
    return ParsedPermissionsPolicy();
  }

  scoped_refptr<const SecurityOrigin> src_origin =
      GetOriginForPermissionsPolicy();
  scoped_refptr<const SecurityOrigin> self_origin =
      GetExecutionContext()->GetSecurityOrigin();

  PolicyParserMessageBuffer logger;

  // Start with the allow attribute
  ParsedPermissionsPolicy container_policy =
      PermissionsPolicyParser::ParseAttribute(allow_, self_origin, src_origin,
                                              logger, GetExecutionContext());

  // Process the allow* attributes. These only take effect if the corresponding
  // feature is not present in the allow attribute's value.

  // If allowfullscreen attribute is present and no fullscreen policy is set,
  // enable the feature for all origins.
  if (AllowFullscreen()) {
    bool policy_changed = AllowFeatureEverywhereIfNotPresent(
        mojom::blink::PermissionsPolicyFeature::kFullscreen, container_policy);
    if (!policy_changed) {
      logger.Warn(
          "Allow attribute will take precedence over 'allowfullscreen'.");
    }
  }
  // If the allowpaymentrequest attribute is present and no 'payment' policy is
  // set, enable the feature for all origins.
  if (AllowPaymentRequest()) {
    bool policy_changed = AllowFeatureEverywhereIfNotPresent(
        mojom::blink::PermissionsPolicyFeature::kPayment, container_policy);
    // Measure cases where allowpaymentrequest had an actual effect, to see if
    // we can deprecate it. See https://crbug.com/1127988
    if (policy_changed) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kAllowPaymentRequestAttributeHasEffect);
    } else {
      logger.Warn(
          "Allow attribute will take precedence over 'allowpaymentrequest'.");
    }
  }

  // Factor in changes in client hint permissions.
  UpdateIFrameContainerPolicyWithDelegationSupportForClientHints(
      container_policy, GetDocument().domWindow());

  // Update the JavaScript policy object associated with this iframe, if it
  // exists.
  if (policy_) {
    policy_->UpdateContainerPolicy(container_policy, src_origin);
  }

  for (const auto& message : logger.GetMessages()) {
    GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther, message.level,
            message.content),
        /* discard_duplicates */ true);
  }

  return container_policy;
}

bool HTMLIFrameElement::LayoutObjectIsNeeded(const DisplayStyle& style) const {
  return ContentFrame() && !collapsed_by_client_ &&
         HTMLElement::LayoutObjectIsNeeded(style);
}

LayoutObject* HTMLIFrameElement::CreateLayoutObject(const ComputedStyle&) {
  return MakeGarbageCollected<LayoutIFrame>(this);
}

Node::InsertionNotificationRequest HTMLIFrameElement::InsertedInto(
    ContainerNode& insertion_point) {
  InsertionNotificationRequest result =
      HTMLFrameElementBase::InsertedInto(insertion_point);

  auto* html_doc = DynamicTo<HTMLDocument>(GetDocument());
  if (html_doc && insertion_point.IsInDocumentTree()) {
    html_doc->AddNamedItem(name_);
  }
  LogAddElementIfIsolatedWorldAndInDocument("iframe", html_names::kSrcAttr);
  return result;
}

void HTMLIFrameElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLFrameElementBase::RemovedFrom(insertion_point);
  auto* html_doc = DynamicTo<HTMLDocument>(GetDocument());
  if (html_doc && insertion_point.IsInDocumentTree()) {
    html_doc->RemoveNamedItem(name_);
  }
}

bool HTMLIFrameElement::IsInteractiveContent() const {
  return true;
}

network::mojom::ReferrerPolicy HTMLIFrameElement::ReferrerPolicyAttribute() {
  return referrer_policy_;
}

network::mojom::blink::TrustTokenParamsPtr
HTMLIFrameElement::ConstructTrustTokenParams() const {
  if (!trust_token_) {
    return nullptr;
  }

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

  // Only the send-redemption-record (the kSigning variant) operation is
  // valid in the iframe context.
  if (parsed_params->operation !=
      network::mojom::blink::TrustTokenOperationType::kSigning) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kError,
        "Trust Tokens: Attempted a trusttoken operation which isn't "
        "send-redemption-record in an iframe."));
    return nullptr;
  }

  if (!GetExecutionContext()->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kTrustTokenRedemption)) {
    GetExecutionContext()->AddConsoleMessage(MakeGarbageCollected<
                                             ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kError,
        "Trust Tokens: Attempted redemption or signing without the "
        "private-state-token-redemption Permissions Policy feature present."));
    return nullptr;
  }

  return parsed_params;
}

void HTMLIFrameElement::DidChangeAttributes() {
  // Don't notify about updates if ContentFrame() is null, for example when
  // the subframe hasn't been created yet; or if we are in the middle of
  // swapping one frame for another, in which case the final state
  // will be propagated at the end of the swapping operation.
  if (is_swapping_frames() || !ContentFrame()) {
    return;
  }

  // ParseContentSecurityPolicies needs a url to resolve report endpoints and
  // for matching the keyword 'self'. However, the csp attribute does not allow
  // report endpoints. Moreover, in the csp attribute, 'self' should not match
  // the owner's url, but rather the frame src url. This is taken care by the
  // Content-Security-Policy Embedded Enforcement algorithm, implemented in the
  // NavigationRequest. That's why we pass an empty url here.
  Vector<network::mojom::blink::ContentSecurityPolicyPtr> csp =
      ParseContentSecurityPolicies(
          required_csp_,
          network::mojom::blink::ContentSecurityPolicyType::kEnforce,
          network::mojom::blink::ContentSecurityPolicySource::kHTTP, KURL());
  DCHECK_LE(csp.size(), 1u);

  auto attributes = mojom::blink::IframeAttributes::New();
  attributes->parsed_csp_attribute = csp.empty() ? nullptr : std::move(csp[0]);
  attributes->credentialless = credentialless_;

  if (RuntimeEnabledFeatures::TopicsAPIEnabled(GetExecutionContext()) &&
      GetExecutionContext()->IsSecureContext()) {
    attributes->browsing_topics =
        !FastGetAttribute(html_names::kBrowsingtopicsAttr).IsNull();
  }

  if (GetExecutionContext()->IsSecureContext()) {
    attributes->ad_auction_headers =
        !FastGetAttribute(html_names::kAdauctionheadersAttr).IsNull();
  }

  if (RuntimeEnabledFeatures::SharedStorageAPIM118Enabled(
          GetExecutionContext()) &&
      GetExecutionContext()->IsSecureContext()) {
    attributes->shared_storage_writable_opted_in =
        !FastGetAttribute(html_names::kSharedstoragewritableAttr).IsNull();
  }

  attributes->id = ConvertToReportValue(id_);
  attributes->name = ConvertToReportValue(name_);
  attributes->src = ConvertToReportValue(src_);
  GetDocument().GetFrame()->GetLocalFrameHostRemote().DidChangeIframeAttributes(
      ContentFrame()->GetFrameToken(), std::move(attributes));

  // Make sure we update the srcdoc value, if any, in the browser.
  String srcdoc_value = "";
  if (FastHasAttribute(html_names::kSrcdocAttr)) {
    srcdoc_value = FastGetAttribute(html_names::kSrcdocAttr).GetString();
  }
  GetDocument().GetFrame()->GetLocalFrameHostRemote().DidChangeSrcDoc(
      ContentFrame()->GetFrameToken(), srcdoc_value);
}

}  // namespace blink
