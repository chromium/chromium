// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_metrics.h"

#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/sparse_histogram.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/should_swap_browsing_instance.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/debug_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/reload_type.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "ui/accessibility/ax_event.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// Overridden time for unit tests. Should be accessed only from the main thread.
base::TickClock* g_mock_time_clock_for_testing = nullptr;

// Reduce the resolution of the longer intervals due to privacy considerations.
base::TimeDelta ClampTime(base::TimeDelta time) {
  if (time < base::Seconds(5))
    return base::Milliseconds(time.InMilliseconds());
  if (time < base::Minutes(3))
    return base::Seconds(time.InSeconds());
  if (time < base::Hours(3))
    return base::Minutes(time.InMinutes());
  return base::Hours(time.InHours());
}

base::TimeTicks Now() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_mock_time_clock_for_testing)
    return g_mock_time_clock_for_testing->NowTicks();
  return base::TimeTicks::Now();
}

bool IsHistoryNavigation(NavigationRequest* navigation) {
  return navigation->GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK;
}

}  // namespace

// static
void BackForwardCacheMetrics::OverrideTimeForTesting(base::TickClock* clock) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  g_mock_time_clock_for_testing = clock;
}

// static
scoped_refptr<BackForwardCacheMetrics>
BackForwardCacheMetrics::CreateOrReuseBackForwardCacheMetrics(
    NavigationEntryImpl* currently_committed_entry,
    bool is_main_frame_navigation,
    int64_t document_sequence_number) {
  if (!currently_committed_entry) {
    // In some rare cases it's possible to navigate a subframe
    // without having a main frame navigation (e.g. extensions
    // injecting frames into a blank page).
    return base::WrapRefCounted(new BackForwardCacheMetrics(
        is_main_frame_navigation ? document_sequence_number : -1));
  }

  BackForwardCacheMetrics* currently_committed_metrics =
      currently_committed_entry->back_forward_cache_metrics();
  if (!currently_committed_metrics) {
    // When we restore the session it's possible to end up with an entry without
    // metrics.
    // We will have to create a new metrics object for the main document.
    return base::WrapRefCounted(new BackForwardCacheMetrics(
        is_main_frame_navigation
            ? document_sequence_number
            : currently_committed_entry->root_node()
                  ->frame_entry->document_sequence_number()));
  }

  if (!is_main_frame_navigation)
    return currently_committed_metrics;
  if (document_sequence_number ==
      currently_committed_metrics->document_sequence_number_) {
    return currently_committed_metrics;
  }
  return base::WrapRefCounted(
      new BackForwardCacheMetrics(document_sequence_number));
}

BackForwardCacheMetrics::BackForwardCacheMetrics(
    int64_t document_sequence_number)
    : document_sequence_number_(document_sequence_number),
      page_store_result_(
          std::make_unique<BackForwardCacheCanStoreDocumentResult>()) {}

BackForwardCacheMetrics::~BackForwardCacheMetrics() = default;

void BackForwardCacheMetrics::MainFrameDidStartNavigationToDocument() {
  if (!started_navigation_timestamp_)
    started_navigation_timestamp_ = Now();
}

