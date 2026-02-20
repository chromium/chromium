// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/lazy_load_media_observer.h"

#include <limits>

#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_check_visibility_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"

namespace blink {

namespace {

bool IsDescendantOrSameDocument(Document& subject, Document& root) {
  for (Document* doc = &subject; doc; doc = doc->ParentDocument()) {
    if (doc == root) {
      return true;
    }
  }
  return false;
}

}  // namespace

void LazyLoadMediaObserver::StartMonitoringNearViewport(Document* root_document,
                                                        Element* element) {
  if (!lazy_load_intersection_observer_) {
    int margin = GetLazyLoadingMarginPx(*root_document);
    IntersectionObserver::Params params = {
        .scroll_margin = {{/* top & bottom */ Length::Fixed(margin),
                           /* right & left */ Length::Fixed(margin / 2)}},
        .thresholds = {std::numeric_limits<float>::min()},
    };
    lazy_load_intersection_observer_ = IntersectionObserver::Create(
        *root_document,
        BindRepeating(&LazyLoadMediaObserver::LoadIfNearViewport,
                      WrapWeakPersistent(this)),
        LocalFrameUkmAggregator::kLazyLoadIntersectionObserver,
        std::move(params));
  }

  lazy_load_intersection_observer_->observe(element);
}

void LazyLoadMediaObserver::StopMonitoring(Element* element) {
  if (lazy_load_intersection_observer_) {
    lazy_load_intersection_observer_->unobserve(element);
  }
}

bool LazyLoadMediaObserver::LoadAllImagesAndBlockLoadEvent(
    Document& for_document) {
  if (!lazy_load_intersection_observer_) {
    return false;
  }
  bool resources_have_started_loading = false;
  HeapVector<Member<Element>> to_be_unobserved;
  for (const IntersectionObservation* observation :
       lazy_load_intersection_observer_->Observations()) {
    Element* element = observation->Target();
    if (!IsDescendantOrSameDocument(element->GetDocument(), for_document)) {
      continue;
    }
    if (auto* image_element = DynamicTo<HTMLImageElement>(element)) {
      const_cast<HTMLImageElement*>(image_element)
          ->LoadDeferredImageBlockingLoad();
      resources_have_started_loading = true;
    }
    to_be_unobserved.push_back(element);
  }
  for (Element* element : to_be_unobserved) {
    lazy_load_intersection_observer_->unobserve(element);
  }
  return resources_have_started_loading;
}

void LazyLoadMediaObserver::LoadIfNearViewport(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  DCHECK(!entries.empty());

  for (auto entry : entries) {
    Element* element = entry->target();
    auto* image_element = DynamicTo<HTMLImageElement>(element);

    // For images: if the loading_attr is 'lazy' explicitly, we'd better to
    // wait for intersection.
    if (!entry->isIntersecting() && image_element &&
        !image_element->HasLazyLoadingAttribute()) {
      // Fully load the invisible image elements. The elements can be invisible
      // by style such as display:none, visibility: hidden, or hidden via
      // attribute, etc. Style might also not be calculated if the ancestors
      // were invisible.
      const ComputedStyle* style = entry->target()->GetComputedStyle();
      if (!style || style->Visibility() != EVisibility::kVisible ||
          style->Display() == EDisplay::kNone) {
        // Check that style was null because it was not computed since the
        // element was in an invisible subtree.
        DCHECK(style || !element->checkVisibility(
                            MakeGarbageCollected<CheckVisibilityOptions>()));
        image_element->LoadDeferredImageFromMicrotask();
        lazy_load_intersection_observer_->unobserve(element);
      }
    }
    if (!entry->isIntersecting()) {
      continue;
    }

    // Handle image elements.
    if (image_element) {
      image_element->LoadDeferredImageFromMicrotask();
    }

    // Handle video and audio elements.
    if (auto* media_element = DynamicTo<HTMLMediaElement>(element)) {
      media_element->LoadDeferredMediaIfNeeded();
    }

    lazy_load_intersection_observer_->unobserve(element);
  }
}

void LazyLoadMediaObserver::Trace(Visitor* visitor) const {
  visitor->Trace(lazy_load_intersection_observer_);
}

int LazyLoadMediaObserver::GetLazyLoadingMarginPx(const Document& document) {
  const Settings* settings = document.GetSettings();
  if (!settings) {
    return 0;
  }

  switch (GetNetworkStateNotifier().EffectiveType()) {
    case WebEffectiveConnectionType::kTypeUnknown:
      return settings->GetLazyLoadingImageMarginPxUnknown();
    case WebEffectiveConnectionType::kTypeOffline:
      return settings->GetLazyLoadingImageMarginPxOffline();
    case WebEffectiveConnectionType::kTypeSlow2G:
      return settings->GetLazyLoadingImageMarginPxSlow2G();
    case WebEffectiveConnectionType::kType2G:
      return settings->GetLazyLoadingImageMarginPx2G();
    case WebEffectiveConnectionType::kType3G:
      return settings->GetLazyLoadingImageMarginPx3G();
    case WebEffectiveConnectionType::kType4G:
      return settings->GetLazyLoadingImageMarginPx4G();
    default:
      NOTREACHED();
  }
}

}  // namespace blink
