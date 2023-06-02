// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/anchor_element_observer_for_service_worker.h"

#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
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
      warm_up_request_cache_(
          blink::features::kSpeculativeServiceWorkerWarmUpRequestCacheSize
              .Get()),
      batch_timer_(
          document.GetTaskRunner(TaskType::kInternalDefault),
          this,
          &AnchorElementObserverForServiceWorker::SendPendingWarmUpRequests) {
  CHECK(document.IsInOutermostMainFrame());
  if (blink::features::kSpeculativeServiceWorkerWarmUpIntersectionObserver
          .Get()) {
    intersection_observer_ = IntersectionObserver::Create(
        {}, {std::numeric_limits<float>::min()}, &document,
        WTF::BindRepeating(
            &AnchorElementObserverForServiceWorker::UpdateVisibleAnchors,
            WrapWeakPersistent(this)),
        LocalFrameUkmAggregator::kAnchorElementMetricsIntersectionObserver,
        IntersectionObserver::kPostTaskToDeliver,
        IntersectionObserver::kFractionOfTarget,
        /*delay=*/
        blink::features::
            kSpeculativeServiceWorkerWarmUpIntersectionObserverDelay.Get());
  }
}

void AnchorElementObserverForServiceWorker::ObserveAnchorElementVisibility(
    HTMLAnchorElement& element) {
  if (blink::features::kSpeculativeServiceWorkerWarmUpIntersectionObserver
          .Get()) {
    TRACE_EVENT0("ServiceWorker",
                 "AnchorElementObserverForServiceWorker::"
                 "ObserveAnchorElementVisibility");
    intersection_observer_->observe(&element);
  }
}

void AnchorElementObserverForServiceWorker::UpdateVisibleAnchors(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  if (!blink::features::kSpeculativeServiceWorkerWarmUpOnVisible.Get()) {
    return;
  }

  TRACE_EVENT0("ServiceWorker",
               "AnchorElementObserverForServiceWorker::UpdateVisibleAnchors");

  Vector<KURL> urls;
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

    CHECK(IsA<HTMLAnchorElement>(*element));
    HTMLAnchorElement& anchor = To<HTMLAnchorElement>(*element);
    if (anchor.IsLink()) {
      urls.push_back(anchor.Url());
    }
  }

  MaybeSendNavigationTargetUrls(urls);
}

void AnchorElementObserverForServiceWorker::MaybeSendNavigationTargetUrls(
    const Vector<KURL>& candidate_urls) {
  if (candidate_urls.empty()) {
    return;
  }

  TRACE_EVENT0("ServiceWorker",
               "AnchorElementObserverForServiceWorker::"
               "MaybeSendNavigationTargetUrls");

  const base::TimeDelta kReWarmUpThreshold =
      blink::features::kSpeculativeServiceWorkerWarmUpReWarmUpThreshold.Get();
  const int kWarmUpRequestLimit =
      blink::features::kSpeculativeServiceWorkerWarmUpRequestLimit.Get();

  const KURL& document_url = GetSupplementable()->Url();
  for (KURL url : candidate_urls) {
    if (!url.IsValid() || !url.ProtocolIsInHTTPFamily() ||
        EqualIgnoringFragmentIdentifier(document_url, url)) {
      continue;
    }

    url.RemoveFragmentIdentifier();
    url.SetUser(String());
    url.SetPass(String());

    // Prevents excessive duplicate warm-up requests.
    base::TimeTicks now = base::TimeTicks::Now();
    auto found = warm_up_request_cache_.Get(url.GetString());
    if (found != warm_up_request_cache_.end() &&
        now - found->second < kReWarmUpThreshold) {
      continue;
    }

    if (total_request_count_ < kWarmUpRequestLimit) {
      warm_up_request_cache_.Put(url.GetString(), now);
      pending_warm_up_requests_.push_back(std::move(url));
      ++total_request_count_;
    }
  }

  MaybeSendPendingWarmUpRequests();
}

void AnchorElementObserverForServiceWorker::MaybeSendPendingWarmUpRequests() {
  TRACE_EVENT0(
      "ServiceWorker",
      "AnchorElementObserverForServiceWorker::MaybeSendPendingWarmUpRequests");
  if (!pending_warm_up_requests_.empty() &&
      GetSupplementable()->LoadEventFinished() && !batch_timer_.IsActive()) {
    batch_timer_.StartOneShot(
        blink::features::kSpeculativeServiceWorkerWarmUpBatchTimer.Get(),
        FROM_HERE);
  }
}

void AnchorElementObserverForServiceWorker::SendPendingWarmUpRequests(
    TimerBase*) {
  LocalFrame* local_frame = GetSupplementable()->GetFrame();

  if (!local_frame) {
    return;
  }

  TRACE_EVENT1(
      "ServiceWorker",
      "AnchorElementObserverForServiceWorker::SendPendingWarmUpRequests",
      "url_count", pending_warm_up_requests_.size());

  Vector<KURL> urls;
  pending_warm_up_requests_.swap(urls);
  local_frame->MaybeStartOutermostMainFrameNavigation(std::move(urls));
}

void AnchorElementObserverForServiceWorker::Trace(Visitor* visitor) const {
  Supplement<Document>::Trace(visitor);
  visitor->Trace(intersection_observer_);
  visitor->Trace(batch_timer_);
}

}  // namespace blink
