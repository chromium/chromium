// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/lazy_image_helper.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/lazy_load_image_observer.h"

namespace blink {

namespace {

enum class LoadingAttrValue { kAuto, kLazy, kEager };

LoadingAttrValue GetLoadingAttrValue(const HTMLImageElement& html_image) {
  const auto& attribute_value =
      html_image.FastGetAttribute(html_names::kLoadingAttr);
  return EqualIgnoringASCIICase(attribute_value, "eager")
             ? LoadingAttrValue::kEager
             : EqualIgnoringASCIICase(attribute_value, "lazy")
                   ? LoadingAttrValue::kLazy
                   : LoadingAttrValue::kAuto;
}

// Returns true if absolute dimension is specified in the width and height
// attributes or in the inline style.
bool IsDimensionAbsoluteLarge(const HTMLImageElement& html_image) {
  if (HTMLImageElement::GetAttributeLazyLoadDimensionType(
          html_image.FastGetAttribute(html_names::kWidthAttr)) ==
          HTMLImageElement::LazyLoadDimensionType::kAbsoluteNotSmall ||
      HTMLImageElement::GetAttributeLazyLoadDimensionType(
          html_image.FastGetAttribute(html_names::kHeightAttr)) ==
          HTMLImageElement::LazyLoadDimensionType::kAbsoluteNotSmall) {
    return true;
  }
  if (HTMLImageElement::GetInlineStyleDimensionsType(
          html_image.InlineStyle()) ==
      HTMLImageElement::LazyLoadDimensionType::kAbsoluteNotSmall) {
    return true;
  }
  return false;
}

Document* GetRootDocumentOrNull(Element* element) {
  if (LocalFrame* frame = element->GetDocument().GetFrame())
    return frame->LocalFrameRoot().GetDocument();
  return nullptr;
}

void StartMonitoringVisibility(HTMLImageElement* html_image) {
  Document* document = GetRootDocumentOrNull(html_image);
  if (document &&
      RuntimeEnabledFeatures::LazyImageVisibleLoadTimeMetricsEnabled()) {
    document->EnsureLazyLoadImageObserver().StartMonitoringVisibility(
        document, html_image);
  }
}

bool IsFullyLoadableFirstKImageAndDecrementCount(
    HTMLImageElement* image_element) {
  Document* document = GetRootDocumentOrNull(image_element);
  if (!document)
    return true;
  return document->EnsureLazyLoadImageObserver()
      .IsFullyLoadableFirstKImageAndDecrementCount();
}

}  // namespace

// static
void LazyImageHelper::StartMonitoring(blink::Element* element) {
  Document* document = GetRootDocumentOrNull(element);
  if (!document)
    return;

  using DeferralMessage = LazyLoadImageObserver::DeferralMessage;
  auto deferral_message = DeferralMessage::kNone;
  if (auto* html_image = ToHTMLImageElementOrNull(element)) {
    LoadingAttrValue loading_attr = GetLoadingAttrValue(*html_image);
    DCHECK_NE(loading_attr, LoadingAttrValue::kEager);
    if (loading_attr == LoadingAttrValue::kAuto) {
      deferral_message = DeferralMessage::kLoadEventsDeferred;
    } else if (!IsDimensionAbsoluteLarge(*html_image)) {
      DCHECK_EQ(loading_attr, LoadingAttrValue::kLazy);
      deferral_message = DeferralMessage::kMissingDimensionForLazy;
    }
  }
  document->EnsureLazyLoadImageObserver().StartMonitoringNearViewport(
      document, element, deferral_message);
}

void LazyImageHelper::StopMonitoring(Element* element) {
  if (Document* document = GetRootDocumentOrNull(element)) {
    document->EnsureLazyLoadImageObserver().StopMonitoring(element);
  }
}

// static
LazyImageHelper::Eligibility
LazyImageHelper::DetermineEligibilityAndTrackVisibilityMetrics(
    const LocalFrame& frame,
    HTMLImageElement* html_image,
    const KURL& url) {
  if (!url.ProtocolIsInHTTPFamily())
    return LazyImageHelper::Eligibility::kDisabled;

  const auto lazy_load_image_setting = frame.GetLazyLoadImageSetting();
  LoadingAttrValue loading_attr = GetLoadingAttrValue(*html_image);
  bool is_fully_loadable =
      IsFullyLoadableFirstKImageAndDecrementCount(html_image);
  if (loading_attr == LoadingAttrValue::kLazy) {
    StartMonitoringVisibility(html_image);
    UseCounter::Count(frame.GetDocument(),
                      WebFeature::kLazyLoadImageLoadingAttributeLazy);
    if (lazy_load_image_setting !=
        LocalFrame::LazyLoadImageSetting::kDisabled) {
      // Developer opt-in lazyload.
      if (!RuntimeEnabledFeatures::LazyImageLoadingMetadataFetchEnabled() ||
          IsDimensionAbsoluteLarge(*html_image)) {
        return LazyImageHelper::Eligibility::kEnabledFullyDeferred;
      }
      return LazyImageHelper::Eligibility::kEnabledFetchPlaceholder;
    }
  }

  if (loading_attr == LoadingAttrValue::kEager &&
      !frame.GetDocument()->IsLazyLoadPolicyEnforced()) {
    UseCounter::Count(frame.GetDocument(),
                      WebFeature::kLazyLoadImageLoadingAttributeEager);
    return LazyImageHelper::Eligibility::kDisabled;
  }

  // Do not lazyload image elements created from javascript.
  if (!html_image->ElementCreatedByParser())
    return LazyImageHelper::Eligibility::kDisabled;

  if (frame.Owner() && !frame.Owner()->ShouldLazyLoadChildren())
    return LazyImageHelper::Eligibility::kDisabled;

  // Avoid automatically lazyloading if width and height attributes are small.
  // This heuristic helps avoid double fetching tracking pixels.
  if (HTMLImageElement::GetAttributeLazyLoadDimensionType(
          html_image->FastGetAttribute(html_names::kWidthAttr)) ==
          HTMLImageElement::LazyLoadDimensionType::kAbsoluteSmall &&
      HTMLImageElement::GetAttributeLazyLoadDimensionType(
          html_image->FastGetAttribute(html_names::kHeightAttr)) ==
          HTMLImageElement::LazyLoadDimensionType::kAbsoluteSmall) {
    return LazyImageHelper::Eligibility::kDisabled;
  }
  // Avoid automatically lazyloading if width or height is specified in inline
  // style and is small enough. This heuristic helps avoid double fetching
  // tracking pixels.
  if (HTMLImageElement::GetInlineStyleDimensionsType(
          html_image->InlineStyle()) ==
      HTMLImageElement::LazyLoadDimensionType::kAbsoluteSmall) {
    return LazyImageHelper::Eligibility::kDisabled;
  }

  StartMonitoringVisibility(html_image);

  if (!is_fully_loadable &&
      lazy_load_image_setting ==
          LocalFrame::LazyLoadImageSetting::kEnabledAutomatic) {
    // Automatic lazyload
    if (!RuntimeEnabledFeatures::LazyImageLoadingMetadataFetchEnabled() ||
        IsDimensionAbsoluteLarge(*html_image)) {
      return LazyImageHelper::Eligibility::kEnabledFullyDeferred;
    }
    return LazyImageHelper::Eligibility::kEnabledFetchPlaceholder;
  }
  return LazyImageHelper::Eligibility::kDisabled;
}

void LazyImageHelper::RecordMetricsOnLoadFinished(
    HTMLImageElement* image_element) {
  if (!RuntimeEnabledFeatures::LazyImageVisibleLoadTimeMetricsEnabled())
    return;
  if (Document* document = GetRootDocumentOrNull(image_element)) {
    document->EnsureLazyLoadImageObserver().OnLoadFinished(image_element);
  }
}

}  // namespace blink
