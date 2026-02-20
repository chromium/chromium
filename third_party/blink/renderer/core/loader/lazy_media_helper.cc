// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/lazy_media_helper.h"

#include "third_party/blink/public/mojom/use_counter/metrics/webdx_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/loading_attribute.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/lazy_load_media_observer.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

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
void LazyMediaHelper::StartMonitoring(Element* element) {
  if (Document* document = GetRootDocumentOrNull(element)) {
    document->EnsureLazyLoadMediaObserver().StartMonitoringNearViewport(
        document, element);
  }
}

// static
void LazyMediaHelper::StopMonitoring(Element* element) {
  if (Document* document = GetRootDocumentOrNull(element)) {
    document->EnsureLazyLoadMediaObserver().StopMonitoring(element);
  }
}

// static
bool LazyMediaHelper::ShouldDeferMediaLoad(LocalFrame& frame,
                                           HTMLMediaElement* media_element) {
  // Do not lazyload media elements when JavaScript is disabled, regardless of
  // the `loading` attribute. IntersectionObserver requires JavaScript.
  if (!frame.DomWindow()->CanExecuteScripts(kNotAboutToExecuteScript)) {
    return false;
  }

  LoadingAttributeValue loading_attr = GetLoadingAttributeValue(
      media_element->FastGetAttribute(html_names::kLoadingAttr));

  // If loading=eager, don't defer.
  if (loading_attr == LoadingAttributeValue::kEager) {
    return false;
  }

  // Only defer if loading=lazy is explicitly set.
  if (loading_attr != LoadingAttributeValue::kLazy) {
    return false;
  }

  UseCounter::CountWebDXFeature(
      frame.GetDocument(), mojom::blink::WebDXFeature::kDRAFT_LoadingLazyMedia);

  return true;
}

}  // namespace blink
