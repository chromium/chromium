// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/lazy_load_image_observer.h"

#include <limits>

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"

namespace blink {

namespace {

int GetLazyImageLoadingViewportDistanceThresholdPx(const Document& document) {
  const Settings* settings = document.GetSettings();
  if (!settings)
    return 0;

  switch (GetNetworkStateNotifier().EffectiveType()) {
    case WebEffectiveConnectionType::kTypeUnknown:
      return settings->GetLazyImageLoadingDistanceThresholdPxUnknown();
    case WebEffectiveConnectionType::kTypeOffline:
      return settings->GetLazyImageLoadingDistanceThresholdPxOffline();
    case WebEffectiveConnectionType::kTypeSlow2G:
      return settings->GetLazyImageLoadingDistanceThresholdPxSlow2G();
    case WebEffectiveConnectionType::kType2G:
      return settings->GetLazyImageLoadingDistanceThresholdPx2G();
    case WebEffectiveConnectionType::kType3G:
      return settings->GetLazyImageLoadingDistanceThresholdPx3G();
    case WebEffectiveConnectionType::kType4G:
      return settings->GetLazyImageLoadingDistanceThresholdPx4G();
  }
  NOTREACHED();
  return 0;
}

// Returns if the element or its ancestors are invisible, due to their style or
// attribute or due to themselves not connected to the main document tree.
bool IsElementInInvisibleSubTree(const Element& element) {
  if (!element.isConnected())
    return true;
  for (Node& ancestor : FlatTreeTraversal::InclusiveAncestorsOf(element)) {
    auto* ancestor_element = DynamicTo<Element>(ancestor);
    if (!ancestor_element)
      continue;
    // Return true if the whole frame is not rendered.
    if (ancestor.IsHTMLElement() && !ancestor.GetLayoutObject())
      return true;
    const ComputedStyle* style = ancestor_element->EnsureComputedStyle();
    if (style && (style->Visibility() != EVisibility::kVisible ||
                  style->Display() == EDisplay::kNone)) {
      return true;
    }
  }
  return false;
}

void RecordVisibleLoadTimeForImage(
    const LazyLoadImageObserver::VisibleLoadTimeMetrics&
        visible_load_time_metrics) {
  DCHECK(visible_load_time_metrics.has_initial_intersection_been_set);
  DCHECK(!visible_load_time_metrics.time_when_first_visible.is_null());
  DCHECK(!visible_load_time_metrics.time_when_first_load_finished.is_null());

  base::TimeDelta visible_load_delay =
      visible_load_time_metrics.time_when_first_load_finished -
      visible_load_time_metrics.time_when_first_visible;
  if (visible_load_delay.is_negative())
    visible_load_delay = base::TimeDelta();

  switch (GetNetworkStateNotifier().EffectiveType()) {
    case WebEffectiveConnectionType::kTypeSlow2G:
      if (visible_load_time_metrics.is_initially_intersecting) {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold.Slow2G",
            visible_load_delay);
      } else {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.Slow2G",
            visible_load_delay);
      }
      break;

    case WebEffectiveConnectionType::kType2G:
      if (visible_load_time_metrics.is_initially_intersecting) {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold.2G",
            visible_load_delay);
      } else {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.2G",
            visible_load_delay);
      }
      break;

    case WebEffectiveConnectionType::kType3G:
      if (visible_load_time_metrics.is_initially_intersecting) {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold.3G",
            visible_load_delay);
      } else {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.3G",
            visible_load_delay);
      }
      break;

    case WebEffectiveConnectionType::kType4G:
      if (visible_load_time_metrics.is_initially_intersecting) {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold.4G",
            visible_load_delay);
      } else {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.4G",
            visible_load_delay);
      }
      break;

    case WebEffectiveConnectionType::kTypeUnknown:
    case WebEffectiveConnectionType::kTypeOffline:
      // No VisibleLoadTime histograms are recorded for these effective
      // connection types.
      break;
  }
}

}  // namespace

