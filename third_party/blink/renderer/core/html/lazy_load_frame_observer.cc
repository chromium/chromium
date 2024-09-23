// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/lazy_load_frame_observer.h"

#include <limits>

#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/loading_attribute.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// Determine if the |bounding_client_rect| for a frame indicates that the frame
// is probably hidden according to some experimental heuristics. Since hidden
// frames are often used for analytics or communication, and lazily loading them
// could break their functionality, so these heuristics are used to recognize
// likely hidden frames and immediately load them so that they can function
// properly.
bool IsFrameProbablyHidden(const gfx::RectF& bounding_client_rect,
                           const Element& element) {
  // Tiny frames that are 4x4 or smaller are likely not intended to be seen by
  // the user. Note that this condition includes frames marked as
  // "display:none", since those frames would have dimensions of 0x0.
  if (bounding_client_rect.width() <= 4.0f ||
      bounding_client_rect.height() <= 4.0f) {
    return true;
  }

  // Frames that are positioned completely off the page above or to the left are
  // likely never intended to be visible to the user.
  if (bounding_client_rect.right() < 0.0f ||
      bounding_client_rect.bottom() < 0.0f) {
    return true;
  }

  const ComputedStyle* style = element.GetComputedStyle();
  if (style) {
    switch (style->UsedVisibility()) {
      case EVisibility::kHidden:
      case EVisibility::kCollapse:
        return true;
      case EVisibility::kVisible:
        break;
      case EVisibility::kInert:
        NOTREACHED();
    }
  }

  return false;
}

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
  NOTREACHED_IN_MIGRATION();
  return 0;
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

  if (RuntimeEnabledFeatures::LazyLoadScrollMarginIframeEnabled()) {
    lazy_load_intersection_observer_ = IntersectionObserver::Create(
        element_->GetDocument(),
        WTF::BindRepeating(&LazyLoadFrameObserver::LoadIfHiddenOrNearViewport,
                           WrapWeakPersistent(this)),
        LocalFrameUkmAggregator::kLazyLoadIntersectionObserver,
        IntersectionObserver::Params{
            .scroll_margin = {Length::Fixed(
                GetLazyLoadingFrameMarginPx(element_->GetDocument()))},
            .thresholds = {std::numeric_limits<float>::min()},
        });
  } else {
    lazy_load_intersection_observer_ = IntersectionObserver::Create(
        element_->GetDocument(),
        WTF::BindRepeating(&LazyLoadFrameObserver::LoadIfHiddenOrNearViewport,
                           WrapWeakPersistent(this)),
        LocalFrameUkmAggregator::kLazyLoadIntersectionObserver,
        IntersectionObserver::Params{
            .margin = {Length::Fixed(
                GetLazyLoadingFrameMarginPx(element_->GetDocument()))},
            .thresholds = {std::numeric_limits<float>::min()},
        });
  }

  lazy_load_intersection_observer_->observe(element_);
}

void LazyLoadFrameObserver::CancelPendingLazyLoad() {
  lazy_load_request_info_.reset();

  if (!lazy_load_intersection_observer_)
    return;
  lazy_load_intersection_observer_->disconnect();
  lazy_load_intersection_observer_.Clear();
}

void LazyLoadFrameObserver::LoadIfHiddenOrNearViewport(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  DCHECK(!entries.empty());
  DCHECK_EQ(element_, entries.back()->target());

  if (entries.back()->isIntersecting()) {
    LoadImmediately();
    return;
  }

  // When frames are loaded lazily, normally loading attributes are specified as
  // |LoadingAttributeValue::kLazy|. However, the browser initiated lazyloading
  // (e.g. LazyEmbeds) may apply lazyload automatically to some frames. In that
  // case, target frames may not have loading="lazy" attributes. If the frame
  // doesn't have loading="lazy", that means the frame is loaded as a lazyload
  // manner, which is enabled by the browser initiated lazyloading.
  //
  // Normally the lazyload is triggered to frames regardless of size or
  // visibility, but as the browser initiated lazyload does not apply
  // lazyloading if the frame is small or hidden. See the comment in
  // |IsFrameProbablyHidden()| for more details.
  LoadingAttributeValue loading_attr = GetLoadingAttributeValue(
      element_->FastGetAttribute(html_names::kLoadingAttr));
  if (loading_attr != LoadingAttributeValue::kLazy &&
      IsFrameProbablyHidden(entries.back()->GetGeometry().TargetRect(),
                            *element_)) {
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
  // clearing the frame's lazy load frame observer via |CancelPendingLayLoad()|.
  CHECK(!IsLazyLoadPending());
}

void LazyLoadFrameObserver::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(lazy_load_intersection_observer_);
}

}  // namespace blink
