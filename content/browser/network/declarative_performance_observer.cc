// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/declarative_performance_observer.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

namespace {
constexpr char kDeclarativePerformanceObserverReportType[] =
    "performance-observer";
}  // namespace

DeclarativePerformanceObserver::DeclarativePerformanceObserver(
    WebContents* web_contents)
    : WebContentsObserver(web_contents),
      WebContentsUserData<DeclarativePerformanceObserver>(*web_contents) {
  started_in_foreground_ = web_contents->GetVisibility() == Visibility::VISIBLE;
}

DeclarativePerformanceObserver::~DeclarativePerformanceObserver() = default;

void DeclarativePerformanceObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsErrorPage()) {
    return;
  }

  RenderFrameHost* new_rfh = navigation_handle->GetRenderFrameHost();

  if (navigation_handle->IsServedFromBackForwardCache()) {
    navigation_start_ = navigation_handle->NavigationStart();
    buffered_entries_.clear();
    active_rfh_ = new_rfh;

    if (enabled_types_.contains(
            network::mojom::PerformanceEntryType::kNavigation)) {
      base::DictValue nav_entry;
      nav_entry.Set("name", committed_url_.spec());
      nav_entry.Set("entryType", "navigation");
      nav_entry.Set("type", "back_forward");
      nav_entry.Set("startTime", 0.0);
      AddEntryToBuffer(std::move(nav_entry));
    }

    if (enabled_types_.contains(
            network::mojom::PerformanceEntryType::kVisibilityState)) {
      base::DictValue visibility_entry;
      visibility_entry.Set("name", "visible");
      visibility_entry.Set("entryType", "visibility-state");
      visibility_entry.Set("startTime", 0.0);
      visibility_entry.Set("duration", 0.0);
      AddEntryToBuffer(std::move(visibility_entry));
    }
    return;
  }

  if (active_rfh_ && active_rfh_ != new_rfh) {
    FlushMetrics(active_rfh_);
    active_rfh_ = nullptr;
  }

  const network::mojom::DeclarativePerformanceObserverPolicy* policy =
      navigation_handle->GetDeclarativePerformanceObserverPolicy();

  if (!policy || !policy->reporting_endpoint || policy->entry_types.empty()) {
    return;
  }

  reporting_endpoint_ = *policy->reporting_endpoint;
  enabled_types_ = base::flat_set<network::mojom::PerformanceEntryType>(
      policy->entry_types.begin(), policy->entry_types.end());

  navigation_start_ = navigation_handle->NavigationStart();
  committed_url_ = navigation_handle->GetURL();

  if (new_rfh) {
    network_anonymization_key_ =
        new_rfh->GetIsolationInfoForSubresources().network_anonymization_key();
    reporting_source_ = new_rfh->GetReportingSource();
    active_rfh_ = new_rfh;

    if (enabled_types_.contains(
            network::mojom::PerformanceEntryType::kVisibilityState)) {
      base::DictValue entry;
      entry.Set("name", started_in_foreground_ ? "visible" : "hidden");
      entry.Set("entryType", "visibility-state");
      entry.Set("startTime", 0.0);
      entry.Set("duration", 0.0);
      AddEntryToBuffer(std::move(entry));
    }
  }
}

void DeclarativePerformanceObserver::OnVisibilityChanged(
    Visibility visibility) {
  if (enabled_types_.contains(
          network::mojom::PerformanceEntryType::kVisibilityState)) {
    base::DictValue entry;
    entry.Set("name", visibility == Visibility::HIDDEN ? "hidden" : "visible");
    entry.Set("entryType", "visibility-state");
    base::TimeDelta relative_time = base::TimeTicks::Now() - navigation_start_;
    entry.Set("startTime", relative_time.InMillisecondsF());
    entry.Set("duration", 0.0);
    AddEntryToBuffer(std::move(entry));
  }
}

void DeclarativePerformanceObserver::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  if (active_rfh_ == render_frame_host) {
    FlushMetrics(render_frame_host);
    active_rfh_ = nullptr;
  }
}

void DeclarativePerformanceObserver::RenderFrameHostStateChanged(
    RenderFrameHost* render_frame_host,
    RenderFrameHost::LifecycleState old_state,
    RenderFrameHost::LifecycleState new_state) {
  if (render_frame_host == active_rfh_ &&
      old_state == RenderFrameHost::LifecycleState::kActive &&
      new_state == RenderFrameHost::LifecycleState::kInBackForwardCache) {
    if (enabled_types_.contains(
            network::mojom::PerformanceEntryType::kVisibilityState) ||
        enabled_types_.contains(
            network::mojom::PerformanceEntryType::kNavigation)) {
      base::DictValue entry;
      entry.Set("name", "session-end-event");
      entry.Set("entryType", "session-end");
      base::TimeDelta relative_time =
          base::TimeTicks::Now() - navigation_start_;
      entry.Set("startTime", relative_time.InMillisecondsF());
      entry.Set("duration", 0.0);
      AddEntryToBuffer(std::move(entry));
    }

    FlushMetrics(render_frame_host);
    active_rfh_ = nullptr;
  }
}

void DeclarativePerformanceObserver::SetStoragePartitionForTesting(  // IN-TEST
    StoragePartition* storage_partition) {
  storage_partition_for_testing_ = storage_partition;
}

void DeclarativePerformanceObserver::FlushMetrics(RenderFrameHost* rfh) {
  if (buffered_entries_.empty()) {
    return;
  }

  base::DictValue body;
  body.Set("entries", std::move(buffered_entries_));
  buffered_entries_.clear();

  StoragePartition* storage_partition =
      storage_partition_for_testing_
          ? storage_partition_for_testing_.get()
          : (rfh ? rfh->GetStoragePartition() : nullptr);

  if (storage_partition) {
    storage_partition->GetNetworkContext()->QueueReport(
        kDeclarativePerformanceObserverReportType, reporting_endpoint_,
        committed_url_, reporting_source_, network_anonymization_key_,
        std::move(body));
  }
}

void DeclarativePerformanceObserver::AddEntryToBuffer(base::DictValue entry) {
  buffered_entries_.Append(std::move(entry));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DeclarativePerformanceObserver);

}  // namespace content
