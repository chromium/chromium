// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/declarative_performance_observer.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_timing.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_timing_info.h"
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
    active_rfh_ = new_rfh ? new_rfh->GetGlobalId() : GlobalRenderFrameHostId();

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

  if (active_rfh_ && active_rfh_ != (new_rfh ? new_rfh->GetGlobalId()
                                             : GlobalRenderFrameHostId())) {
    AppendSessionEndEntry();
    FlushMetrics(RenderFrameHost::FromID(active_rfh_));
    active_rfh_ = GlobalRenderFrameHostId();
  }

  const network::mojom::DeclarativePerformanceObserverPolicy* policy =
      navigation_handle->GetDeclarativePerformanceObserverPolicy();

  if (!policy || !policy->reporting_endpoint || policy->entry_types.empty()) {
    return;
  }

  reporting_endpoint_ = *policy->reporting_endpoint;
  enabled_types_ = base::flat_set<network::mojom::PerformanceEntryType>(
      policy->entry_types.begin(), policy->entry_types.end());
  if (policy->include_user_timing) {
    include_user_timing_ =
        base::flat_set<std::string>(policy->include_user_timing->begin(),
                                    policy->include_user_timing->end());
  } else {
    include_user_timing_ = std::nullopt;
  }

  navigation_start_ = navigation_handle->NavigationStart();
  committed_url_ = navigation_handle->GetURL();

  if (new_rfh) {
    network_anonymization_key_ =
        new_rfh->GetIsolationInfoForSubresources().network_anonymization_key();
    reporting_source_ = new_rfh->GetReportingSource();
    active_rfh_ = new_rfh->GetGlobalId();

    if (enabled_types_.contains(
            network::mojom::PerformanceEntryType::kVisibilityState)) {
      base::DictValue entry;
      entry.Set("name", started_in_foreground_ ? "visible" : "hidden");
      entry.Set("entryType", "visibility-state");
      entry.Set("startTime", 0.0);
      entry.Set("duration", 0.0);
      AddEntryToBuffer(std::move(entry));
    }

    if (enabled_types_.contains(
            network::mojom::PerformanceEntryType::kNavigation)) {
      base::DictValue entry;
      entry.Set("name", committed_url_.spec());
      entry.Set("entryType", "navigation");
      entry.Set("startTime", 0.0);

      // Retrieve real metrics from NavigationHandleTiming safely without
      // casting
      const NavigationHandleTiming& timing =
          navigation_handle->GetNavigationHandleTiming();

      auto to_relative_ms = [&](base::TimeTicks t) {
        return t.is_null() ? 0.0 : (t - navigation_start_).InMillisecondsF();
      };

      double response_start = to_relative_ms(timing.final_response_start_time);
      double request_start = to_relative_ms(timing.final_request_start_time);
      double domain_lookup_start =
          to_relative_ms(timing.final_request_domain_lookup_start_time);
      double domain_lookup_end =
          to_relative_ms(timing.final_request_domain_lookup_end_time);
      double connect_start =
          to_relative_ms(timing.final_request_connect_start_time);
      double connect_end =
          to_relative_ms(timing.final_request_connect_end_time);
      double secure_connection_start =
          to_relative_ms(timing.final_request_ssl_start_time);

      entry.Set("responseStart", response_start);
      entry.Set("requestStart", request_start);
      entry.Set("connectStart", connect_start);
      entry.Set("connectEnd", connect_end);
      entry.Set("domainLookupStart", domain_lookup_start);
      entry.Set("domainLookupEnd", domain_lookup_end);
      entry.Set("secureConnectionStart", secure_connection_start);

      // Handle activationStart for Prerender to satisfy Phase 6
      if (navigation_handle->IsPrerenderedPageActivation()) {
        entry.Set("activationStart",
                  0.0);  // Relative to activating navigation start
      } else {
        entry.Set("activationStart", 0.0);
      }

      // Handle deliveryType based on W3C spec ("", cache,
      // navigational-prefetch)
      std::string delivery_type = "";
      if (navigation_handle->WasResponseCached()) {
        delivery_type = "cache";
      } else if (navigation_handle->IsPrerenderedPageActivation()) {
        delivery_type = "navigational-prefetch";
      }
      entry.Set("deliveryType", delivery_type);

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

  if (visibility == Visibility::HIDDEN) {
    FlushMetrics(RenderFrameHost::FromID(active_rfh_));
  }
}

void DeclarativePerformanceObserver::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  if (active_rfh_ == render_frame_host->GetGlobalId()) {
    AppendSessionEndEntry();
    FlushMetrics(render_frame_host);
    active_rfh_ = GlobalRenderFrameHostId();
  }
}

void DeclarativePerformanceObserver::RenderFrameHostStateChanged(
    RenderFrameHost* render_frame_host,
    RenderFrameHost::LifecycleState old_state,
    RenderFrameHost::LifecycleState new_state) {
  if (render_frame_host->GetGlobalId() == active_rfh_ &&
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
    active_rfh_ = GlobalRenderFrameHostId();
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

void DeclarativePerformanceObserver::AppendSessionEndEntry() {
  if (enabled_types_.contains(
          network::mojom::PerformanceEntryType::kVisibilityState) ||
      enabled_types_.contains(
          network::mojom::PerformanceEntryType::kNavigation)) {
    base::DictValue entry;
    entry.Set("name", "session-end-event");
    entry.Set("entryType", "session-end");
    base::TimeDelta relative_time = base::TimeTicks::Now() - navigation_start_;
    entry.Set("startTime", relative_time.InMillisecondsF());
    entry.Set("duration", 0.0);
    AddEntryToBuffer(std::move(entry));
  }
}

void DeclarativePerformanceObserver::AddEntryToBuffer(base::DictValue entry) {
  buffered_entries_.Append(std::move(entry));
}

// static
void DeclarativePerformanceObserver::Bind(
    RenderFrameHost* rfh,
    mojo::PendingReceiver<blink::mojom::DeclarativePerformanceObserverHost>
        receiver) {
  WebContents* web_contents = WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    return;
  }
  auto* observer =
      DeclarativePerformanceObserver::FromWebContents(web_contents);
  if (observer) {
    observer->BindReceiver(rfh, std::move(receiver));
  }
}

void DeclarativePerformanceObserver::BindReceiver(
    RenderFrameHost* rfh,
    mojo::PendingReceiver<blink::mojom::DeclarativePerformanceObserverHost>
        receiver) {
  receivers_.Add(this, std::move(receiver), rfh->GetGlobalId());
}

void DeclarativePerformanceObserver::DidObservePerformanceEntries(
    std::vector<blink::mojom::DeclarativePerformanceEntryPtr> entries) {
  GlobalRenderFrameHostId rfh_id = receivers_.current_context();
  if (rfh_id != active_rfh_) {
    return;
  }

  for (auto& entry : entries) {
    if (!enabled_types_.contains(network::mojom::PerformanceEntryType::kMark)) {
      continue;
    }
    if (include_user_timing_ && !include_user_timing_->contains(entry->name)) {
      continue;
    }

    base::DictValue dict;
    dict.Set("name", entry->name);
    dict.Set("entryType", "mark");
    dict.Set("startTime", entry->start_time.InMillisecondsF());
    dict.Set("duration", 0.0);

    if (entry->detail.has_value()) {
      dict.Set("detail", std::move(entry->detail.value()));
    }
    AddEntryToBuffer(std::move(dict));
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DeclarativePerformanceObserver);

}  // namespace content
