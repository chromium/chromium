// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/lazy_image_helper.h"

#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
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

// Records |bytes| to |histogram_name| in kilobytes (i.e., bytes / 1024).
// https://almanac.httparchive.org/en/2022/page-weight#fig-12 reports the 90th
// percentile of jpeg images is 213KB with a max of ~64MB. The max bucket size
// has been set at 64MB to capture this range with as much granularity as
// possible.
#define IMAGE_BYTES_HISTOGRAM(histogram_name, bytes)                        \
  UMA_HISTOGRAM_CUSTOM_COUNTS(histogram_name,                               \
                              base::saturated_cast<int>((bytes) / 1024), 1, \
                              64 * 1024, 50)

Document* GetRootDocumentOrNull(Element* element) {
  if (LocalFrame* frame = element->GetDocument().GetFrame())
    return frame->LocalFrameRoot().GetDocument();
  return nullptr;
}

}  // namespace

// static
void LazyImageHelper::StartMonitoring(blink::Element* element) {
  if (Document* document = GetRootDocumentOrNull(element)) {
    document->EnsureLazyLoadImageObserver().StartMonitoringNearViewport(
        document, element);
  }
}

void LazyImageHelper::StopMonitoring(Element* element) {
  if (Document* document = GetRootDocumentOrNull(element)) {
    document->EnsureLazyLoadImageObserver().StopMonitoring(element);
  }
}

// static
bool LazyImageHelper::ShouldDeferImageLoad(LocalFrame& frame,
                                           HTMLImageElement* html_image) {
  // Do not lazyload image elements when JavaScript is disabled, regardless of
  // the `loading` attribute.
  if (!frame.DomWindow()->CanExecuteScripts(kNotAboutToExecuteScript)) {
    return false;
  }

  LoadingAttributeValue loading_attr = GetLoadingAttributeValue(
      html_image->FastGetAttribute(html_names::kLoadingAttr));
  if (loading_attr == LoadingAttributeValue::kEager) {
    UseCounter::Count(frame.GetDocument(),
                      WebFeature::kLazyLoadImageLoadingAttributeEager);
    return false;
  }

  if (loading_attr != LoadingAttributeValue::kLazy) {
    return false;
  }

  UseCounter::Count(frame.GetDocument(),
                    WebFeature::kLazyLoadImageLoadingAttributeLazy);
  if (frame.GetLazyLoadImageSetting() ==
      LocalFrame::LazyLoadImageSetting::kDisabled) {
    return false;
  }

  return true;
}

// static
void LazyImageHelper::StartMonitoringVisibilityMetrics(
    HTMLImageElement* html_image) {
  if (Document* root_document = GetRootDocumentOrNull(html_image)) {
    root_document->EnsureLazyLoadImageObserver().StartMonitoringVisibility(
        root_document, html_image);
  }
}

void LazyImageHelper::RecordMetricsOnLoadFinished(
    HTMLImageElement* image_element) {
  // TODO(pdr): We should only report metrics for images that were actually lazy
  // loaded, and checking the attribute alone is not sufficient. See:
  // `LazyImageHelper::ShouldDeferImageLoad`.
  if (!image_element->HasLazyLoadingAttribute()) {
    return;
  }

  Document* root_document = GetRootDocumentOrNull(image_element);
  if (!root_document) {
    return;
  }

  if (ImageResourceContent* content = image_element->CachedImage()) {
    int64_t response_size = content->GetResponse().EncodedDataLength();
    IMAGE_BYTES_HISTOGRAM("Blink.LazyLoadedImage.Size", response_size);
    if (!root_document->LoadEventFinished()) {
      IMAGE_BYTES_HISTOGRAM("Blink.LazyLoadedImageBeforeDocumentOnLoad.Size",
                            response_size);
    }
  }

  root_document->EnsureLazyLoadImageObserver().OnLoadFinished(image_element);
}

}  // namespace blink
