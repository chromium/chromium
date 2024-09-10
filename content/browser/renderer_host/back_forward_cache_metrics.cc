// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_metrics.h"

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/sparse_histogram.h"
#include "components/back_forward_cache/disabled_reason_id.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/should_swap_browsing_instance.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/debug_utils.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/reload_type.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
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

}  // namespace

// static
void BackForwardCacheMetrics::OverrideTimeForTesting(base::TickClock* clock) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  g_mock_time_clock_for_testing = clock;
}

// static
bool BackForwardCacheMetrics::IsCrossDocumentMainFrameHistoryNavigation(
    NavigationRequest* navigation) {
  return navigation->IsInPrimaryMainFrame() &&
         !navigation->frame_tree_node()->GetParentOrOuterDocumentOrEmbedder() &&
         !navigation->IsSameDocument() &&
         navigation->GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK;
}

// static
scoped_refptr<BackForwardCacheMetrics>
BackForwardCacheMetrics::CreateOrReuseBackForwardCacheMetricsForNavigation(
    NavigationEntryImpl* previous_entry,
    bool is_main_frame_navigation,
    int64_t committing_document_sequence_number) {
  if (!previous_entry) {
    // There is no previous NavigationEntry, so we must create a new metrics
    // object.
    return base::WrapRefCounted(new BackForwardCacheMetrics(
        is_main_frame_navigation ? committing_document_sequence_number : -1));
  }

  BackForwardCacheMetrics* previous_entry_metrics =
      previous_entry->back_forward_cache_metrics();
  if (!previous_entry_metrics) {
    // It's possible to encounter a `previous_entry` without metrics, e.g. on
    // session restore. We will have to create a new metrics object for the main
    // document.
    return base::WrapRefCounted(new BackForwardCacheMetrics(
        is_main_frame_navigation
            ? committing_document_sequence_number
            : previous_entry->root_node()
                  ->frame_entry->document_sequence_number()));
  }

  // Reuse `previous_entry_metrics` on subframe navigations and same-document
  // navigations.
  if (!is_main_frame_navigation ||
      committing_document_sequence_number ==
          previous_entry_metrics->document_sequence_number_) {
    return previous_entry_metrics;
  }

  return base::WrapRefCounted(
      new BackForwardCacheMetrics(committing_document_sequence_number));
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
  // Back-forward cache in enabled only for primary frame trees, so we need to
  // record metrics only for primary main frame navigations.
  if (!navigation->IsInPrimaryMainFrame() || navigation->IsSameDocument())
    return;

  // Record metrics for history navigation, if applicable.
  if (IsCrossDocumentMainFrameHistoryNavigation(navigation)) {
    // We have to update not restored reasons even though we already did in
    // |SendCommitNavigation()|, because the NavigationEntry and
    // the BackForwardCacheMetrics object might not exist anymore, e.g. when the
    // NavigationEntry got pruned by another navigation committing before the
    // history navigation committed.
    UpdateNotRestoredReasonsForNavigation(navigation);
    bool can_restore = page_store_result_->CanRestore();
    bool did_store = navigation->IsServedFromBackForwardCache();
    DCHECK_EQ(can_restore, did_store) << page_store_result_->ToString();

    // If a navigation serves the result from back/forward cache, then it must
    // not have logged any NotRestoredReasons. Also if it is not restored from
    // back/forward cache, the logged reasons must match the actual condition of
    // the navigation and other logged data.
    bool served_from_bfcache_not_match =
        did_store && !page_store_result_->not_restored_reasons().empty();
    bool browsing_instance_not_swapped_not_match =
        page_store_result_->HasNotRestoredReason(
            NotRestoredReason::kBrowsingInstanceNotSwapped) &&
        DidSwapBrowsingInstance();
    bool disable_for_rfh_not_match =
        page_store_result_->HasNotRestoredReason(
            NotRestoredReason::kDisableForRenderFrameHostCalled) &&
        page_store_result_->disabled_reasons().size() == 0;
    bool blocklisted_features_not_match =
        page_store_result_->HasNotRestoredReason(
            NotRestoredReason::kBlocklistedFeatures) &&
        page_store_result_->blocklisted_features().empty();
    if (served_from_bfcache_not_match ||
        browsing_instance_not_swapped_not_match || disable_for_rfh_not_match ||
        blocklisted_features_not_match) {
      CaptureTraceForNavigationDebugScenario(
          DebugScenario::kDebugBackForwardCacheMetricsMismatch);
    }

    // TODO(crbug.com/40229455): Remove this.
    if (served_from_bfcache_not_match) {
      SCOPED_CRASH_KEY_BOOL("BFCacheMismatch", "did_store", did_store);
      SCOPED_CRASH_KEY_BOOL("BFCacheMismatch", "can_restore", can_restore);
      SCOPED_CRASH_KEY_NUMBER("BFCacheMismatch", "not_restored",
                              page_store_result_->not_restored_reasons()
                                  .GetNth64bitWordBitmask(0)
                                  .value());
      auto not_restored_1 =
          page_store_result_->not_restored_reasons().GetNth64bitWordBitmask(1);
      if (not_restored_1.has_value()) {
        SCOPED_CRASH_KEY_NUMBER("BFCacheMismatch", "not_restored_1",
                                not_restored_1.value());
      }
      SCOPED_CRASH_KEY_NUMBER(
          "BFCacheMismatch", "bi_swap",
          page_store_result_->browsing_instance_swap_result().has_value()
              ? static_cast<int>(
                    page_store_result_->browsing_instance_swap_result().value())
              : -1);
      SCOPED_CRASH_KEY_NUMBER("BFCacheMismatch", "blocklisted",
                              page_store_result_->blocklisted_features()
                                  .GetNth64bitWordBitmask(0)
                                  .value());
      auto blocklisted_1 =
          page_store_result_->blocklisted_features().GetNth64bitWordBitmask(1);
      if (blocklisted_1.has_value()) {
        SCOPED_CRASH_KEY_NUMBER("BFCacheMismatch", "blocklisted_1",
                                blocklisted_1.value());
      }
      SCOPED_CRASH_KEY_NUMBER("BFCacheMismatch", "disabled",
                              page_store_result_->disabled_reasons().size());
      SCOPED_CRASH_KEY_NUMBER(
          "BFCacheMismatch", "disallow_activation",
          page_store_result_->disallow_activation_reasons().size());
      SCOPED_CRASH_KEY_NUMBER("BFCacheMismatch", "restore_type",
                              static_cast<int>(navigation->GetRestoreType()));
      SCOPED_CRASH_KEY_NUMBER("BFCacheMismatch", "reload_type",
                              static_cast<int>(navigation->GetReloadType()));
      SCOPED_CRASH_KEY_STRING256("BFCacheMismatch", "url",
                                 navigation->GetURL().spec());
      SCOPED_CRASH_KEY_STRING256(
          "BFCacheMismatch", "previous_url",
          navigation->GetPreviousPrimaryMainFrameURL().spec());
      // base::debug::DumpWithoutCrashing();
    }

    TRACE_EVENT1("navigation", "HistoryNavigationOutcome", "outcome",
                 page_store_result_->ToString());
    RecordHistoryNavigationUMA(navigation, back_forward_cache_allowed);
    RecordHistoryNavigationUKM(navigation);
    if (!navigation->IsServedFromBackForwardCache()) {
      devtools_instrumentation::BackForwardCacheNotUsed(
          navigation, page_store_result_.get(), page_store_tree_result_.get());
    }
    if (test_observer_) {
      // This is for reporting |page_store_tree_result_| for testing.
      test_observer_->NotifyNotRestoredReasons(
          std::move(page_store_tree_result_));
    }
  }
  // Save the information about the last cross-document main frame navigation
  // that uses this metrics object.
  last_committed_cross_document_main_frame_navigation_id_ =
      navigation->GetNavigationId();

  // BackForwardCacheMetrics can be reused in some cases. Reset fields for UKM
  // for the next navigation.
  page_store_result_ =
      std::make_unique<BackForwardCacheCanStoreDocumentResult>();
  page_store_tree_result_ = nullptr;
  navigated_away_from_main_document_timestamp_ = std::nullopt;
  started_navigation_timestamp_ = std::nullopt;
  renderer_killed_timestamp_ = std::nullopt;
  browsing_instance_swap_result_ = std::nullopt;
}