void BackForwardCacheMetrics::DidCommitNavigation(
    NavigationRequest* navigation,
    bool back_forward_cache_allowed) {
  // "Back-forward cache in enabled only for primary frame trees, so we need to
  // record metrics only for primary main frame navigations".
  if (!navigation->IsInPrimaryMainFrame() || navigation->IsSameDocument())
    return;

  {
    bool is_reload = navigation->GetReloadType() != ReloadType::NONE;
    RecordHistogramForReloadsAndHistoryNavigations(is_reload,
                                                   back_forward_cache_allowed);
  }

  if (IsHistoryNavigation(navigation)) {
    UpdateNotRestoredReasonsForNavigation(navigation);

    bool can_store = *page_store_result_;
    bool did_store = navigation->IsServedFromBackForwardCache();
    DCHECK_EQ(can_store, did_store) << page_store_result_->ToString();

    // If a navigation serves the result from back/forward cache, then it must
    // not have logged any NotRestoredReasons. Also if it is not restored from
    // back/forward cache, the logged reasons must match the actual condition of
    // the navigation and other logged data.
    bool served_from_bfcache_not_match =
        did_store && !page_store_result_->not_stored_reasons().Empty();
    bool browsing_instance_not_swapped_not_match =
        page_store_result_->HasNotStoredReason(
            NotRestoredReason::kBrowsingInstanceNotSwapped) &&
        DidSwapBrowsingInstance();
    bool disable_for_rfh_not_match =
        page_store_result_->HasNotStoredReason(
            NotRestoredReason::kDisableForRenderFrameHostCalled) &&
        page_store_result_->disabled_reasons().size() == 0;
    bool blocklisted_features_not_match =
        page_store_result_->HasNotStoredReason(
            NotRestoredReason::kBlocklistedFeatures) &&
        page_store_result_->blocklisted_features().Empty();
    if (served_from_bfcache_not_match ||
        browsing_instance_not_swapped_not_match || disable_for_rfh_not_match ||
        blocklisted_features_not_match) {
      CaptureTraceForNavigationDebugScenario(
          DebugScenario::kDebugBackForwardCacheMetricsMismatch);
    }

    TRACE_EVENT1("navigation", "HistoryNavigationOutcome", "outcome",
                 page_store_result_->ToString());
    RecordMetricsForHistoryNavigationCommit(navigation,
                                            back_forward_cache_allowed);
    RecordHistoryNavigationUkm(navigation);
    if (!navigation->IsServedFromBackForwardCache()) {
      devtools_instrumentation::BackForwardCacheNotUsed(
          navigation, page_store_result_.get(), page_store_tree_result_.get());
    }
  }

  page_store_result_ =
      std::make_unique<BackForwardCacheCanStoreDocumentResult>();
  previous_navigation_is_served_from_bfcache_ =
      navigation->IsServedFromBackForwardCache();
  previous_navigation_is_history_ = IsHistoryNavigation(navigation);
  last_committed_cross_document_main_frame_navigation_id_ =
      navigation->GetNavigationId();

  // BackForwardCacheMetrics can be reused when reloading. Reset fields for UKM
  // for the next navigation.
  navigated_away_from_main_document_timestamp_ = absl::nullopt;
  started_navigation_timestamp_ = absl::nullopt;
  renderer_killed_timestamp_ = absl::nullopt;
  browsing_instance_swap_result_ = absl::nullopt;
}

