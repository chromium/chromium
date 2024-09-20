// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/lazy_load_image_observer.h"

#include <limits>

#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

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
    if (style && (style->UsedVisibility() != EVisibility::kVisible ||
                  style->Display() == EDisplay::kNone)) {
      return true;
    }
  }
  return false;
}

bool IsDescendantOrSameDocument(Document& subject, Document& root) {
  for (Document* doc = &subject; doc; doc = doc->ParentDocument()) {
    if (doc == root) {
      return true;
    }
  }
  return false;
}

}  // namespace

void LazyLoadImageObserver::StartMonitoringNearViewport(Document* root_document,
                                                        Element* element) {
  if (!lazy_load_intersection_observer_) {
    int margin = GetLazyLoadingImageMarginPx(*root_document);
    IntersectionObserver::Params params = {
        .thresholds = {std::numeric_limits<float>::min()},
    };
    if (RuntimeEnabledFeatures::LazyLoadScrollMarginEnabled()) {
      params.scroll_margin = {{/* top & bottom */ Length::Fixed(margin),
                               /* right & left */ Length::Fixed(margin / 2)}};
    } else {
      params.margin = {Length::Fixed(margin)};
    }
    lazy_load_intersection_observer_ = IntersectionObserver::Create(
        *root_document,
        WTF::BindRepeating(&LazyLoadImageObserver::LoadIfNearViewport,
                           WrapWeakPersistent(this)),
        LocalFrameUkmAggregator::kLazyLoadIntersectionObserver,
        std::move(params));
  }

  lazy_load_intersection_observer_->observe(element);
}

void LazyLoadImageObserver::StopMonitoring(Element* element) {
  if (lazy_load_intersection_observer_) {
    lazy_load_intersection_observer_->unobserve(element);
  }
}

bool LazyLoadImageObserver::LoadAllImagesAndBlockLoadEvent(
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
        !image_element->HasLazyLoadingAttribute()) {
      // Fully load the invisible image elements. The elements can be invisible
      // by style such as display:none, visibility: hidden, or hidden via
      // attribute, etc. Style might also not be calculated if the ancestors
      // were invisible.
      const ComputedStyle* style = entry->target()->GetComputedStyle();
      if (!style || style->UsedVisibility() != EVisibility::kVisible ||
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

void LazyLoadImageObserver::Trace(Visitor* visitor) const {
  visitor->Trace(lazy_load_intersection_observer_);
}

int LazyLoadImageObserver::GetLazyLoadingImageMarginPx(
    const Document& document) {
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
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
}

}  // namespace blink
