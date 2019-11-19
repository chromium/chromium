// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"

#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/style_rule_keyframe.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/feature_policy/layout_animations_policy.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/imports/html_imports_controller.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

CSSParserContext::CSSParserContext(const CSSParserContext* other,
                                   const CSSStyleSheet* style_sheet)
    : CSSParserContext(other, CSSStyleSheet::SingleOwnerDocument(style_sheet)) {
}

CSSParserContext::CSSParserContext(
    const CSSParserContext* other,
    const StyleSheetContents* style_sheet_contents)
    : CSSParserContext(
          other,
          StyleSheetContents::SingleOwnerDocument(style_sheet_contents)) {}

CSSParserContext::CSSParserContext(const CSSParserContext* other,
                                   const Document* use_counter_document)
    : CSSParserContext(other->base_url_,
                       other->origin_clean_,
                       other->charset_,
                       other->mode_,
                       other->match_mode_,
                       other->profile_,
                       other->referrer_,
                       other->is_html_document_,
                       other->use_legacy_background_size_shorthand_behavior_,
                       other->secure_context_mode_,
                       other->should_check_content_security_policy_,
                       use_counter_document,
                       other->resource_fetch_restriction_) {}

CSSParserContext::CSSParserContext(
    const CSSParserContext* other,
    const KURL& base_url,
    bool origin_clean,
    network::mojom::ReferrerPolicy referrer_policy,
    const WTF::TextEncoding& charset,
    const Document* use_counter_document)
    : CSSParserContext(
          base_url,
          origin_clean,
          charset,
          other->mode_,
          other->match_mode_,
          other->profile_,
          Referrer(base_url.StrippedForUseAsReferrer(), referrer_policy),
          other->is_html_document_,
          other->use_legacy_background_size_shorthand_behavior_,
          other->secure_context_mode_,
          other->should_check_content_security_policy_,
          use_counter_document,
          other->resource_fetch_restriction_) {}

CSSParserContext::CSSParserContext(CSSParserMode mode,
                                   SecureContextMode secure_context_mode,
                                   SelectorProfile profile,
                                   const Document* use_counter_document)
    : CSSParserContext(KURL(),
                       true /* origin_clean */,
                       WTF::TextEncoding(),
                       mode,
                       mode,
                       profile,
                       Referrer(),
                       false,
                       false,
                       secure_context_mode,
                       kDoNotCheckContentSecurityPolicy,
                       use_counter_document,
                       ResourceFetchRestriction::kNone) {}

CSSParserContext::CSSParserContext(const Document& document)
    : CSSParserContext(document,
                       document.BaseURL(),
                       true /* origin_clean */,
                       document.GetReferrerPolicy(),
                       WTF::TextEncoding(),
                       kLiveProfile) {}

CSSParserContext::CSSParserContext(
    const Document& document,
    const KURL& base_url_override,
    bool origin_clean,
    network::mojom::ReferrerPolicy referrer_policy_override,
    const WTF::TextEncoding& charset,
    SelectorProfile profile,
    enum ResourceFetchRestriction resource_fetch_restriction)
    : CSSParserContext(
          base_url_override,
          origin_clean,
          charset,
          document.InQuirksMode() ? kHTMLQuirksMode : kHTMLStandardMode,
          document.ImportsController() && profile == kLiveProfile
              ? (document.ImportsController()->Master()->InQuirksMode()
                     ? kHTMLQuirksMode
                     : kHTMLStandardMode)
              : document.InQuirksMode() ? kHTMLQuirksMode : kHTMLStandardMode,
          profile,
          Referrer(base_url_override.StrippedForUseAsReferrer(),
                   referrer_policy_override),
          document.IsHTMLDocument(),
          document.GetSettings()
              ? document.GetSettings()
                    ->GetUseLegacyBackgroundSizeShorthandBehavior()
              : false,
          document.GetSecureContextMode(),
          ContentSecurityPolicy::ShouldBypassMainWorld(&document)
              ? kDoNotCheckContentSecurityPolicy
              : kCheckContentSecurityPolicy,
          &document,
          resource_fetch_restriction) {}

CSSParserContext::CSSParserContext(const ExecutionContext& context)
    : CSSParserContext(context.Url(),
                       true /* origin_clean */,
                       WTF::TextEncoding(),
                       kHTMLStandardMode,
                       kHTMLStandardMode,
                       kLiveProfile,
                       Referrer(context.Url().StrippedForUseAsReferrer(),
                                context.GetReferrerPolicy()),
                       true,
                       false,
                       context.GetSecureContextMode(),
                       ContentSecurityPolicy::ShouldBypassMainWorld(&context)
                           ? kDoNotCheckContentSecurityPolicy
                           : kCheckContentSecurityPolicy,
                       DynamicTo<Document>(context),
                       ResourceFetchRestriction::kNone) {}