void BackForwardCacheMetrics::RecordHistoryNavigationUkm(
    NavigationRequest* navigation) {
  // If |IsHistoryNavigation| is true and
  // |last_committed_cross_document_main_frame_navigation_id_| is not -1, it's a
  // history navigation which we're interested in.
  //
  // |IsHistoryNavigation| is true when the navigation is history navigation,
  // but just after cloning, the metrics object is missing. Then, checking this
  // is not enough. |last_committed_cross_document_main_frame_navigation_id_| is
  // not -1 when the metrics object is available.
  if (!IsHistoryNavigation(navigation))
    return;
  if (last_committed_cross_document_main_frame_navigation_id_ == -1)
    return;

  // We've visited an entry associated with this main frame document before,
  // so record metrics to determine whether it might be a back-forward cache
  // hit.
  ukm::SourceId source_id = ukm::ConvertToSourceId(
      navigation->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
  ukm::builders::HistoryNavigation builder(source_id);
  builder.SetLastCommittedCrossDocumentNavigationSourceIdForTheSameDocument(
      ukm::ConvertToSourceId(
          last_committed_cross_document_main_frame_navigation_id_,
          ukm::SourceIdType::NAVIGATION_ID));
  builder.SetMainFrameFeatures(main_frame_features_.ToEnumBitmask());
  builder.SetSameOriginSubframesFeatures(
      same_origin_frames_features_.ToEnumBitmask());
  builder.SetCrossOriginSubframesFeatures(
      cross_origin_frames_features_.ToEnumBitmask());
  // DidStart notification might be missing for some same-document
  // navigations. It's good that we don't care about the time in the cache
  // in that case.
  if (started_navigation_timestamp_ &&
      navigated_away_from_main_document_timestamp_) {
    builder.SetTimeSinceNavigatedAwayFromDocument(
        ClampTime(started_navigation_timestamp_.value() -
                  navigated_away_from_main_document_timestamp_.value())
            .InMilliseconds());
  }

  builder.SetBackForwardCache_IsServedFromBackForwardCache(
      navigation->IsServedFromBackForwardCache());
  builder.SetBackForwardCache_NotRestoredReasons(
      page_store_result_->not_stored_reasons().ToEnumBitmask());

  builder.SetBackForwardCache_BlocklistedFeatures(
      page_store_result_->blocklisted_features().ToEnumBitmask());

  if (browsing_instance_swap_result_) {
    builder.SetBackForwardCache_BrowsingInstanceNotSwappedReason(
        static_cast<int64_t>(browsing_instance_swap_result_.value()));
  }

  builder.SetBackForwardCache_DisabledForRenderFrameHostReasonCount(
      page_store_result_->disabled_reasons().size());

  builder.Record(ukm::UkmRecorder::Get());

  for (const BackForwardCache::DisabledReason& reason :
       page_store_result_->disabled_reasons()) {
    ukm::builders::BackForwardCacheDisabledForRenderFrameHostReason
        rfh_reason_builder(source_id);
    rfh_reason_builder.SetReason2(MetricValue(reason));
    rfh_reason_builder.Record(ukm::UkmRecorder::Get());
  }

  for (const uint64_t reason :
       page_store_result_->disallow_activation_reasons()) {
    ukm::builders::BackForwardCacheDisallowActivationReason reason_builder(
        source_id);
    reason_builder.SetReason(reason);
    reason_builder.Record(ukm::UkmRecorder::Get());
  }
}

void BackForwardCacheMetrics::MainFrameDidNavigateAwayFromDocument(
    RenderFrameHostImpl* new_main_frame,
    NavigationRequest* navigation) {
  // MainFrameDidNavigateAwayFromDocument is called when we commit a navigation
  // to another main frame document and the current document loses its "last
  // committed" status.
  navigated_away_from_main_document_timestamp_ = Now();
}

void BackForwardCacheMetrics::RecordFeatureUsage(
    RenderFrameHostImpl* main_frame) {
  DCHECK(!main_frame->GetParent());

  main_frame_features_.Clear();
  same_origin_frames_features_.Clear();
  cross_origin_frames_features_.Clear();

  CollectFeatureUsageFromSubtree(main_frame,
                                 main_frame->GetLastCommittedOrigin());
}

void BackForwardCacheMetrics::CollectFeatureUsageFromSubtree(
    RenderFrameHostImpl* rfh,
    const url::Origin& main_frame_origin) {
  blink::scheduler::WebSchedulerTrackedFeatures features =
      rfh->GetBackForwardCacheDisablingFeatures();
  if (!rfh->GetParent()) {
    main_frame_features_.PutAll(features);
  } else if (rfh->GetLastCommittedOrigin().IsSameOriginWith(
                 main_frame_origin)) {
    same_origin_frames_features_.PutAll(features);
  } else {
    cross_origin_frames_features_.PutAll(features);
  }

  for (size_t i = 0; i < rfh->child_count(); ++i) {
    CollectFeatureUsageFromSubtree(rfh->child_at(i)->current_frame_host(),
                                   main_frame_origin);
  }
}

void BackForwardCacheMetrics::FinalizeNotRestoredReasons(
    const BackForwardCacheCanStoreDocumentResult& can_store_flat,
    std::unique_ptr<BackForwardCacheCanStoreTreeResult> can_store_tree) {
  page_store_tree_result_ = std::move(can_store_tree);
  MarkNotRestoredWithReason(can_store_flat);
}

void BackForwardCacheMetrics::MarkNotRestoredWithReason(
    const BackForwardCacheCanStoreDocumentResult& can_store) {
  page_store_result_->AddReasonsFrom(can_store);

  const BackForwardCacheCanStoreDocumentResult::NotStoredReasons&
      not_stored_reasons = can_store.not_stored_reasons();

  if (not_stored_reasons.Has(NotRestoredReason::kRendererProcessKilled)) {
    renderer_killed_timestamp_ = Now();
  }
  if (!not_stored_reasons.Has(NotRestoredReason::kHTTPStatusNotOK) &&
      !not_stored_reasons.Has(NotRestoredReason::kSchemeNotHTTPOrHTTPS) &&
      not_stored_reasons.Has(NotRestoredReason::kNoResponseHead)) {
    CaptureTraceForNavigationDebugScenario(
        DebugScenario::kDebugNoResponseHeadForHttpOrHttps);
    base::debug::DumpWithoutCrashing();
  }
}

void BackForwardCacheMetrics::UpdateNotRestoredReasonsForNavigation(
    NavigationRequest* navigation) {
  // |last_committed_cross_document_main_frame_navigation_id_| is -1 when
  // navigation history has never been initialized. This can happen only when
  // the session history has been restored.
  if (last_committed_cross_document_main_frame_navigation_id_ == -1) {
    page_store_result_->No(NotRestoredReason::kSessionRestored);
  }

  if (!DidSwapBrowsingInstance()) {
    page_store_result_->No(NotRestoredReason::kBrowsingInstanceNotSwapped);
  }

  TRACE_EVENT("navigation",
              "BackForwardCacheMetrics::UpdateNotRestoredReasonsForNavigation",
              ChromeTrackEvent::kBackForwardCacheCanStoreDocumentResult,
              *(page_store_result_.get()));

  // This should not happen, but record this as an 'unknown' reason just in
  // case.
  if (page_store_result_->not_stored_reasons().Empty() &&
      !navigation->IsServedFromBackForwardCache()) {
    page_store_result_->No(NotRestoredReason::kUnknown);

    // TODO(altimin): Add a (D)CHECK here, but this code is reached in
    // unittests.
    return;
  }
}

void BackForwardCacheMetrics::RecordMetricsForHistoryNavigationCommit(
    NavigationRequest* navigation,
    bool back_forward_cache_allowed) const {
  HistoryNavigationOutcome outcome = HistoryNavigationOutcome::kNotRestored;
  if (navigation->IsServedFromBackForwardCache()) {
    outcome = HistoryNavigationOutcome::kRestored;

    if (back_forward_cache_allowed) {
      UMA_HISTOGRAM_ENUMERATION(
          "BackForwardCache.EvictedAfterDocumentRestoredReason",
          EvictedAfterDocumentRestoredReason::kRestored);
    }
    UMA_HISTOGRAM_ENUMERATION(
        "BackForwardCache.AllSites.EvictedAfterDocumentRestoredReason",
        EvictedAfterDocumentRestoredReason::kRestored);
  }

  if (back_forward_cache_allowed) {
    UMA_HISTOGRAM_ENUMERATION("BackForwardCache.HistoryNavigationOutcome",
                              outcome);

    // Record total number of history navigations for all websites allowed by
    // back-forward cache.
    UMA_HISTOGRAM_ENUMERATION("BackForwardCache.ReloadsAndHistoryNavigations",
                              ReloadsAndHistoryNavigations::kHistoryNavigation);
  }

  UMA_HISTOGRAM_ENUMERATION(
      "BackForwardCache.AllSites.HistoryNavigationOutcome", outcome);

  for (NotRestoredReason reason : page_store_result_->not_stored_reasons()) {
    DCHECK(!navigation->IsServedFromBackForwardCache());
    if (back_forward_cache_allowed) {
      UMA_HISTOGRAM_ENUMERATION(
          "BackForwardCache.HistoryNavigationOutcome.NotRestoredReason",
          reason);
    }
    UMA_HISTOGRAM_ENUMERATION(
        "BackForwardCache.AllSites.HistoryNavigationOutcome.NotRestoredReason",
        reason);
    if (reason == NotRestoredReason::kRendererProcessKilled) {
      DCHECK(renderer_killed_timestamp_);
      DCHECK(navigated_away_from_main_document_timestamp_);
      base::TimeDelta time =
          renderer_killed_timestamp_.value() -
          navigated_away_from_main_document_timestamp_.value();
      UMA_HISTOGRAM_LONG_TIMES(
          "BackForwardCache.Eviction.TimeUntilProcessKilled", time);
    }
  }

  for (blink::scheduler::WebSchedulerTrackedFeature feature :
       page_store_result_->blocklisted_features()) {
    if (back_forward_cache_allowed) {
      UMA_HISTOGRAM_ENUMERATION(
          "BackForwardCache.HistoryNavigationOutcome.BlocklistedFeature",
          feature);
    }
    UMA_HISTOGRAM_ENUMERATION(
        "BackForwardCache.AllSites.HistoryNavigationOutcome."
        "BlocklistedFeature",
        feature);
  }

  for (const BackForwardCache::DisabledReason& reason :
       page_store_result_->disabled_reasons()) {
    // Use SparseHistogram instead of other simple macros for metrics. The
    // reasons cannot be represented as a unified enum because they come from
    // multiple sources. At first they were represented as strings but that
    // makes it hard to track new additions. Now they are represented by
    // a combination of source and source-specific enum.
    base::UmaHistogramSparse(
        "BackForwardCache.HistoryNavigationOutcome."
        "DisabledForRenderFrameHostReason2",
        MetricValue(reason));
  }

  for (const uint64_t reason :
       page_store_result_->disallow_activation_reasons()) {
    base::UmaHistogramSparse(
        "BackForwardCache.HistoryNavigationOutcome."
        "DisallowActivationReason",
        reason);
  }

  for (const ax::mojom::Event event : page_store_result_->ax_events()) {
    base::UmaHistogramSparse(
        "BackForwardCache.HistoryNavigationOutcome."
        "NotRestoredDueToAccessibility."
        "AXEventType",
        static_cast<int>(event));
  }

  if (!DidSwapBrowsingInstance()) {
    DCHECK(!navigation->IsServedFromBackForwardCache());

    if (back_forward_cache_allowed) {
      UMA_HISTOGRAM_ENUMERATION(
          "BackForwardCache.HistoryNavigationOutcome."
          "BrowsingInstanceNotSwappedReason",
          browsing_instance_swap_result_.value());
    }
    UMA_HISTOGRAM_ENUMERATION(
        "BackForwardCache.AllSites.HistoryNavigationOutcome."
        "BrowsingInstanceNotSwappedReason",
        browsing_instance_swap_result_.value());
  }
}

void BackForwardCacheMetrics::RecordEvictedAfterDocumentRestored(
    EvictedAfterDocumentRestoredReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "BackForwardCache.EvictedAfterDocumentRestoredReason", reason);
  UMA_HISTOGRAM_ENUMERATION(
      "BackForwardCache.AllSites.EvictedAfterDocumentRestoredReason", reason);
}

