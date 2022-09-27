// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/lazy_image_helper.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/lazy_load_image_observer.h"
#include "third_party/blink/renderer/core/html/loading_attribute.h"

namespace blink {

namespace {

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

}  // namespace

// static
void LazyImageHelper::StartMonitoring(blink::Element* element) {
  Document* document = GetRootDocumentOrNull(element);
  if (!document)
    return;

  using DeferralMessage = LazyLoadImageObserver::DeferralMessage;
  auto deferral_message = DeferralMessage::kNone;
  if (auto* html_image = DynamicTo<HTMLImageElement>(element)) {
    LoadingAttributeValue effective_loading_attr = GetLoadingAttributeValue(
        html_image->FastGetAttribute(html_names::kLoadingAttr));
    DCHECK_NE(effective_loading_attr, LoadingAttributeValue::kEager);
    if (effective_loading_attr != LoadingAttributeValue::kAuto &&
        !IsDimensionAbsoluteLarge(*html_image)) {
      DCHECK_EQ(effective_loading_attr, LoadingAttributeValue::kLazy);
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
    LocalFrame& frame,
    HTMLImageElement* html_image,
    const KURL& url) {
  if (!url.ProtocolIsInHTTPFamily())
    return LazyImageHelper::Eligibility::kDisabled;

  // Do not lazyload image elements when JavaScript is disabled, regardless of
  // the `loading` attribute.
  if (!frame.DomWindow()->CanExecuteScripts(kNotAboutToExecuteScript))
    return LazyImageHelper::Eligibility::kDisabled;

  const auto lazy_load_image_setting = frame.GetLazyLoadImageSetting();
  LoadingAttributeValue loading_attr = GetLoadingAttributeValue(
      html_image->FastGetAttribute(html_names::kLoadingAttr));
  if (loading_attr == LoadingAttributeValue::kLazy) {
    StartMonitoringVisibility(html_image);
    UseCounter::Count(frame.GetDocument(),
                      WebFeature::kLazyLoadImageLoadingAttributeLazy);
    if (lazy_load_image_setting !=
        LocalFrame::LazyLoadImageSetting::kDisabled) {
      // Developer opt-in lazyload.
      return LazyImageHelper::Eligibility::kEnabledFullyDeferred;
    }
  }

  if (loading_attr == LoadingAttributeValue::kEager) {
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