CSSParserContext::CSSParserContext(
    const KURL& base_url,
    bool origin_clean,
    const WTF::TextEncoding& charset,
    CSSParserMode mode,
    CSSParserMode match_mode,
    SelectorProfile profile,
    const Referrer& referrer,
    bool is_html_document,
    bool use_legacy_background_size_shorthand_behavior,
    SecureContextMode secure_context_mode,
    ContentSecurityPolicyDisposition policy_disposition,
    const Document* use_counter_document,
    enum ResourceFetchRestriction resource_fetch_restriction)
    : base_url_(base_url),
      should_check_content_security_policy_(policy_disposition),
      origin_clean_(origin_clean),
      mode_(mode),
      match_mode_(match_mode),
      profile_(profile),
      referrer_(referrer),
      is_html_document_(is_html_document),
      use_legacy_background_size_shorthand_behavior_(
          use_legacy_background_size_shorthand_behavior),
      secure_context_mode_(secure_context_mode),
      charset_(charset),
      document_(use_counter_document),
      resource_fetch_restriction_(resource_fetch_restriction) {}

bool CSSParserContext::operator==(const CSSParserContext& other) const {
  return base_url_ == other.base_url_ && origin_clean_ == other.origin_clean_ &&
         charset_ == other.charset_ && mode_ == other.mode_ &&
         match_mode_ == other.match_mode_ && profile_ == other.profile_ &&
         is_html_document_ == other.is_html_document_ &&
         use_legacy_background_size_shorthand_behavior_ ==
             other.use_legacy_background_size_shorthand_behavior_ &&
         secure_context_mode_ == other.secure_context_mode_ &&
         resource_fetch_restriction_ == other.resource_fetch_restriction_;
}

const CSSParserContext* StrictCSSParserContext(
    SecureContextMode secure_context_mode) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<Persistent<CSSParserContext>>,
                                  strict_context_pool, ());
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<Persistent<CSSParserContext>>,
                                  secure_strict_context_pool, ());

  Persistent<CSSParserContext>& context =
      secure_context_mode == SecureContextMode::kSecureContext
          ? *secure_strict_context_pool
          : *strict_context_pool;
  if (!context) {
    context = MakeGarbageCollected<CSSParserContext>(kHTMLStandardMode,
                                                     secure_context_mode);
    context.RegisterAsStaticReference();
  }

  return context;
}

bool CSSParserContext::IsOriginClean() const {
  return origin_clean_;
}

bool CSSParserContext::IsSecureContext() const {
  return secure_context_mode_ == SecureContextMode::kSecureContext;
}

KURL CSSParserContext::CompleteURL(const String& url) const {
  if (url.IsNull())
    return KURL();
  if (!Charset().IsValid())
    return KURL(BaseURL(), url);
  return KURL(BaseURL(), url, Charset());
}

void CSSParserContext::Count(WebFeature feature) const {
  if (IsUseCounterRecordingEnabled())
    document_->CountUse(feature);
}

void CSSParserContext::CountDeprecation(WebFeature feature) const {
  if (IsUseCounterRecordingEnabled())
    Deprecation::CountDeprecation(*document_, feature);
}

void CSSParserContext::Count(CSSParserMode mode, CSSPropertyID property) const {
  if (IsUseCounterRecordingEnabled() && IsUseCounterEnabledForMode(mode)) {
    document_->CountProperty(property);
  }
}

bool CSSParserContext::IsDocumentHandleEqual(const Document* other) const {
  return document_.Get() == other;
}

void CSSParserContext::ReportLayoutAnimationsViolationIfNeeded(
    const StyleRuleKeyframe& rule) const {
  if (!document_)
    return;
  for (size_t i = 0; i < rule.Properties().PropertyCount(); ++i) {
    const CSSProperty& property = rule.Properties().PropertyAt(i).Property();
    if (!LayoutAnimationsPolicy::AffectedCSSProperties().Contains(&property))
      continue;
    LayoutAnimationsPolicy::ReportViolation(property, *document_);
  }
}

bool CSSParserContext::CustomElementsV0Enabled() const {
  // Support features conservatively.
  if (!document_)
    return true;
  return RuntimeEnabledFeatures::CustomElementsV0Enabled(document_);
}

void CSSParserContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
}

}  // namespace blink
