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

Document* GetRootDocumentOrNull(Node* node) {
  if (LocalFrame* frame = node->GetDocument().GetFrame()) {
    return frame->LocalFrameRoot().GetDocument();
  }
  return nullptr;
}

}  // namespace

// static
void LazyImageHelper::StartMonitoring(Element* element) {
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
bool LazyImageHelper::LoadAllImagesAndBlockLoadEvent(Document& document) {
  if (Document* root_document = GetRootDocumentOrNull(&document)) {
    return root_document->EnsureLazyLoadImageObserver()
        .LoadAllImagesAndBlockLoadEvent(document);
  }
  return false;
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

}  // namespace blink
