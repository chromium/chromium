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

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/core/svg/proxy_svg_resource_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/cross_origin_attribute_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

CSSImageValue::CSSImageValue(CSSUrlData url_data,
                             const Referrer& referrer,
                             OriginClean origin_clean,
                             bool is_ad_related,
                             StyleImage* image)
    : CSSValue(kImageClass),
      url_data_(std::move(url_data)),
      referrer_(referrer),
      cached_image_(image),
      origin_clean_(origin_clean),
      is_ad_related_(is_ad_related) {}

CSSImageValue::~CSSImageValue() = default;

FetchParameters CSSImageValue::PrepareFetch(
    const Document& document,
    FetchParameters::ImageRequestBehavior image_request_behavior,
    CrossOriginAttributeValue cross_origin) const {
  ResourceRequest resource_request(url_data_.ResolveUrl(document));
  resource_request.SetReferrerPolicy(
      ReferrerUtils::MojoReferrerPolicyResolveDefault(
          referrer_.referrer_policy));
  resource_request.SetReferrerString(referrer_.referrer);
  if (is_ad_related_) {
    resource_request.SetIsAdResource();
  }
  ExecutionContext* execution_context = document.GetExecutionContext();
  ResourceLoaderOptions options(execution_context->GetCurrentWorld());
  options.initiator_info.name = initiator_name_.empty()
                                    ? fetch_initiator_type_names::kCSS
                                    : initiator_name_;
  if (referrer_.referrer != Referrer::ClientReferrerString()) {
    options.initiator_info.referrer = referrer_.referrer;
  }
  FetchParameters params(std::move(resource_request), options);

  if (cross_origin != kCrossOriginAttributeNotSet) {
    params.SetCrossOriginAccessControl(execution_context->GetSecurityOrigin(),
                                       cross_origin);
  }

  if (image_request_behavior ==
      FetchParameters::ImageRequestBehavior::kDeferImageLoad) {
    params.SetLazyImageDeferred();
  }

  if (origin_clean_ != OriginClean::kTrue) {
    params.SetFromOriginDirtyStyleSheet(true);
  }

  return params;
}

StyleImage* CSSImageValue::CacheImage(
    const Document& document,
    FetchParameters::ImageRequestBehavior image_request_behavior,
    CrossOriginAttributeValue cross_origin,
    const float override_image_resolution) {
  if (!cached_image_) {
    if (url_data_.ResolvedUrl().empty()) {
      url_data_.ReResolveUrl(document);
    }

    FetchParameters params =
        PrepareFetch(document, image_request_behavior, cross_origin);
    cached_image_ = document.GetStyleEngine().CacheStyleImage(
        params, origin_clean_, is_ad_related_, override_image_resolution);
  }
  return cached_image_.Get();
}

void CSSImageValue::RestoreCachedResourceIfNeeded(
    const Document& document) const {
  if (!cached_image_ || !document.Fetcher() ||
      url_data_.ResolvedUrl().IsNull()) {
    return;
  }

  ImageResourceContent* cached_content = cached_image_->CachedImage();
  if (!cached_content) {
    return;
  }

  cached_content->EmulateLoadStartedForInspector(
      document.Fetcher(), KURL(url_data_.ResolvedUrl()),
      initiator_name_.empty() ? fetch_initiator_type_names::kCSS
                              : initiator_name_);
}

bool CSSImageValue::HasFailedOrCanceledSubresources() const {
  if (!cached_image_) {
    return false;
  }
  if (ImageResourceContent* cached_content = cached_image_->CachedImage()) {
    return cached_content->LoadFailedOrCanceled();
  }
  return true;
}

bool CSSImageValue::Equals(const CSSImageValue& other) const {
  return url_data_ == other.url_data_;
}

String CSSImageValue::CustomCSSText() const {
  return url_data_.CssText();
}

void CSSImageValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(cached_image_);
  visitor->Trace(proxy_svg_resource_client_);
  CSSValue::TraceAfterDispatch(visitor);
}

bool CSSImageValue::IsLocal(const Document& document) const {
  return url_data_.IsLocal(document);
}

CSSImageValue* CSSImageValue::ComputedCSSValueMaybeLocal() const {
  if (url_data_.UnresolvedUrl().StartsWith('#')) {
    return Clone();
  }
  return ComputedCSSValue();
}

ProxySVGResourceClient* CSSImageValue::GetSVGResourceClient() {
  if (!proxy_svg_resource_client_) {
    proxy_svg_resource_client_ =
        MakeGarbageCollected<ProxySVGResourceClient>(*this);
  }
  return proxy_svg_resource_client_.Get();
}

AtomicString CSSImageValue::NormalizedFragmentIdentifier() const {
  // Always use KURL's FragmentIdentifier to ensure that we're handling the
  // fragment in a consistent manner.
  return AtomicString(DecodeURLEscapeSequences(
      KURL(url_data_.ResolvedUrl()).FragmentIdentifier(),
      DecodeURLMode::kUTF8OrIsomorphic));
}

void CSSImageValue::ReResolveURL(const Document& document) const {
  if (url_data_.ReResolveUrl(document)) {
    cached_image_.Clear();
  }
}

}  // namespace blink
