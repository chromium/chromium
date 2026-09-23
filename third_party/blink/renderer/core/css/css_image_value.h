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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_IMAGE_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_IMAGE_VALUE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_url_data.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/loader/fetch/cross_origin_attribute_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class Document;
class ResourceFetcher;
class StyleImage;
class SVGResource;

class CORE_EXPORT CSSImageValue : public CSSValue {
 public:
  // Optionally accepts a pre-resolved `fetcher_agnostic_image` that should not
  // be associated with a `ResourceFetcher`. It will be made
  // available as a fallback to all callers checking for an already resolved
  // `StyleImage` (`IsCachePending` / `CachedImage(ResourceFetcher*)`). Callers
  // that are potentially attempting a new fetch (`CacheImage(Document&...)`)
  // will still proceed with their own, and the result will take precedence over
  // this value.
  //
  // At the time of this writing, this option is used exclusively by
  // `StyleFetchedImage::CssValue()` to expose an already-fetched,
  // document-scoped image to fetcher-less consumers such as CSS paint/layout
  // worklets and other cross-thread Typed OM readers.
  CSSImageValue(const CSSUrlData& url_data,
                StyleImage* fetcher_agnostic_image = nullptr);
  ~CSSImageValue();

  bool IsCachePending(ResourceFetcher* fetcher) const;
  StyleImage* CachedImage(ResourceFetcher* fetcher) const;
  FetchParameters PrepareFetch(const Document&,
                               CrossOriginAttributeValue) const;
  StyleImage* CacheImage(
      Document&,
      CrossOriginAttributeValue = kCrossOriginAttributeNotSet,
      const float override_image_resolution = 0.0f);

  const String& RelativeUrl() const { return UrlData().UnresolvedUrl(); }
  bool IsLocal(const Document&) const;
  AtomicString NormalizedFragmentIdentifier() const;
  const CSSUrlData& UrlData() const { return *url_data_; }

  void ReResolveURL(const Document&) const;

  String CustomCSSText() const;

  bool HasFailedOrCanceledSubresources(ResourceFetcher*) const;

  bool Equals(const CSSImageValue&) const;

  CSSImageValue* ComputedCSSValue() const;

  CSSImageValue* Clone() const;

  void SetInitiator(const AtomicString& name) { initiator_name_ = name; }

  void TraceAfterDispatch(blink::Visitor*) const;
  void RestoreCachedResourceIfNeeded(const Document&) const;
  SVGResource* EnsureSVGResource(ResourceFetcher*) const;

 private:
  static CSSImageValue* Copy(const CSSImageValue& other,
                             const CSSUrlData& url_data);

  AtomicString initiator_name_;
  const Member<const CSSUrlData> url_data_;

  // Cached image data.
  // fetcher_agnostic_image_ is the lowest-priority fallback image. See the
  // comment on the constructor for more details.
  Member<StyleImage> fetcher_agnostic_image_;
  mutable Member<StyleImage> cached_image_;
  mutable HeapHashMap<WeakMember<ResourceFetcher>, Member<StyleImage>>
      cached_images_;
  mutable Member<SVGResource> svg_resource_;
  mutable HeapHashMap<WeakMember<ResourceFetcher>, Member<SVGResource>>
      svg_resources_;
};

template <>
struct DowncastTraits<CSSImageValue> {
  static bool AllowFrom(const CSSValue& value) { return value.IsImageValue(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_IMAGE_VALUE_H_