void BackForwardCacheMetrics::RecordHistogramForReloadsAndHistoryNavigations(
    bool is_reload,
    bool back_forward_cache_allowed) const {
  if (!is_reload)
    return;
  if (!previous_navigation_is_history_)
    return;
  if (!back_forward_cache_allowed)
    return;

  // Record the total number of reloads after a history navigation.
  UMA_HISTOGRAM_ENUMERATION(
      "BackForwardCache.ReloadsAndHistoryNavigations",
      ReloadsAndHistoryNavigations::kReloadAfterHistoryNavigation);

  // Record separate buckets for cases served and not served from
  // back-forward cache.
  UMA_HISTOGRAM_ENUMERATION(
      "BackForwardCache.ReloadsAfterHistoryNavigation",
      previous_navigation_is_served_from_bfcache_
          ? ReloadsAfterHistoryNavigation::kServedFromBackForwardCache
          : ReloadsAfterHistoryNavigation::kNotServedFromBackForwardCache);
}

// static
uint64_t BackForwardCacheMetrics::MetricValue(
    BackForwardCache::DisabledReason reason) {
  return static_cast<BackForwardCache::DisabledReasonType>(reason.source)
             << BackForwardCache::kDisabledReasonTypeBits |
         reason.id;
}

void BackForwardCacheMetrics::SetBrowsingInstanceSwapResult(
    absl::optional<ShouldSwapBrowsingInstance> reason) {
  browsing_instance_swap_result_ = reason;
}

