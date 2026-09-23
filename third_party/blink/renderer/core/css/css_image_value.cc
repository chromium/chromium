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
#include "third_party/blink/renderer/core/svg/svg_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/cross_origin_attribute_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {
CSSImageValue::CSSImageValue(const CSSUrlData& url_data,
                             StyleImage* fetcher_agnostic_image)
    : CSSValue(kImageClass),
      url_data_(url_data),
      fetcher_agnostic_image_(fetcher_agnostic_image) {}

CSSImageValue::~CSSImageValue() = default;

CSSImageValue* CSSImageValue::Copy(const CSSImageValue& other,
                                   const CSSUrlData& url_data) {
  auto* result = MakeGarbageCollected<CSSImageValue>(url_data);
  result->fetcher_agnostic_image_ = other.fetcher_agnostic_image_;
  result->cached_image_ = other.cached_image_;
  result->cached_images_ = other.cached_images_;
  result->svg_resource_ = other.svg_resource_;
  result->svg_resources_ = other.svg_resources_;
  return result;
}

CSSImageValue* CSSImageValue::ComputedCSSValue() const {
  return Copy(*this, *UrlData().MakeComputed());
}

CSSImageValue* CSSImageValue::Clone() const {
  return Copy(*this, *UrlData().MakeWithoutReferrer());
}

FetchParameters CSSImageValue::PrepareFetch(
    const Document& document,
    CrossOriginAttributeValue cross_origin) const {
  const CSSUrlData& url_data = UrlData();
  const CSSUrlRequestModifiers& modifiers = url_data.GetModifiers();
  const Referrer& referrer = url_data.GetReferrer();
  ResourceRequest resource_request(
      url_data.ResolveUrl(*document.GetExecutionContext()));

  if (modifiers.referrer_policy) {
    resource_request.SetReferrerPolicy(*modifiers.referrer_policy);
  } else {
    resource_request.SetReferrerPolicy(
        ReferrerUtils::MojoReferrerPolicyResolveDefault(
            referrer.referrer_policy));
  }
  // The referrer URL in the Referrer object is the referrer before any
  // stripping due to the referrer policy. For external stylesheets this is the
  // stylesheet URL, for inline styles it is the document URL. It is correct to
  // set it regardless of whether the policy was overridden by a URL modifier;
  // the policy determines how this URL is transformed, not which URL is used.
  resource_request.SetReferrerString(referrer.referrer);

  if (url_data.IsAdRelated()) {
    resource_request.SetIsAdResource();
  }
  ExecutionContext* execution_context = document.GetExecutionContext();
  ResourceLoaderOptions options(execution_context->GetCurrentWorld());
  options.initiator_info.name = initiator_name_.empty()
                                    ? fetch_initiator_type_names::kCSS
                                    : initiator_name_;
  if (referrer.referrer != Referrer::ClientReferrerString()) {
    options.initiator_info.referrer = referrer.referrer;
  }
  FetchParameters params(std::move(resource_request), options);

  // URL modifier cross-origin overrides the property-level cross-origin.
  CrossOriginAttributeValue effective_cross_origin =
      modifiers.cross_origin != kCrossOriginAttributeNotSet
          ? modifiers.cross_origin
          : cross_origin;
  if (effective_cross_origin != kCrossOriginAttributeNotSet) {
    params.SetCrossOriginAccessControl(execution_context->GetSecurityOrigin(),
                                       effective_cross_origin);
  }

  if (!modifiers.integrity.IsNull()) {
    IntegrityMetadataSet metadata_set;
    SubresourceIntegrity::ParseIntegrityAttribute(
        modifiers.integrity, metadata_set, execution_context);
    params.SetIntegrityMetadata(metadata_set);
    params.MutableResourceRequest().SetFetchIntegrity(modifiers.integrity,
                                                      execution_context);
  }

  if (!url_data.IsFromOriginCleanStyleSheet()) {
    params.SetFromOriginDirtyStyleSheet(true);
  }

  return params;
}

bool CSSImageValue::IsCachePending(ResourceFetcher* fetcher) const {
  if (fetcher &&
      RuntimeEnabledFeatures::StyleResourceFetcherIdentityCheckEnabled()) {
    if (cached_images_.Contains(fetcher)) {
      return false;
    }
  } else if (cached_image_) {
    return false;
  }
  return !fetcher_agnostic_image_;
}

StyleImage* CSSImageValue::CachedImage(ResourceFetcher* fetcher) const {
  if (fetcher &&
      RuntimeEnabledFeatures::StyleResourceFetcherIdentityCheckEnabled()) {
    auto it = cached_images_.find(fetcher);
    if (it != cached_images_.end()) {
      return it->value.Get();
    }
  } else if (cached_image_) {
    return cached_image_.Get();
  }
  return fetcher_agnostic_image_.Get();
}

