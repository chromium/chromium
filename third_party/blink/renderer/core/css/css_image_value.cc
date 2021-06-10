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
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

CSSImageValue::CSSImageValue(const AtomicString& raw_value,
                             const KURL& url,
                             const Referrer& referrer,
                             OriginClean origin_clean,
                             bool is_ad_related,
                             StyleImage* image)
    : CSSValue(kImageClass),
      relative_url_(raw_value),
      referrer_(referrer),
      absolute_url_(url.GetString()),
      cached_image_(image),
      origin_clean_(origin_clean),
      is_ad_related_(is_ad_related) {}

CSSImageValue::~CSSImageValue() = default;

FetchParameters CSSImageValue::PrepareFetch(
    const Document& document,
    FetchParameters::ImageRequestBehavior image_request_behavior,
    CrossOriginAttributeValue cross_origin) const {
  ResourceRequest resource_request(absolute_url_);
  resource_request.SetReferrerPolicy(
      ReferrerUtils::MojoReferrerPolicyResolveDefault(
          referrer_.referrer_policy));
  resource_request.SetReferrerString(referrer_.referrer);
  if (is_ad_related_)
    resource_request.SetIsAdResource();
  ExecutionContext* execution_context = document.GetExecutionContext();
  ResourceLoaderOptions options(execution_context->GetCurrentWorld());
  options.initiator_info.name = initiator_name_.IsEmpty()
                                    ? fetch_initiator_type_names::kCSS
                                    : initiator_name_;
  if (referrer_.referrer != Referrer::ClientReferrerString())
    options.initiator_info.referrer = referrer_.referrer;
  FetchParameters params(std::move(resource_request), options);

  if (cross_origin != kCrossOriginAttributeNotSet) {
    params.SetCrossOriginAccessControl(execution_context->GetSecurityOrigin(),
                                       cross_origin);
  }

  bool is_lazily_loaded =
      image_request_behavior == FetchParameters::kDeferImageLoad &&
      // Only http/https images are eligible to be lazily loaded.
      params.Url().ProtocolIsInHTTPFamily();
  if (is_lazily_loaded) {
    if (document.GetFrame() && document.GetFrame()->Client()) {
      document.GetFrame()->Client()->DidObserveLazyLoadBehavior(
          WebLocalFrameClient::LazyLoadBehavior::kDeferredImage);
    }
    params.SetLazyImageDeferred();
  }

  if (base::FeatureList::IsEnabled(blink::features::kSubresourceRedirect) &&
      params.Url().ProtocolIsInHTTPFamily() &&
      GetNetworkStateNotifier().SaveDataEnabled()) {
    auto& subresource_request = params.MutableResourceRequest();
    subresource_request.SetPreviewsState(
        subresource_request.GetPreviewsState() |
        PreviewsTypes::kSubresourceRedirectOn);
  }

  if (origin_clean_ != OriginClean::kTrue)
    params.SetFromOriginDirtyStyleSheet(true);

  return params;
}

StyleImage* CSSImageValue::CacheImage(
    const Document& document,
    FetchParameters::ImageRequestBehavior image_request_behavior,
    CrossOriginAttributeValue cross_origin) {
  if (!cached_image_) {
    if (absolute_url_.IsEmpty())
      ReResolveURL(document);

    FetchParameters params =
        PrepareFetch(document, image_request_behavior, cross_origin);
    cached_image_ = MakeGarbageCollected<StyleFetchedImage>(
        ImageResourceContent::Fetch(params, document.Fetcher()), document,
        params.GetImageRequestBehavior() == FetchParameters::kDeferImageLoad,
        origin_clean_ == OriginClean::kTrue, is_ad_related_, params.Url());
  }
  return cached_image_.Get();
}

void CSSImageValue::RestoreCachedResourceIfNeeded(
    const Document& document) const {
  if (!cached_image_ || !document.Fetcher() || absolute_url_.IsNull())
    return;

  ImageResourceContent* cached_content = cached_image_->CachedImage();
  if (!cached_content)
    return;

  cached_content->EmulateLoadStartedForInspector(
      document.Fetcher(), KURL(absolute_url_),
      initiator_name_.IsEmpty() ? fetch_initiator_type_names::kCSS
                                : initiator_name_);
}

bool CSSImageValue::HasFailedOrCanceledSubresources() const {
  if (!cached_image_)
    return false;
  if (ImageResourceContent* cached_content = cached_image_->CachedImage())
    return cached_content->LoadFailedOrCanceled();
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

void CSSImageValue::TraceAfterDispatch(blink::Visitor* visitor) const {
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