namespace {

void RecordDisabledForRenderFrameHostReasonUKM(ukm::SourceId source_id,
                                               uint64_t reason) {
  ukm::builders::BackForwardCacheDisabledForRenderFrameHostReason(source_id)
      .SetReason2(reason)
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace

void BackForwardCacheMetrics::RecordHistoryNavigationUKM(
    NavigationRequest* navigation) {
  DCHECK(IsCrossDocumentMainFrameHistoryNavigation(navigation));
  // We've visited an entry associated with this main frame document before,
  // so record metrics to determine whether it might be a back-forward cache
  // hit.
  ukm::SourceId source_id = ukm::ConvertToSourceId(
      navigation->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
  ukm::builders::HistoryNavigation builder(source_id);
  if (last_committed_cross_document_main_frame_navigation_id_ != -1) {
    // Only record `last_committed_cross_document_main_frame_navigation_id_`
    // when it's set. It won't be set if the NavigationEntry this history
    // navigation is targeting hasn't been navigated to in this session (e.g.
    // due to session restore or cloning a tab).
    builder.SetLastCommittedCrossDocumentNavigationSourceIdForTheSameDocument(
        ukm::ConvertToSourceId(
            last_committed_cross_document_main_frame_navigation_id_,
            ukm::SourceIdType::NAVIGATION_ID));
  }

  builder.SetMainFrameFeatures(
      main_frame_features_.GetNth64bitWordBitmask(0).value());
  auto main_frame_features_2 = main_frame_features_.GetNth64bitWordBitmask(1);
  if (main_frame_features_2.has_value()) {
    builder.SetMainFrameFeatures2(main_frame_features_2.value());
  }

  builder.SetSameOriginSubframesFeatures(
      same_origin_frames_features_.GetNth64bitWordBitmask(0).value());
  auto same_origin_frames_features_2 =
      same_origin_frames_features_.GetNth64bitWordBitmask(1);
  if (same_origin_frames_features_2.has_value()) {
    builder.SetSameOriginSubframesFeatures2(
        same_origin_frames_features_2.value());
  }

  builder.SetCrossOriginSubframesFeatures(
      cross_origin_frames_features_.GetNth64bitWordBitmask(0).value());
  auto cross_origin_frames_features_2 =
      cross_origin_frames_features_.GetNth64bitWordBitmask(1);
  if (cross_origin_frames_features_2.has_value()) {
    builder.SetCrossOriginSubframesFeatures2(
        cross_origin_frames_features_2.value());
  }
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
      page_store_result_->not_restored_reasons()
          .GetNth64bitWordBitmask(0)
          .value());
  auto not_restored_reasons_2 =
      page_store_result_->not_restored_reasons().GetNth64bitWordBitmask(1);
  if (not_restored_reasons_2.has_value()) {
    builder.SetBackForwardCache_NotRestoredReasons2(
        not_restored_reasons_2.value());
  }

  builder.SetBackForwardCache_BlocklistedFeatures(
      page_store_result_->blocklisted_features()
          .GetNth64bitWordBitmask(0)
          .value());
  auto blocklisted_features_2 =
      page_store_result_->blocklisted_features().GetNth64bitWordBitmask(1);
  if (blocklisted_features_2.has_value()) {
    builder.SetBackForwardCache_BlocklistedFeatures2(
        blocklisted_features_2.value());
  }

  if (browsing_instance_swap_result_) {
    builder.SetBackForwardCache_BrowsingInstanceNotSwappedReason(
        static_cast<int64_t>(browsing_instance_swap_result_.value()));
  }

  builder.SetBackForwardCache_DisabledForRenderFrameHostReasonCount(
      page_store_result_->disabled_reasons().size());

  builder.Record(ukm::UkmRecorder::Get());

  bool is_disabled_for_extension_messaging = false;
  std::string blocking_extension_id;

  for (const auto& [reason, associated_source_ids] :
       page_store_result_->disabled_reasons()) {
    uint64_t reason_value = MetricValue(reason);
    // We always record the event under the source id that was obtained from
    // the navigation.
    RecordDisabledForRenderFrameHostReasonUKM(source_id, reason_value);

    if (!is_disabled_for_extension_messaging &&
        reason.id == static_cast<BackForwardCache::DisabledReasonType>(
                         back_forward_cache::DisabledReasonId::
                             kExtensionSentMessageToCachedFrame)) {
      // Only the first extension (ideally, there should be only one)
      // that triggers `kExtensionSentMessageToCachedFrame` will be recorded in
      // the message.
      is_disabled_for_extension_messaging = true;
      blocking_extension_id = reason.context;
    }

    for (const auto& associated_source_id : associated_source_ids) {
      if (associated_source_id.has_value()) {
        RecordDisabledForRenderFrameHostReasonUKM(associated_source_id.value(),
                                                  reason_value);
      }
    }
  }

  if (is_disabled_for_extension_messaging) {
    navigation->GetRenderFrameHost()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        base::StringPrintf(
            "This page was not restored from back/forward cache because a "
            "content script from the extension with ID %s received a message "
            "while the page was cached. This behavior will change shortly "
            "which may break the extension. If you are the developer of the "
            "extension, see "
            "https://developer.chrome.com/blog/"
            "bfcache-extension-messaging-changes.",
            blocking_extension_id.c_str()));
  }

  for (const uint64_t reason :
       page_store_result_->disallow_activation_reasons()) {
    ukm::builders::BackForwardCacheDisallowActivationReason reason_builder(
        source_id);
    reason_builder.SetReason(reason);
    reason_builder.Record(ukm::UkmRecorder::Get());
  }
}

void BackForwardCacheMetrics::MainFrameDidNavigateAwayFromDocument() {
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

void BackForwardCacheMetrics::AddNotRestoredFlattenedReasonsToExistingResult(
    BackForwardCacheCanStoreDocumentResult& flattened) {
  page_store_result_->AddReasonsFrom(flattened);

  const BackForwardCacheCanStoreDocumentResult::NotRestoredReasons&
      not_restored_reasons = flattened.not_restored_reasons();

  if (not_restored_reasons.Has(NotRestoredReason::kRendererProcessKilled)) {
    renderer_killed_timestamp_ = Now();
  }
}

void BackForwardCacheMetrics::SetNotRestoredReasons(
    BackForwardCacheCanStoreDocumentResultWithTree& can_store) {
  DCHECK(can_store.tree_reasons->FlattenTree() == can_store.flattened_reasons);
  page_store_tree_result_ = std::move(can_store.tree_reasons);
  AddNotRestoredFlattenedReasonsToExistingResult(can_store.flattened_reasons);
}

blink::mojom::BackForwardCacheNotRestoredReasonsPtr
BackForwardCacheMetrics::GetWebExposedNotRestoredReasons() {
  return page_store_tree_result_->GetWebExposedNotRestoredReasons();
}

void BackForwardCacheMetrics::UpdateNotRestoredReasonsForNavigation(
    NavigationRequest* navigation) {
  DCHECK(IsCrossDocumentMainFrameHistoryNavigation(navigation));
  BackForwardCacheCanStoreDocumentResult new_blocking_reasons;
  // |last_committed_cross_document_main_frame_navigation_id_| is -1 even though
  // this is a history navigation. This can happen only when the session history
  // has been restored, as the NavigationEntry will exist and can be navigated
  // to, but the BackForwardCacheMetrics object is brand new (as it's not
  // persisted and restored).
  if (last_committed_cross_document_main_frame_navigation_id_ == -1) {
    new_blocking_reasons.No(NotRestoredReason::kSessionRestored);
  }

  // TODO(rakina): Remove this call from here and move it to
  // |SetNotRestoredReasons()| that is called from |UnloadOldFrame()|.
  if (!DidSwapBrowsingInstance()) {
    new_blocking_reasons.No(NotRestoredReason::kBrowsingInstanceNotSwapped);
  }

  // This should not happen, but record this as an 'unknown' reason just in
  // case.
  if (page_store_result_->not_restored_reasons().empty() &&
      new_blocking_reasons.not_restored_reasons().empty() &&
      !navigation->IsServedFromBackForwardCache()) {
    // TODO(altimin): Add a (D)CHECK here, but this code is reached in
    // unittests.
    new_blocking_reasons.No(NotRestoredReason::kUnknown);
  }

  page_store_result_->AddReasonsFrom(new_blocking_reasons);

  // Initialize the empty tree result if nothing is set.
  if (!page_store_tree_result_) {
    page_store_tree_result_ =
        BackForwardCacheCanStoreTreeResult::CreateEmptyTreeForNavigation(
            navigation);
  }
  // Add the same reason to the root node of the tree once we update the
  // flattened list of reasons.
  page_store_tree_result_->AddReasonsToSubtreeRootFrom(new_blocking_reasons);

  TRACE_EVENT("navigation",
              "BackForwardCacheMetrics::UpdateNotRestoredReasonsForNavigation",
              ChromeTrackEvent::kBackForwardCacheCanStoreDocumentResult,
              *(page_store_result_.get()));
}

void BackForwardCacheMetrics::RecordHistoryNavigationUMA(
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
    int nav_offset = navigation->GetNavigationEntryOffset();
    HistoryNavigationDirection direction =
        nav_offset == 0
            ? HistoryNavigationDirection::kSameEntry
            : (nav_offset < 0 ? HistoryNavigationDirection::kBack
                              : HistoryNavigationDirection::kForward);
    if (navigation->IsServedFromBackForwardCache()) {
      base::UmaHistogramEnumeration(
          "BackForwardCache.RestoredNavigationDirection", direction);
    } else {
      base::UmaHistogramEnumeration(
          "BackForwardCache.NonRestoredNavigationDirection", direction);
    }
  }

  UMA_HISTOGRAM_ENUMERATION(
      "BackForwardCache.AllSites.HistoryNavigationOutcome", outcome);

  if (had_form_data_associated()) {
    UMA_HISTOGRAM_ENUMERATION("BackForwardCache.PageWithForm.RestoreResult",
                              outcome);
  }

  for (NotRestoredReason reason : page_store_result_->not_restored_reasons()) {
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

  if (back_forward_cache_allowed) {
    for (const auto& [reason, _] : page_store_result_->disabled_reasons()) {
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

    if (back_forward_cache_allowed &&
        browsing_instance_swap_result_ ==
            ShouldSwapBrowsingInstance::kNo_HasRelatedActiveContents) {
      CHECK_GT(related_active_contents_count_, 1);
      // If a page was not restored from the back/forward cache because there
      // are related active contents, log the details of the related active
      // contents. Note that this also logs in cases where there are other
      // reasons causing the page to not get restored from the back/forward
      // cache (e.g. use of blocking features).
      base::UmaHistogramCounts100(
          "BackForwardCache.HistoryNavigationOutcome."
          "RelatedActiveContents.Count2",
          related_active_contents_count_);
      base::UmaHistogramEnumeration(
          "BackForwardCache.HistoryNavigationOutcome."
          "RelatedActiveContents.IsPotentiallySyncAccessible2",
          related_active_contents_sync_access_info_);
    }
  }
}

void BackForwardCacheMetrics::RecordEvictedAfterDocumentRestored(
    EvictedAfterDocumentRestoredReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "BackForwardCache.EvictedAfterDocumentRestoredReason", reason);
  UMA_HISTOGRAM_ENUMERATION(
      "BackForwardCache.AllSites.EvictedAfterDocumentRestoredReason", reason);
}

// static
uint64_t BackForwardCacheMetrics::MetricValue(
    BackForwardCache::DisabledReason reason) {
  return static_cast<BackForwardCache::DisabledReasonType>(reason.source)
             << BackForwardCache::kDisabledReasonTypeBits |
         reason.id;
}

void BackForwardCacheMetrics::SetBrowsingInstanceSwapResult(
    std::optional<ShouldSwapBrowsingInstance> reason,
    RenderFrameHostImpl* navigated_away_rfh) {
  browsing_instance_swap_result_ = reason;
  if (navigated_away_rfh) {
    SetRelatedActiveContentsInfo(navigated_away_rfh);
  } else {
    // The tracked reason is being reset as a result of prerender activation.
    // Also reset `related_active_contents_count_` and
    // `related_active_contents_sync_access_info_` as they are stale too.
    CHECK(!reason.has_value());
    related_active_contents_count_ = 1;
    related_active_contents_sync_access_info_ =
        RelatedActiveContentsSyncAccessInfo::kNoSyncAccess;
  }
}

void BackForwardCacheMetrics::SetRelatedActiveContentsInfo(
    RenderFrameHostImpl* navigated_away_rfh) {
  CHECK(navigated_away_rfh->is_main_frame());
  related_active_contents_count_ =
      navigated_away_rfh->GetSiteInstance()->GetRelatedActiveContentsCount();

  // Count how many documents in the navigating page are using each SiteInfo.
  std::map<SiteInfo, int> doc_count_in_page;
  navigated_away_rfh->ForEachRenderFrameHost([&doc_count_in_page](
                                                 RenderFrameHost* rfh) {
    const SiteInfo& site_info = static_cast<RenderFrameHostImpl*>(rfh)
                                    ->last_committed_url_derived_site_info();
    if (doc_count_in_page.contains(site_info)) {
      doc_count_in_page[site_info]++;
    } else {
      doc_count_in_page[site_info] = 1;
    }
  });

  // Determine if any document in the navigating page is potentially
  // synchronously accessible by documents in other pages, by checking if there
  // are documents in other pages that use the same SiteInfo as a document in
  // the navigating page. This uses SiteInfos derived from document URLs, which
  // works even when Site Isolation is disabled and the default SiteInstance may
  // contain multiple sites.
  related_active_contents_sync_access_info_ =
      RelatedActiveContentsSyncAccessInfo::kNoSyncAccess;
  navigated_away_rfh->ForEachRenderFrameHostWithAction(
      [&doc_count_in_page, this](RenderFrameHost* rfh) {
        // `active_document_count()` counts the number of committed
        // documents in all pages that are using the same SiteInfo, including
        // the navigating page. To get the number of committed documents using
        // the same SiteInfo in pages other than the navigating page, just
        // subtract by the number of committed documents using SiteInfo in the
        // navigating page.
        auto* rfhi = static_cast<RenderFrameHostImpl*>(rfh);
        const SiteInfo& site_info =
            rfhi->last_committed_url_derived_site_info();
        int matching_doc_count =
            rfhi->GetSiteInstance()->GetActiveDocumentCount(site_info);
        int matching_doc_in_other_pages_count =
            matching_doc_count - doc_count_in_page[site_info];
        if (matching_doc_in_other_pages_count > 0) {
          // The document shares a SiteInfo with another tab. This means the
          // contents of this document might be synchronously accessible by
          // a document in another tab (either because the documents are
          // same-origin, or through modifying document.domain), so note down
          // this information.
          related_active_contents_sync_access_info_ =
              RelatedActiveContentsSyncAccessInfo::kPotentiallySyncAccessible;
          // Once we've found a case where sync access is possible, we can stop,
          // as we've reached the maximum value for the enum
          // (kPotentiallySyncAccessible).
          return RenderFrameHost::FrameIterationAction::kStop;
        }
        return RenderFrameHost::FrameIterationAction::kContinue;
      });
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
    case ShouldSwapBrowsingInstance::kNo_NotPrimaryMainFrame:
    case ShouldSwapBrowsingInstance::kNo_InitiatorRequestedNoProactiveSwap:
      return false;
    case ShouldSwapBrowsingInstance::kYes_ForceSwap:
    case ShouldSwapBrowsingInstance::kYes_CrossSiteProactiveSwap:
    case ShouldSwapBrowsingInstance::kYes_SameSiteProactiveSwap:
      return true;
  }
}

std::string BackForwardCacheMetrics::GetPageStoredResultString() {
  return page_store_result_->ToString();
}

}  // namespace content