bool BackForwardCacheMetrics::DidSwapBrowsingInstance() const {
  if (!browsing_instance_swap_result_)
    return true;

  switch (browsing_instance_swap_result_.value()) {
    case ShouldSwapBrowsingInstance::kNo_ProactiveSwapDisabled:
    case ShouldSwapBrowsingInstance::kNo_NotMainFrame:
    case ShouldSwapBrowsingInstance::kNo_HasRelatedActiveContents:
    case ShouldSwapBrowsingInstance::kNo_DoesNotHaveSite:
    case ShouldSwapBrowsingInstance::kNo_SourceURLSchemeIsNotHTTPOrHTTPS:
    case ShouldSwapBrowsingInstance::kNo_SameSiteNavigation:
    case ShouldSwapBrowsingInstance::kNo_AlreadyHasMatchingBrowsingInstance:
    case ShouldSwapBrowsingInstance::kNo_RendererDebugURL:
    case ShouldSwapBrowsingInstance::kNo_NotNeededForBackForwardCache:
    case ShouldSwapBrowsingInstance::kNo_SameDocumentNavigation:
    case ShouldSwapBrowsingInstance::kNo_SameUrlNavigation:
    case ShouldSwapBrowsingInstance::kNo_WillReplaceEntry:
    case ShouldSwapBrowsingInstance::kNo_Reload:
    case ShouldSwapBrowsingInstance::kNo_Guest:
    case ShouldSwapBrowsingInstance::kNo_HasNotComittedAnyNavigation:
    case ShouldSwapBrowsingInstance::
        kNo_UnloadHandlerExistsOnSameSiteNavigation:
    case ShouldSwapBrowsingInstance::kNo_NotPrimaryMainFrame:
      return false;
    case ShouldSwapBrowsingInstance::kYes_ForceSwap:
    case ShouldSwapBrowsingInstance::kYes_CrossSiteProactiveSwap:
    case ShouldSwapBrowsingInstance::kYes_SameSiteProactiveSwap:
      return true;
  }
}

}  // namespace content