StyleImage* CSSImageValue::CacheImage(Document& document,
                                      CrossOriginAttributeValue cross_origin,
                                      const float override_image_resolution) {
  ResourceFetcher* fetcher = document.Fetcher();
  if (fetcher &&
      RuntimeEnabledFeatures::StyleResourceFetcherIdentityCheckEnabled()) {
    auto it = cached_images_.find(fetcher);
    if (it != cached_images_.end()) {
      return it->value.Get();
    }
  } else if (cached_image_) {
    return cached_image_.Get();
  }

  const CSSUrlData& url_data = UrlData();
  if (url_data.ResolvedUrl().empty()) {
    url_data.ReResolveUrl(document);
  }

  FetchParameters params = PrepareFetch(document, cross_origin);
  ImageResourceContent* image_content =
      document.GetStyleEngine().CacheImageContent(params);
  StyleImage* image = MakeGarbageCollected<StyleFetchedImage>(
      image_content, *url_data.MakeResolvedIfDanglingMarkup(document), document,
      params.Url(), override_image_resolution);

  if (fetcher &&
      RuntimeEnabledFeatures::StyleResourceFetcherIdentityCheckEnabled()) {
    cached_images_.Set(fetcher, image);
  } else {
    cached_image_ = image;
  }
  return image;
}

void CSSImageValue::RestoreCachedResourceIfNeeded(
    const Document& document) const {
  ResourceFetcher* fetcher = document.Fetcher();
  if (!fetcher || UrlData().ResolvedUrl().IsNull()) {
    return;
  }

  StyleImage* image = CachedImage(fetcher);
  if (!image) {
    return;
  }

  ImageResourceContent* cached_content = image->CachedImage();
  if (!cached_content) {
    return;
  }

  cached_content->EmulateLoadStartedForInspector(
      fetcher, initiator_name_.empty() ? fetch_initiator_type_names::kCSS
                                       : initiator_name_);
}

SVGResource* CSSImageValue::EnsureSVGResource(ResourceFetcher* fetcher) const {
  if (fetcher &&
      RuntimeEnabledFeatures::StyleResourceFetcherIdentityCheckEnabled()) {
    auto it = svg_resources_.find(fetcher);
    if (it != svg_resources_.end()) {
      return it->value.Get();
    }
  } else if (svg_resource_) {
    return svg_resource_.Get();
  }
  SVGResource* resource = MakeGarbageCollected<ExternalSVGResourceImageContent>(
      CachedImage(fetcher)->CachedImage(), NormalizedFragmentIdentifier());
  if (fetcher &&
      RuntimeEnabledFeatures::StyleResourceFetcherIdentityCheckEnabled()) {
    svg_resources_.Set(fetcher, resource);
  } else {
    svg_resource_ = resource;
  }
  return resource;
}

bool CSSImageValue::HasFailedOrCanceledSubresources(
    ResourceFetcher* fetcher) const {
  StyleImage* image = CachedImage(fetcher);
  if (!image) {
    return false;
  }
  ImageResourceContent* content = image->CachedImage();
  return !content || content->LoadFailedOrCanceled();
}

bool CSSImageValue::Equals(const CSSImageValue& other) const {
  return *url_data_ == *other.url_data_;
}

String CSSImageValue::CustomCSSText() const {
  return UrlData().CssText();
}

void CSSImageValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(url_data_);
  visitor->Trace(fetcher_agnostic_image_);
  visitor->Trace(cached_image_);
  visitor->Trace(cached_images_);
  visitor->Trace(svg_resource_);
  visitor->Trace(svg_resources_);
  CSSValue::TraceAfterDispatch(visitor);
}

bool CSSImageValue::IsLocal(const Document& document) const {
  return UrlData().IsLocal(document);
}

AtomicString CSSImageValue::NormalizedFragmentIdentifier() const {
  // Always use KURL's FragmentIdentifier to ensure that we're handling the
  // fragment in a consistent manner.
  return AtomicString(DecodeUrlEscapeSequences(
      KURL(UrlData().ResolvedUrl()).FragmentIdentifier(),
      DecodeUrlMode::kUtf8OrIsomorphic));
}

void CSSImageValue::ReResolveURL(const Document& document) const {
  if (UrlData().ReResolveUrl(document)) {
    cached_image_.Clear();
    cached_images_.clear();
    svg_resource_.Clear();
    svg_resources_.clear();
  }
}

}  // namespace blink
