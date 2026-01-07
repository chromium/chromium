// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/lazy_load_frame_observer.h"

#include <limits>

#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"

namespace blink {

namespace {

int GetLazyLoadingFrameMarginPx(const Document& document) {
  const Settings* settings = document.GetSettings();
  if (!settings)
    return 0;

  switch (GetNetworkStateNotifier().EffectiveType()) {
    case WebEffectiveConnectionType::kTypeUnknown:
      return settings->GetLazyLoadingFrameMarginPxUnknown();
    case WebEffectiveConnectionType::kTypeOffline:
      return settings->GetLazyLoadingFrameMarginPxOffline();
    case WebEffectiveConnectionType::kTypeSlow2G:
      return settings->GetLazyLoadingFrameMarginPxSlow2G();
    case WebEffectiveConnectionType::kType2G:
      return settings->GetLazyLoadingFrameMarginPx2G();
    case WebEffectiveConnectionType::kType3G:
      return settings->GetLazyLoadingFrameMarginPx3G();
    case WebEffectiveConnectionType::kType4G:
      return settings->GetLazyLoadingFrameMarginPx4G();
  }
  NOTREACHED();
}

}  // namespace

struct LazyLoadFrameObserver::LazyLoadRequestInfo {
  LazyLoadRequestInfo(const ResourceRequestHead& passed_resource_request,
                      WebFrameLoadType frame_load_type)
      : resource_request(passed_resource_request),
        frame_load_type(frame_load_type) {}

  ResourceRequestHead resource_request;
  const WebFrameLoadType frame_load_type;
};

LazyLoadFrameObserver::LazyLoadFrameObserver(HTMLFrameOwnerElement& element,
                                             LoadType load_type)
    : element_(&element), load_type_(load_type) {}

LazyLoadFrameObserver::~LazyLoadFrameObserver() = default;

void LazyLoadFrameObserver::DeferLoadUntilNearViewport(
    const ResourceRequestHead& resource_request,
    WebFrameLoadType frame_load_type) {
  DCHECK(!lazy_load_intersection_observer_);
  DCHECK(!lazy_load_request_info_);
  lazy_load_request_info_ =
      std::make_unique<LazyLoadRequestInfo>(resource_request, frame_load_type);

  lazy_load_intersection_observer_ = IntersectionObserver::Create(
      element_->GetDocument(),
      BindRepeating(&LazyLoadFrameObserver::LoadIfNearViewport,
                    WrapWeakPersistent(this)),
      LocalFrameUkmAggregator::kLazyLoadIntersectionObserver,
      IntersectionObserver::Params{
          .scroll_margin = {Length::Fixed(
              GetLazyLoadingFrameMarginPx(element_->GetDocument()))},
          .thresholds = {std::numeric_limits<float>::min()},
      });
  lazy_load_intersection_observer_->observe(element_);
}

void LazyLoadFrameObserver::CancelPendingLazyLoad() {
  lazy_load_request_info_.reset();

  if (!lazy_load_intersection_observer_)
    return;
  lazy_load_intersection_observer_->disconnect();
  lazy_load_intersection_observer_.Clear();
}

void LazyLoadFrameObserver::LoadIfNearViewport(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  DCHECK(!entries.empty());
  DCHECK_EQ(element_, entries.back()->target());

  if (entries.back()->isIntersecting()) {
    LoadImmediately();
    return;
  }
}

void LazyLoadFrameObserver::LoadImmediately() {
  CHECK(IsLazyLoadPending());
  CHECK(lazy_load_request_info_);
  TRACE_EVENT0("navigation", "LazyLoadFrameObserver::LoadImmediately");

  std::unique_ptr<LazyLoadRequestInfo> scoped_request_info =
      std::move(lazy_load_request_info_);

  // The content frame of the element should not have changed, since any
  // pending lazy load should have been already been cancelled in
  // DisconnectContentFrame() if the content frame changes.
  CHECK(element_->ContentFrame());

  FrameLoadRequest request(element_->GetDocument().domWindow(),
                           scoped_request_info->resource_request);

  if (load_type_ == LoadType::kFirst) {
    To<LocalFrame>(element_->ContentFrame())
        ->Loader()
        .StartNavigation(request, scoped_request_info->frame_load_type);
  } else if (load_type_ == LoadType::kSubsequent) {
    element_->ContentFrame()->Navigate(request,
                                       scoped_request_info->frame_load_type);
  }

  // Note that whatever we delegate to for the navigation is responsible for
  // clearing the frame's lazy load frame observer via
  // `CancelPendingLazyLoad()`.
  CHECK(!IsLazyLoadPending());
}

void LazyLoadFrameObserver::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(lazy_load_intersection_observer_);
}

}  // namespace blink
