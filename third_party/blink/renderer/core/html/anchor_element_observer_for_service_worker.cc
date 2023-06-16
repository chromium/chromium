// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/anchor_element_observer_for_service_worker.h"

#include <algorithm>

#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

// static
const char AnchorElementObserverForServiceWorker::kSupplementName[] =
    "AnchorElementObserverForServiceWorker";

// static
AnchorElementObserverForServiceWorker*
AnchorElementObserverForServiceWorker::From(Document& document) {
  TRACE_EVENT0("ServiceWorker", "AnchorElementObserverForServiceWorker::From");
  const KURL& url = document.Url();
  if (!document.IsInOutermostMainFrame() || !url.IsValid() ||
      !url.ProtocolIsInHTTPFamily()) {
    return nullptr;
  }

  AnchorElementObserverForServiceWorker* observer =
      Supplement<Document>::From<AnchorElementObserverForServiceWorker>(
          document);

  if (!observer) {
    observer = MakeGarbageCollected<AnchorElementObserverForServiceWorker>(
        base::PassKey<AnchorElementObserverForServiceWorker>(), document);
    ProvideTo(document, observer);
  }

  return observer;
}

AnchorElementObserverForServiceWorker::AnchorElementObserverForServiceWorker(
    base::PassKey<AnchorElementObserverForServiceWorker>,
    Document& document)
    : Supplement<Document>(document),
      batch_timer_(
          document.GetTaskRunner(TaskType::kInternalDefault),
          this,
          &AnchorElementObserverForServiceWorker::SendPendingWarmUpRequests) {
  CHECK(document.IsInOutermostMainFrame());
  if (features::kSpeculativeServiceWorkerWarmUpIntersectionObserver.Get()) {
    intersection_observer_ = IntersectionObserver::Create(
        {}, {std::numeric_limits<float>::min()}, &document,
        WTF::BindRepeating(
            &AnchorElementObserverForServiceWorker::UpdateVisibleAnchors,
            WrapWeakPersistent(this)),
        LocalFrameUkmAggregator::kAnchorElementMetricsIntersectionObserver,
        IntersectionObserver::kPostTaskToDeliver,
        IntersectionObserver::kFractionOfTarget,
        /*delay=*/
        features::kSpeculativeServiceWorkerWarmUpIntersectionObserverDelay
            .Get());
  }
}

void AnchorElementObserverForServiceWorker::ObserveAnchorElementVisibility(
    HTMLAnchorElement& element) {
  if (features::kSpeculativeServiceWorkerWarmUpIntersectionObserver.Get()) {
    TRACE_EVENT0("ServiceWorker",
                 "AnchorElementObserverForServiceWorker::"
                 "ObserveAnchorElementVisibility");
    intersection_observer_->observe(&element);
  }
}

void AnchorElementObserverForServiceWorker::UpdateVisibleAnchors(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  if (!features::kSpeculativeServiceWorkerWarmUpOnVisible.Get()) {
    return;
  }

  TRACE_EVENT0("ServiceWorker",
               "AnchorElementObserverForServiceWorker::UpdateVisibleAnchors");

  Links links;
  for (const auto& entry : entries) {
    if (!entry->isIntersecting()) {
      continue;
    }

    Element* element = entry->target();
    if (!element) {
      continue;
    }

    // Once an element is evaluated, we stop observing the element to reduce the
    // computational load caused by IntersectionObserver.
    intersection_observer_->unobserve(element);

    HTMLAnchorElement* anchor = DynamicTo<HTMLAnchorElement>(element);
    if (!anchor) {
      anchor = DynamicTo<HTMLAreaElement>(element);
    }
    if (anchor && anchor->IsLink() &&
        !already_handled_links_.Contains(anchor)) {
      links.push_back(anchor);
    }
  }

  MaybeSendNavigationTargetLinks(links);
}

void AnchorElementObserverForServiceWorker::MaybeSendNavigationTargetLinks(
    const Links& candidate_links) {
  if (candidate_links.empty()) {
    return;
  }

  TRACE_EVENT0("ServiceWorker",
               "AnchorElementObserverForServiceWorker::"
               "MaybeSendNavigationTargetLinks");

  const int kWarmUpRequestLimit =
      features::kSpeculativeServiceWorkerWarmUpRequestLimit.Get();

  for (const auto& link : candidate_links) {
    // Prevents excessive duplicate warm-up requests.
    if (already_handled_links_.Contains(link)) {
      continue;
    }

    if (total_request_count_ < kWarmUpRequestLimit) {
      ++total_request_count_;
      already_handled_links_.insert(link);
      pending_warm_up_links_.push_back(link);
    }
  }

  MaybeSendPendingWarmUpRequests();
}

void AnchorElementObserverForServiceWorker::MaybeSendPendingWarmUpRequests() {
  TRACE_EVENT0(
      "ServiceWorker",
      "AnchorElementObserverForServiceWorker::MaybeSendPendingWarmUpRequests");

  if (features::kSpeculativeServiceWorkerWarmUpWaitForLoad.Get() &&
      !GetDocument().LoadEventFinished()) {
    return;
  }

  if (!pending_warm_up_links_.empty() && !batch_timer_.IsActive()) {
    batch_timer_.StartOneShot(
        is_first_batch_
            ? features::kSpeculativeServiceWorkerWarmUpFirstBatchTimer.Get()
            : features::kSpeculativeServiceWorkerWarmUpBatchTimer.Get(),
        FROM_HERE);
    is_first_batch_ = false;
  }
}

void AnchorElementObserverForServiceWorker::SendPendingWarmUpRequests(
    TimerBase*) {
  LocalFrame* local_frame = GetDocument().GetFrame();

  if (!local_frame) {
    return;
  }

  TRACE_EVENT1(
      "ServiceWorker",
      "AnchorElementObserverForServiceWorker::SendPendingWarmUpRequests",
      "pending_link_count", pending_warm_up_links_.size());

  const uint32_t kMaxBatchSize =
      features::kSpeculativeServiceWorkerWarmUpBatchSize.Get();

  const KURL& document_url = GetDocument().Url();
  HashSet<KURL> url_set;
  Vector<KURL> urls;
  urls.reserve(std::min(pending_warm_up_links_.size(), kMaxBatchSize));
  while (!pending_warm_up_links_.empty()) {
    KURL url = pending_warm_up_links_.back()->Url();
    pending_warm_up_links_.pop_back();

    if (!url.IsValid() || !url.ProtocolIsInHTTPFamily() ||
        EqualIgnoringFragmentIdentifier(document_url, url)) {
      continue;
    }

    url.RemoveFragmentIdentifier();
    url.SetUser(String());
    url.SetPass(String());

    if (url_set.Contains(url)) {
      continue;
    }
    url_set.insert(url);
    urls.push_back(std::move(url));
    if (urls.size() >= kMaxBatchSize) {
      break;
    }
  }
  urls.Reverse();
  local_frame->MaybeStartOutermostMainFrameNavigation(std::move(urls));

  // Send remaining requests later.
  MaybeSendPendingWarmUpRequests();
}

void AnchorElementObserverForServiceWorker::Trace(Visitor* visitor) const {
  Supplement<Document>::Trace(visitor);
  visitor->Trace(intersection_observer_);
  visitor->Trace(already_handled_links_);
  visitor->Trace(pending_warm_up_links_);
  visitor->Trace(batch_timer_);
}

}  // namespace blink
