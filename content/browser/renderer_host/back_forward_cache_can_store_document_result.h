// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_CAN_STORE_DOCUMENT_RESULT_H_
#define CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_CAN_STORE_DOCUMENT_RESULT_H_

#include <bitset>
#include <cstdint>
#include <set>

#include "base/containers/enum_set.h"
#include "base/trace_event/typed_macros.h"
#include "content/browser/renderer_host/back_forward_cache_metrics.h"
#include "content/browser/renderer_host/should_swap_browsing_instance.h"
#include "content/common/content_export.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "ui/accessibility/ax_event.h"

namespace content {

using BlockListedFeatures = blink::scheduler::WebSchedulerTrackedFeatures;
using ChromeTrackEvent = perfetto::protos::pbzero::ChromeTrackEvent;

// Represents the result whether the page could be stored in the back-forward
// cache with the reasons.
// TODO(rakina): Rename this to use "Page" instead of "Document", to follow
// the naming of BackForwardCacheImpl::CanStorePageNow().
class CONTENT_EXPORT BackForwardCacheCanStoreDocumentResult {
 public:
  using NotStoredReasons =
      base::EnumSet<BackForwardCacheMetrics::NotRestoredReason,
                    BackForwardCacheMetrics::NotRestoredReason::kMinValue,
                    BackForwardCacheMetrics::NotRestoredReason::kMaxValue>;

  BackForwardCacheCanStoreDocumentResult();
  BackForwardCacheCanStoreDocumentResult(
      BackForwardCacheCanStoreDocumentResult&);
  BackForwardCacheCanStoreDocumentResult(
      BackForwardCacheCanStoreDocumentResult&&);
  ~BackForwardCacheCanStoreDocumentResult();

  // Add reasons contained in the |other| to |this|.
  void AddReasonsFrom(const BackForwardCacheCanStoreDocumentResult& other);
  bool HasNotStoredReason(
      BackForwardCacheMetrics::NotRestoredReason reason) const;

  void No(BackForwardCacheMetrics::NotRestoredReason reason);
  void NoDueToFeatures(BlockListedFeatures features);

  void NoDueToRelatedActiveContents(
      absl::optional<ShouldSwapBrowsingInstance> browsing_instance_swap_result);

  // TODO(hajimehoshi): Replace the arbitrary strings with base::Location /
  // FROM_HERE for privacy reasons.
  void NoDueToDisableForRenderFrameHostCalled(
      const std::set<BackForwardCache::DisabledReason>& reasons);
  void NoDueToDisallowActivation(uint64_t reason);
  void NoDueToAXEvents(const std::vector<ui::AXEvent>& events);
  void RecordAXEvent(ax::mojom::Event event_type);

  bool CanStore() const;
  operator bool() const { return CanStore(); }

  const NotStoredReasons& not_stored_reasons() const {
    return not_stored_reasons_;
  }
  BlockListedFeatures blocklisted_features() const {
    return blocklisted_features_;
  }

  const std::set<BackForwardCache::DisabledReason>& disabled_reasons() const {
    return disabled_reasons_;
  }

  const std::set<uint64_t>& disallow_activation_reasons() const {
    return disallow_activation_reasons_;
  }

  const std::set<ax::mojom::Event>& ax_events() const { return ax_events_; }

  std::string ToString() const;

  void WriteIntoTrace(
      perfetto::TracedProto<
          perfetto::protos::pbzero::BackForwardCacheCanStoreDocumentResult>
          result) const;

 private:
  void AddNotStoredReason(BackForwardCacheMetrics::NotRestoredReason reason);
  std::string NotRestoredReasonToString(
      BackForwardCacheMetrics::NotRestoredReason reason) const;

  NotStoredReasons not_stored_reasons_;
  BlockListedFeatures blocklisted_features_;
  std::set<BackForwardCache::DisabledReason> disabled_reasons_;
  absl::optional<ShouldSwapBrowsingInstance> browsing_instance_swap_result_;
  std::set<uint64_t> disallow_activation_reasons_;
  // The list of the accessibility events that made the page bfcache ineligible.
  std::set<ax::mojom::Event> ax_events_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_CAN_STORE_DOCUMENT_RESULT_H_
