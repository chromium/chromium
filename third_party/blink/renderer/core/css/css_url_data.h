/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_URL_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_URL_DATA_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class Document;
class KURL;
class TextEncoding;

// Stores data for a <url> value (url(), src()).
class CORE_EXPORT CSSUrlData : public GarbageCollected<CSSUrlData> {
 public:
  CSSUrlData(const AtomicString& unresolved_url,
             const KURL& resolved_url,
             const Referrer&,
             bool is_from_origin_clean_style_sheet,
             bool is_ad_related);
  CSSUrlData(base::PassKey<CSSUrlData>,
             const AtomicString& unresolved_url,
             const AtomicString& resolved_url,
             const Referrer&,
             bool is_from_origin_clean_style_sheet,
             bool is_ad_related,
             bool is_local,
             bool potentially_dangling_markup);

  // Create URL data with a resolved (absolute) URL. Generally used for
  // computed values - the above should otherwise be preferred.
  explicit CSSUrlData(const AtomicString& resolved_url);

  // Returns the resolved URL, potentially reresolving against passed Document
  // if there's a potential risk of "dangling markup".
  KURL ResolveUrl(const Document&) const;

  // Re-resolve the URL against the base provided by the passed
  // Document. Returns true if the resolved URL changed, otherwise false.
  bool ReResolveUrl(const Document&) const;

  // Returns a copy of this URL data suitable for computed value.
  const CSSUrlData* MakeComputed() const;

  // Returns a copy where the unresolved URL has been resolved against
  // `base_url` (using `charset` encoding if valid).
  const CSSUrlData* MakeResolved(const KURL& base_url,
                                 const TextEncoding& charset) const;

  // Returns a copy with the URL (re)resolved against the base URL of the
  // document if there's is potential risk of "dangling markup". Otherwise
  // returns itself.
  const CSSUrlData* MakeResolvedIfDanglingMarkup(const Document&) const;

  // Returns a copy where the referrer has been reset.
  const CSSUrlData* MakeWithoutReferrer() const;

  const AtomicString& ValueForSerialization() const {
    return is_local_ || absolute_url_.empty() ? relative_url_ : absolute_url_;
  }

  const AtomicString& UnresolvedUrl() const { return relative_url_; }
  const AtomicString& ResolvedUrl() const { return absolute_url_; }

  const Referrer& GetReferrer() const { return referrer_; }

  bool IsFromOriginCleanStyleSheet() const {
    return is_from_origin_clean_style_sheet_;
  }
  bool IsAdRelated() const { return is_ad_related_; }

  // Returns true if this URL is "local" to the specified Document (either by
  // being a fragment-only URL or by matching the document URL).
  bool IsLocal(const Document&) const;

  String CssText() const;

  void Trace(Visitor*) const {}

  bool operator==(const CSSUrlData& other) const;

 private:
  AtomicString relative_url_;
  mutable AtomicString absolute_url_;
  const Referrer referrer_;

  // The 'local url flag': https://drafts.csswg.org/css-values/#local-urls
  const bool is_local_;

  // Whether the stylesheet that requested this image is origin-clean:
  // https://drafts.csswg.org/cssom-1/#concept-css-style-sheet-origin-clean-flag
  const bool is_from_origin_clean_style_sheet_;

  // Whether this was created by an ad-related CSSParserContext.
  const bool is_ad_related_;

  // The url passed into the constructor had the PotentiallyDanglingMarkup flag
  // set. That information needs to be passed on to the fetch code to block such
  // resources from loading.
  const bool potentially_dangling_markup_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_URL_DATA_H_
