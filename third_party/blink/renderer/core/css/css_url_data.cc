/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/css_url_data.h"

#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {

void CSSUrlRequestModifiers::AppendCssText(StringBuilder& result) const {
  if (cross_origin != kCrossOriginAttributeNotSet) {
    result.Append(" cross-origin(");
    result.Append(cross_origin == kCrossOriginAttributeAnonymous
                      ? "anonymous"
                      : "use-credentials");
    result.Append(')');
  }
  if (!integrity.IsNull()) {
    result.Append(" integrity(");
    SerializeString(integrity, result);
    result.Append(')');
  }
  if (referrer_policy) {
    CSSValueID value_id;
    switch (*referrer_policy) {
      case network::mojom::blink::ReferrerPolicy::kNever:
        value_id = CSSValueID::kNoReferrer;
        break;
      case network::mojom::blink::ReferrerPolicy::kNoReferrerWhenDowngrade:
        value_id = CSSValueID::kNoReferrerWhenDowngrade;
        break;
      case network::mojom::blink::ReferrerPolicy::kSameOrigin:
        value_id = CSSValueID::kSameOrigin;
        break;
      case network::mojom::blink::ReferrerPolicy::kOrigin:
        value_id = CSSValueID::kOrigin;
        break;
      case network::mojom::blink::ReferrerPolicy::kStrictOrigin:
        value_id = CSSValueID::kStrictOrigin;
        break;
      case network::mojom::blink::ReferrerPolicy::kOriginWhenCrossOrigin:
        value_id = CSSValueID::kOriginWhenCrossOrigin;
        break;
      case network::mojom::blink::ReferrerPolicy::kStrictOriginWhenCrossOrigin:
        value_id = CSSValueID::kStrictOriginWhenCrossOrigin;
        break;
      case network::mojom::blink::ReferrerPolicy::kAlways:
        value_id = CSSValueID::kUnsafeUrl;
        break;
      case network::mojom::blink::ReferrerPolicy::kDefault:
        NOTREACHED();
    }
    result.Append(" referrer-policy(");
    result.Append(GetCSSValueNameAs<StringView>(value_id));
    result.Append(')');
  }
}

CSSUrlData::CSSUrlData(const AtomicString& unresolved_url,
                       const KURL& resolved_url,
                       const Referrer& referrer,
                       bool is_from_origin_clean_style_sheet,
                       bool is_ad_related,
                       const CSSUrlRequestModifiers& modifiers)
    : relative_url_(unresolved_url),
      absolute_url_(resolved_url.GetString()),
      referrer_(referrer),
      is_local_(unresolved_url.starts_with('#')),
      is_from_origin_clean_style_sheet_(is_from_origin_clean_style_sheet),
      is_ad_related_(is_ad_related),
      potentially_dangling_markup_(resolved_url.PotentiallyDanglingMarkup()),
      modifiers_(modifiers) {}

CSSUrlData::CSSUrlData(base::PassKey<CSSUrlData>,
                       const AtomicString& unresolved_url,
                       const AtomicString& resolved_url,
                       const Referrer& referrer,
                       bool is_from_origin_clean_style_sheet,
                       bool is_ad_related,
                       bool is_local,
                       bool potentially_dangling_markup,
                       const CSSUrlRequestModifiers& modifiers)
    : relative_url_(unresolved_url),
      absolute_url_(resolved_url),
      referrer_(referrer),
      is_local_(is_local),
      is_from_origin_clean_style_sheet_(is_from_origin_clean_style_sheet),
      is_ad_related_(is_ad_related),
      potentially_dangling_markup_(potentially_dangling_markup),
      modifiers_(modifiers) {}

CSSUrlData::CSSUrlData(const AtomicString& resolved_url)
    : CSSUrlData(resolved_url,
                 KURL(resolved_url),
                 Referrer(),
                 /*is_from_origin_clean_style_sheet=*/true,
                 /*is_ad_related=*/false,
                 /*modifiers=*/CSSUrlRequestModifiers()) {}