LazyLoadImageObserver::LazyLoadImageObserver(const Document& document) {}

void LazyLoadImageObserver::StartMonitoringNearViewport(
    Document* root_document,
    Element* element,
    DeferralMessage deferral_message) {
  DCHECK(RuntimeEnabledFeatures::LazyImageLoadingEnabled());

  if (!lazy_load_intersection_observer_) {
    lazy_load_intersection_observer_ = IntersectionObserver::Create(
        {Length::Fixed(
            GetLazyImageLoadingViewportDistanceThresholdPx(*root_document))},
        {std::numeric_limits<float>::min()}, root_document,
        WTF::BindRepeating(&LazyLoadImageObserver::LoadIfNearViewport,
                           WrapWeakPersistent(this)),
        LocalFrameUkmAggregator::kLazyLoadIntersectionObserver);
  }
  lazy_load_intersection_observer_->observe(element);

  if (deferral_message == DeferralMessage::kMissingDimensionForLazy) {
    UseCounter::Count(root_document,
                      WebFeature::kLazyLoadImageMissingDimensionsForLazy);
  }
}

void LazyLoadImageObserver::StopMonitoring(Element* element) {
  if (lazy_load_intersection_observer_) {
    lazy_load_intersection_observer_->unobserve(element);
  }
}

bool LazyLoadImageObserver::LoadAllImagesAndBlockLoadEvent() {
  if (!lazy_load_intersection_observer_) {
    return false;
  }
  bool resources_have_started_loading = false;
  HeapVector<Member<Element>> to_be_unobserved;
  for (const IntersectionObservation* observation :
       lazy_load_intersection_observer_->Observations()) {
    Element* element = observation->Target();
    if (auto* image_element = DynamicTo<HTMLImageElement>(element)) {
      const_cast<HTMLImageElement*>(image_element)
          ->LoadDeferredImageBlockingLoad();
      resources_have_started_loading = true;
    }
    if (const ComputedStyle* style = element->GetComputedStyle()) {
      style->LoadDeferredImages(element->GetDocument());
      resources_have_started_loading = true;
    }
    to_be_unobserved.push_back(element);
  }
  for (Element* element : to_be_unobserved)
    lazy_load_intersection_observer_->unobserve(element);
  return resources_have_started_loading;
}

void LazyLoadImageObserver::LoadIfNearViewport(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  DCHECK(!entries.empty());

  for (auto entry : entries) {
    Element* element = entry->target();
    auto* image_element = DynamicTo<HTMLImageElement>(element);
    // If the loading_attr is 'lazy' explicitly, we'd better to wait for
    // intersection.
    if (!entry->isIntersecting() && image_element &&
        !EqualIgnoringASCIICase(image_element->FastGetAttribute(html_names::kLoadingAttr), "lazy")) {
      // Fully load the invisible image elements. The elements can be invisible
      // by style such as display:none, visibility: hidden, or hidden via
      // attribute, etc. Style might also not be calculated if the ancestors
      // were invisible.
      const ComputedStyle* style = entry->target()->GetComputedStyle();
      if (!style || style->Visibility() != EVisibility::kVisible ||
          style->Display() == EDisplay::kNone) {
        // Check that style was null because it was not computed since the
        // element was in an invisible subtree.
        DCHECK(style || IsElementInInvisibleSubTree(*element));
        image_element->LoadDeferredImageFromMicrotask();
        lazy_load_intersection_observer_->unobserve(element);
      }
    }
    if (!entry->isIntersecting())
      continue;
    if (image_element)
      image_element->LoadDeferredImageFromMicrotask();

    // Load the background image if the element has one deferred.
    if (const ComputedStyle* style = element->GetComputedStyle())
      style->LoadDeferredImages(element->GetDocument());

    lazy_load_intersection_observer_->unobserve(element);
  }
}

