// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"

#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/style_rule_keyframe.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/permissions_policy/layout_animations_policy.h"

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
                       other->profile_,
                       other->referrer_,
                       other->is_html_document_,
                       other->use_legacy_background_size_shorthand_behavior_,
                       other->secure_context_mode_,
                       other->world_,
                       use_counter_document,
                       other->resource_fetch_restriction_) {
  is_ad_related_ = other->is_ad_related_;
}

CSSParserContext::CSSParserContext(const CSSParserContext* other,
                                   const KURL& base_url,
                                   bool origin_clean,
                                   const Referrer& referrer,
                                   const WTF::TextEncoding& charset,
                                   const Document* use_counter_document)
    : CSSParserContext(base_url,
                       origin_clean,
                       charset,
                       other->mode_,
                       other->profile_,
                       referrer,
                       other->is_html_document_,
                       other->use_legacy_background_size_shorthand_behavior_,
                       other->secure_context_mode_,
                       other->world_,
                       use_counter_document,
                       other->resource_fetch_restriction_) {
  is_ad_related_ = other->is_ad_related_;
}

CSSParserContext::CSSParserContext(CSSParserMode mode,
                                   SecureContextMode secure_context_mode,
                                   SelectorProfile profile,
                                   const Document* use_counter_document)
    : CSSParserContext(KURL(),
                       true /* origin_clean */,
                       WTF::TextEncoding(),
                       mode,
                       profile,
                       Referrer(),
                       false,
                       false,
                       secure_context_mode,
                       nullptr,
                       use_counter_document,
                       ResourceFetchRestriction::kNone) {}

CSSParserContext::CSSParserContext(const Document& document)
    : CSSParserContext(document, document.BaseURL()) {}

CSSParserContext::CSSParserContext(const Document& document,
                                   const KURL& base_url_override)
    : CSSParserContext(
          document,
          base_url_override,
          true /* origin_clean */,
          Referrer(document.GetExecutionContext()
                       ? document.GetExecutionContext()->OutgoingReferrer()
                       : String(),  // GetExecutionContext() only returns null
                                    // in tests.
                   document.GetReferrerPolicy()),
          WTF::TextEncoding(),
          kLiveProfile) {}

CSSParserContext::CSSParserContext(
    const Document& document,
    const KURL& base_url_override,
    bool origin_clean,
    const Referrer& referrer,
    const WTF::TextEncoding& charset,
    SelectorProfile profile,
    enum ResourceFetchRestriction resource_fetch_restriction)
    : CSSParserContext(
          base_url_override,
          origin_clean,
          charset,
          document.InQuirksMode() ? kHTMLQuirksMode : kHTMLStandardMode,
          profile,
          referrer,
          IsA<HTMLDocument>(document),
          document.GetSettings()
              ? document.GetSettings()
                    ->GetUseLegacyBackgroundSizeShorthandBehavior()
              : false,
          document.GetExecutionContext()
              ? document.GetExecutionContext()->GetSecureContextMode()
              : SecureContextMode::kInsecureContext,
          document.GetExecutionContext()
              ? document.GetExecutionContext()->GetCurrentWorld()
              : nullptr,
          &document,
          resource_fetch_restriction) {}

CSSParserContext::CSSParserContext(const ExecutionContext& context)
    : CSSParserContext(context.Url(),
                       true /* origin_clean */,
                       WTF::TextEncoding(),
                       kHTMLStandardMode,
                       kLiveProfile,
                       Referrer(context.Url().StrippedForUseAsReferrer(),
                                context.GetReferrerPolicy()),
                       true,
                       false,
                       context.GetSecureContextMode(),
                       context.GetCurrentWorld(),
                       IsA<LocalDOMWindow>(&context)
                           ? To<LocalDOMWindow>(context).document()
                           : nullptr,
                       ResourceFetchRestriction::kNone) {}

CSSParserContext::CSSParserContext(
    const KURL& base_url,
    bool origin_clean,
    const WTF::TextEncoding& charset,
    CSSParserMode mode,
    SelectorProfile profile,
    const Referrer& referrer,
    bool is_html_document,
    bool use_legacy_background_size_shorthand_behavior,
    SecureContextMode secure_context_mode,
    scoped_refptr<const DOMWrapperWorld> world,
    const Document* use_counter_document,
    enum ResourceFetchRestriction resource_fetch_restriction)
    : base_url_(base_url),
      world_(std::move(world)),
      origin_clean_(origin_clean),
      mode_(mode),
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
         profile_ == other.profile_ && is_ad_related_ == other.is_ad_related_ &&
         is_html_document_ == other.is_html_document_ &&
         use_legacy_background_size_shorthand_behavior_ ==
             other.use_legacy_background_size_shorthand_behavior_ &&
         secure_context_mode_ == other.secure_context_mode_ &&
         resource_fetch_restriction_ == other.resource_fetch_restriction_;
}

// TODO(xiaochengh): This function never returns null. Change it to return a
// const reference to avoid confusion.
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
    LEAK_SANITIZER_IGNORE_OBJECT(&context);
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
  if (url.IsNull()) {
    return KURL();
  }
  if (!Charset().IsValid()) {
    return KURL(BaseURL(), url);
  }
  return KURL(BaseURL(), url, Charset());
}

KURL CSSParserContext::CompleteNonEmptyURL(const String& url) const {
  if (url.empty() && !url.IsNull()) {
    return KURL(g_empty_string);
  }
  return CompleteURL(url);
}

void CSSParserContext::Count(WebFeature feature) const {
  if (IsUseCounterRecordingEnabled()) {
    document_->CountUse(feature);
  }
}

void CSSParserContext::CountDeprecation(WebFeature feature) const {
  if (IsUseCounterRecordingEnabled() && document_) {
    Deprecation::CountDeprecation(document_->GetExecutionContext(), feature);
  }
}

void CSSParserContext::Count(CSSParserMode mode, CSSPropertyID property) const {
  if (IsUseCounterRecordingEnabled() && IsUseCounterEnabledForMode(mode)) {
    document_->CountProperty(property);
  }
}

bool CSSParserContext::IsDocumentHandleEqual(const Document* other) const {
  return document_.Get() == other;
}

const Document* CSSParserContext::GetDocument() const {
  return document_.Get();
}

// Fuzzers may execution CSS parsing code without a Document being available,
// thus this method can return null.
const ExecutionContext* CSSParserContext::GetExecutionContext() const {
  return (document_.Get()) ? document_.Get()->GetExecutionContext() : nullptr;
}

void CSSParserContext::ReportLayoutAnimationsViolationIfNeeded(
    const StyleRuleKeyframe& rule) const {
  if (!document_ || !document_->GetExecutionContext()) {
    return;
  }
  for (unsigned i = 0; i < rule.Properties().PropertyCount(); ++i) {
    CSSPropertyID id = rule.Properties().PropertyAt(i).Id();
    if (id == CSSPropertyID::kVariable) {
      continue;
    }
    const CSSProperty& property = CSSProperty::Get(id);
    if (!LayoutAnimationsPolicy::AffectedCSSProperties().Contains(&property)) {
      continue;
    }
    LayoutAnimationsPolicy::ReportViolation(property,
                                            *document_->GetExecutionContext());
  }
}

bool CSSParserContext::IsForMarkupSanitization() const {
  return document_ && document_->IsForMarkupSanitization();
}

void CSSParserContext::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
}

}  // namespace blink