KURL CSSUrlData::ResolveUrl(const Document& document) const {
  if (!potentially_dangling_markup_) {
    return KURL(absolute_url_);
  }
  // The PotentiallyDanglingMarkup() flag is lost when storing the absolute
  // url as a string from which the KURL is constructed here. The url passed
  // into the constructor had the PotentiallyDanglingMarkup flag set. That
  // information needs to be passed on to the fetch code to block such
  // resources from loading.
  //
  // Note: the PotentiallyDanglingMarkup() state on the base url may have
  // changed if the base url for the document changed since last time the url
  // was resolved. This change in base url resolving is different from the
  // typical behavior for base url changes. CSS urls are typically not re-
  // resolved. This is mentioned in the "What “browser eccentricities”?" note
  // in https://www.w3.org/TR/css-values-3/#local-urls
  //
  // Having the more spec-compliant behavior for the dangling markup edge case
  // should be fine.
  return document.CompleteURL(relative_url_);
}

bool CSSUrlData::ReResolveUrl(const Document& document) const {
  if (relative_url_.empty()) {
    return false;
  }
  KURL url = document.CompleteURL(relative_url_);
  AtomicString url_string(url.GetString());
  if (url_string == absolute_url_) {
    return false;
  }
  absolute_url_ = url_string;
  return true;
}

const CSSUrlData* CSSUrlData::MakeComputed() const {
  if (relative_url_.empty() || is_local_ || absolute_url_.empty()) {
    return this;
  }
  return MakeGarbageCollected<CSSUrlData>(
      base::PassKey<CSSUrlData>(), absolute_url_, absolute_url_, Referrer(),
      is_from_origin_clean_style_sheet_, is_ad_related_, is_local_,
      potentially_dangling_markup_, modifiers_);
}

const CSSUrlData* CSSUrlData::MakeResolved(const KURL& base_url,
                                           const TextEncoding& charset) const {
  if (relative_url_.empty()) {
    return this;
  }
  const KURL resolved_url = charset.IsValid()
                                ? KURL(base_url, relative_url_, charset)
                                : KURL(base_url, relative_url_);
  if (is_local_) {
    return MakeGarbageCollected<CSSUrlData>(
        relative_url_, resolved_url, Referrer(),
        is_from_origin_clean_style_sheet_, is_ad_related_, modifiers_);
  }
  return MakeGarbageCollected<CSSUrlData>(
      AtomicString(resolved_url.GetString()), resolved_url, Referrer(),
      is_from_origin_clean_style_sheet_, is_ad_related_, modifiers_);
}

const CSSUrlData* CSSUrlData::MakeResolvedIfDanglingMarkup(
    const Document& document) const {
  if (!potentially_dangling_markup_) {
    return this;
  }
  return MakeGarbageCollected<CSSUrlData>(
      relative_url_, ResolveUrl(document), referrer_,
      is_from_origin_clean_style_sheet_, is_ad_related_, modifiers_);
}

const CSSUrlData* CSSUrlData::MakeWithoutReferrer() const {
  return MakeGarbageCollected<CSSUrlData>(
      relative_url_, KURL(absolute_url_), Referrer(),
      is_from_origin_clean_style_sheet_, is_ad_related_, modifiers_);
}

bool CSSUrlData::IsLocal(const Document& document) const {
  return is_local_ ||
         EqualIgnoringFragmentIdentifier(KURL(absolute_url_), document.Url());
}

String CSSUrlData::CssText() const {
  return SerializeURI(relative_url_, modifiers_);
}

bool CSSUrlData::operator==(const CSSUrlData& other) const {
  // If only one has the 'local url' flag set, the URLs can't match.
  if (is_local_ != other.is_local_) {
    return false;
  }
  if (is_local_) {
    return relative_url_ == other.relative_url_;
  }
  if (modifiers_ != other.modifiers_) {
    return false;
  }
  if (absolute_url_.empty() && other.absolute_url_.empty()) {
    return relative_url_ == other.relative_url_;
  }
  return absolute_url_ == other.absolute_url_;
}

}  // namespace blink
