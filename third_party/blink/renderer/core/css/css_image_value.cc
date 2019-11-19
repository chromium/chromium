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

#include "third_party/blink/renderer/core/css/css_image_value.h"

#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/platform/loader/fetch/cross_origin_attribute_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

CSSImageValue::CSSImageValue(const AtomicString& raw_value,
                             const KURL& url,
                             const Referrer& referrer,
                             StyleImage* image,
                             OriginClean origin_clean)
    : CSSValue(kImageClass),
      relative_url_(raw_value),
      referrer_(referrer),
      absolute_url_(url.GetString()),
      cached_image_(image),
      origin_clean_(origin_clean) {}

CSSImageValue::CSSImageValue(const AtomicString& absolute_url,
                             OriginClean origin_clean)
    : CSSValue(kImageClass),
      relative_url_(absolute_url),
      absolute_url_(absolute_url),
      origin_clean_(OriginClean::kFalse) {}

CSSImageValue::~CSSImageValue() = default;

StyleImage* CSSImageValue::CacheImage(
    const Document& document,
    FetchParameters::ImageRequestOptimization image_request_optimization,
    CrossOriginAttributeValue cross_origin) {
  if (!cached_image_) {
    if (absolute_url_.IsEmpty())
      ReResolveURL(document);
    ResourceRequest resource_request(absolute_url_);
    resource_request.SetReferrerPolicy(
        ReferrerPolicyResolveDefault(referrer_.referrer_policy));
    resource_request.SetReferrerString(referrer_.referrer);
    ResourceLoaderOptions options;
    options.initiator_info.name = initiator_name_.IsEmpty()
                                      ? fetch_initiator_type_names::kCSS
                                      : initiator_name_;
    FetchParameters params(resource_request, options);

    if (cross_origin != kCrossOriginAttributeNotSet) {
      params.SetCrossOriginAccessControl(document.GetSecurityOrigin(),
                                         cross_origin);
    }

    bool is_lazily_loaded =
        image_request_optimization == FetchParameters::kDeferImageLoad &&
        // Only http/https images are eligible to be lazily loaded.
        params.Url().ProtocolIsInHTTPFamily();
    if (is_lazily_loaded) {
      if (document.GetFrame() && document.GetFrame()->Client()) {
        document.GetFrame()->Client()->DidObserveLazyLoadBehavior(
            WebLocalFrameClient::LazyLoadBehavior::kDeferredImage);
      }
      params.SetLazyImageDeferred();
    }

    if (origin_clean_ != OriginClean::kTrue)
      params.SetFromOriginDirtyStyleSheet(true);

    cached_image_ = MakeGarbageCollected<StyleFetchedImage>(document, params,
                                                            is_lazily_loaded);
  }
  return cached_image_.Get();
}

void CSSImageValue::RestoreCachedResourceIfNeeded(
    const Document& document) const {
  if (!cached_image_ || !document.Fetcher() || absolute_url_.IsNull())
    return;

  ImageResourceContent* resource = cached_image_->CachedImage();
  if (!resource)
    return;

  resource->EmulateLoadStartedForInspector(
      document.Fetcher(), KURL(absolute_url_),
      initiator_name_.IsEmpty() ? fetch_initiator_type_names::kCSS
                                : initiator_name_);
}

bool CSSImageValue::HasFailedOrCanceledSubresources() const {
  if (!cached_image_)
    return false;
  if (ImageResourceContent* cached_resource = cached_image_->CachedImage())
    return cached_resource->LoadFailedOrCanceled();
  return true;
}

bool CSSImageValue::Equals(const CSSImageValue& other) const {
  if (absolute_url_.IsEmpty() && other.absolute_url_.IsEmpty())
    return relative_url_ == other.relative_url_;
  return absolute_url_ == other.absolute_url_;
}

String CSSImageValue::CustomCSSText() const {
  return SerializeURI(relative_url_);
}

bool CSSImageValue::KnownToBeOpaque(const Document& document,
                                    const ComputedStyle& style) const {
  return cached_image_ ? cached_image_->KnownToBeOpaque(document, style)
                       : false;
}

void CSSImageValue::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(cached_image_);
  CSSValue::TraceAfterDispatch(visitor);
}

void CSSImageValue::ReResolveURL(const Document& document) const {
  KURL url = document.CompleteURL(relative_url_);
  AtomicString url_string(url.GetString());
  if (url_string == absolute_url_)
    return;
  absolute_url_ = url_string;
  cached_image_.Clear();
}

}  // namespace blink
