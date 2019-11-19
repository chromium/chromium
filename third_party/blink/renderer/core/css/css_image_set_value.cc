/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/css_image_set_value.h"

#include <algorithm>
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/style/style_fetched_image_set.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSImageSetValue::CSSImageSetValue(CSSParserMode parser_mode)
    : CSSValueList(kImageSetClass, kCommaSeparator),
      cached_scale_factor_(1),
      parser_mode_(parser_mode) {}

CSSImageSetValue::~CSSImageSetValue() = default;

void CSSImageSetValue::FillImageSet() {
  wtf_size_t length = this->length();
  wtf_size_t i = 0;
  while (i < length) {
    const auto& image_value = To<CSSImageValue>(Item(i));
    String image_url = image_value.Url();

    ++i;
    SECURITY_DCHECK(i < length);
    const CSSValue& scale_factor_value = Item(i);
    float scale_factor =
        To<CSSPrimitiveValue>(scale_factor_value).GetFloatValue();

    ImageWithScale image;
    image.image_url = image_url;
    image.referrer.referrer = image_value.GetReferrer().referrer;
    image.referrer.referrer_policy = image_value.GetReferrer().referrer_policy;
    image.scale_factor = scale_factor;
    images_in_set_.push_back(image);
    ++i;
  }

  // Sort the images so that they are stored in order from lowest resolution to
  // highest.
  std::sort(images_in_set_.begin(), images_in_set_.end(),
            CSSImageSetValue::CompareByScaleFactor);
}

CSSImageSetValue::ImageWithScale CSSImageSetValue::BestImageForScaleFactor(
    float scale_factor) {
  ImageWithScale image;
  wtf_size_t number_of_images = images_in_set_.size();
  for (wtf_size_t i = 0; i < number_of_images; ++i) {
    image = images_in_set_.at(i);
    if (image.scale_factor >= scale_factor)
      return image;
  }
  return image;
}

bool CSSImageSetValue::IsCachePending(float device_scale_factor) const {
  return !cached_image_ || device_scale_factor != cached_scale_factor_;
}

StyleImage* CSSImageSetValue::CachedImage(float device_scale_factor) const {
  DCHECK(!IsCachePending(device_scale_factor));
  return cached_image_.Get();
}

StyleImage* CSSImageSetValue::CacheImage(
    const Document& document,
    float device_scale_factor,
    FetchParameters::ImageRequestOptimization image_request_optimization,
    CrossOriginAttributeValue cross_origin) {
  if (!images_in_set_.size())
    FillImageSet();

  if (IsCachePending(device_scale_factor)) {
    // FIXME: In the future, we want to take much more than deviceScaleFactor
    // into acount here. All forms of scale should be included:
    // Page::PageScaleFactor(), LocalFrame::PageZoomFactor(), and any CSS
    // transforms. https://bugs.webkit.org/show_bug.cgi?id=81698
    ImageWithScale image = BestImageForScaleFactor(device_scale_factor);
    ResourceRequest resource_request(document.CompleteURL(image.image_url));
    resource_request.SetReferrerPolicy(
        ReferrerPolicyResolveDefault(image.referrer.referrer_policy));
    resource_request.SetReferrerString(image.referrer.referrer);
    ResourceLoaderOptions options;
    options.initiator_info.name = parser_mode_ == kUASheetMode
                                      ? fetch_initiator_type_names::kUacss
                                      : fetch_initiator_type_names::kCSS;
    FetchParameters params(resource_request, options);

    if (cross_origin != kCrossOriginAttributeNotSet) {
      params.SetCrossOriginAccessControl(document.GetSecurityOrigin(),
                                         cross_origin);
    }

    cached_image_ = MakeGarbageCollected<StyleFetchedImageSet>(
        ImageResourceContent::Fetch(params, document.Fetcher()),
        image.scale_factor, this, params.Url());
    cached_scale_factor_ = device_scale_factor;
  }

  return cached_image_.Get();
}

String CSSImageSetValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("-webkit-image-set(");

  wtf_size_t length = this->length();
  wtf_size_t i = 0;
  while (i < length) {
    if (i > 0)
      result.Append(", ");

    const CSSValue& image_value = Item(i);
    result.Append(image_value.CssText());
    result.Append(' ');

    ++i;
    SECURITY_DCHECK(i < length);
    const CSSValue& scale_factor_value = Item(i);
    result.Append(scale_factor_value.CssText());
    // FIXME: Eventually the scale factor should contain it's own unit
    // http://wkb.ug/100120.
    // For now 'x' is hard-coded in the parser, so we hard-code it here too.
    result.Append('x');

    ++i;
  }

  result.Append(')');
  return result.ToString();
}

bool CSSImageSetValue::HasFailedOrCanceledSubresources() const {
  if (!cached_image_)
    return false;
  if (ImageResourceContent* cached_resource = cached_image_->CachedImage())
    return cached_resource->LoadFailedOrCanceled();
  return true;
}

void CSSImageSetValue::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(cached_image_);
  CSSValueList::TraceAfterDispatch(visitor);
}

CSSImageSetValue* CSSImageSetValue::ValueWithURLsMadeAbsolute() {
  auto* value = MakeGarbageCollected<CSSImageSetValue>(parser_mode_);
  for (auto& item : *this) {
    auto* image_value = DynamicTo<CSSImageValue>(item.Get());
    image_value ? value->Append(*image_value->ValueWithURLMadeAbsolute())
                : value->Append(*item);
  }
  return value;
}

}  // namespace blink