void LazyLoadImageObserver::StartMonitoringVisibility(
    Document* root_document,
    HTMLImageElement* image_element) {
  DCHECK(RuntimeEnabledFeatures::LazyImageVisibleLoadTimeMetricsEnabled());

  VisibleLoadTimeMetrics& visible_load_time_metrics =
      image_element->EnsureVisibleLoadTimeMetrics();
  if (!visible_load_time_metrics.time_when_first_visible.is_null()) {
    // The time when the image first became visible has already been measured,
    // so there's no need to monitor the visibility of the image any more.
    return;
  }
  if (!visibility_metrics_observer_) {
    visibility_metrics_observer_ = IntersectionObserver::Create(
        {}, {std::numeric_limits<float>::min()}, root_document,
        WTF::BindRepeating(&LazyLoadImageObserver::OnVisibilityChanged,
                           WrapWeakPersistent(this)),
        LocalFrameUkmAggregator::kLazyLoadIntersectionObserver);
  }
  visibility_metrics_observer_->observe(image_element);
}

void LazyLoadImageObserver::OnLoadFinished(HTMLImageElement* image_element) {
  DCHECK(RuntimeEnabledFeatures::LazyImageVisibleLoadTimeMetricsEnabled());
  VisibleLoadTimeMetrics& visible_load_time_metrics =
      image_element->EnsureVisibleLoadTimeMetrics();

  if (!visible_load_time_metrics.time_when_first_load_finished.is_null())
    return;
  visible_load_time_metrics.time_when_first_load_finished =
      base::TimeTicks::Now();

  if (visible_load_time_metrics.time_when_first_visible.is_null())
    return;

  RecordVisibleLoadTimeForImage(visible_load_time_metrics);
}

void LazyLoadImageObserver::OnVisibilityChanged(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  DCHECK(!entries.empty());

  for (auto entry : entries) {
    auto* image_element = DynamicTo<HTMLImageElement>(entry->target());
    if (!image_element)
      continue;

    VisibleLoadTimeMetrics& visible_load_time_metrics =
        image_element->EnsureVisibleLoadTimeMetrics();
    // The image's visiblity shouldn't still be monitored if the time when the
    // image first became visible has already been measured.
    DCHECK(visible_load_time_metrics.time_when_first_visible.is_null());

    if (!visible_load_time_metrics.has_initial_intersection_been_set) {
      visible_load_time_metrics.has_initial_intersection_been_set = true;
      visible_load_time_metrics.is_initially_intersecting =
          entry->isIntersecting();
    }
    if (!entry->isIntersecting())
      continue;

    visible_load_time_metrics.time_when_first_visible = base::TimeTicks::Now();
    if (visible_load_time_metrics.time_when_first_load_finished.is_null()) {
      // Note: If the WebEffectiveConnectionType enum ever gets out of sync
      // with mojom::blink::EffectiveConnectionType, then both the AboveTheFold
      // and BelowTheFold histograms here will have to be updated to record the
      // sample in terms of mojom::blink::EffectiveConnectionType instead of
      // WebEffectiveConnectionType.
      if (visible_load_time_metrics.is_initially_intersecting) {
        UMA_HISTOGRAM_ENUMERATION(
            "Blink.VisibleBeforeLoaded.LazyLoadImages.AboveTheFold",
            GetNetworkStateNotifier().EffectiveType());
      } else {
        UMA_HISTOGRAM_ENUMERATION(
            "Blink.VisibleBeforeLoaded.LazyLoadImages.BelowTheFold",
            GetNetworkStateNotifier().EffectiveType());
      }
    } else {
      RecordVisibleLoadTimeForImage(visible_load_time_metrics);
    }

    visibility_metrics_observer_->unobserve(image_element);
  }
}

void LazyLoadImageObserver::Trace(Visitor* visitor) const {
  visitor->Trace(lazy_load_intersection_observer_);
  visitor->Trace(visibility_metrics_observer_);
}

}  // namespace blink
