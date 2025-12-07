// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_navigation_metrics_manager.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "content/renderer/render_thread_impl.h"

namespace content {

namespace {

// If a navigation hasn't completed within this timeout, its timeline data will
// be cleaned up. See also cleanup comments in GetOrCreateTimeline() below.
constexpr base::TimeDelta kLazyCleanupTimeout = base::Seconds(300);

// Kill switch for `RendererNavigationMetricsManager`'s generation of renderer
// trace events and metrics, in case they cause any unexpected overhead or other
// issues. See https://crbug.com/415821826.
BASE_FEATURE(kEnableRendererNavigationTimeline,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Used to record how ready a renderer process is for an incoming
// CommitNavigation IPC. Please keep in sync with "RendererProcessReadiness" in
// tools/metrics/histograms/metadata/navigation/enums.xml. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
//
// LINT.IfChange(RendererProcessReadiness)
enum class RendererProcessReadiness {
  // The commit IPC was sent before the process was ready. This implies that
  // we should've started the renderer process earlier.
  kProcessNotReady = 0,
  // Renderer was ready, but the preparatory work of creating the
  // view/proxy/frame objects wasn't complete before the commit IPC was sent.
  // This implies that the speculative RenderFrameHost should've been created
  // earlier.
  kViewProxyFrameNotReady = 1,
  // Renderer was ready to process the commit IPC right away.
  kReadyToProcessCommitIPC = 2,

  kMaxValue = kReadyToProcessCommitIPC
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/navigation/enums.xml:RendererProcessReadiness)

void RecordProcessReadiness(bool is_main_frame,
                            RendererProcessReadiness readiness) {
  base::UmaHistogramEnumeration("Navigation.Renderer.ProcessReadiness",
                                readiness);
  if (is_main_frame) {
    base::UmaHistogramEnumeration(
        "Navigation.Renderer.ProcessReadiness.MainFrameOnly", readiness);
  }

  std::string readiness_description;
  switch (readiness) {
    case RendererProcessReadiness::kProcessNotReady:
      readiness_description = "Process not ready";
      break;
    case RendererProcessReadiness::kViewProxyFrameNotReady:
      readiness_description = "View/proxy/frame not ready";
      break;
    case RendererProcessReadiness::kReadyToProcessCommitIPC:
      readiness_description = "Ready to process commit IPC";
      break;
  }
  // Also emit an instant trace event to expose process readiness in traces.
  TRACE_EVENT_INSTANT(
      "navigation",
      "RendererNavigationMetricsManager::RecordTraceEventsAndMetrics",
      "ProcessReadiness", readiness_description);
}

}  // namespace

RendererNavigationMetricsManager& RendererNavigationMetricsManager::Instance() {
  static base::NoDestructor<RendererNavigationMetricsManager> manager;
  return *manager.get();
}

RendererNavigationMetricsManager::RendererNavigationMetricsManager() = default;
RendererNavigationMetricsManager::~RendererNavigationMetricsManager() = default;

RendererNavigationMetricsManager::Timeline::Timeline() = default;
RendererNavigationMetricsManager::Timeline::~Timeline() = default;

RendererNavigationMetricsManager::Timeline&
RendererNavigationMetricsManager::GetOrCreateTimeline(
    const base::UnguessableToken& navigation_metrics_token) {
  auto it = timelines_.find(navigation_metrics_token);
  if (it != timelines_.end()) {
    return it->second;
  }

  // Create a new Timeline object for this navigation.
  Timeline& timeline = timelines_[navigation_metrics_token];

  // Post a task to clean up the new timeline after a timeout, if the
  // corresponding navigation hasn't finished by then.
  //
  // Ideally, we would detect navigation cancellation and remove the timeline at
  // that point. Unfortunately, this is not as easy as it seems:
  // - IPCs for destroying previously created proxies, views, and provisional
  //   frames would all have to identify which navigation they pertained to, if
  //   any, which would add a lot of complexity.
  // - detecting NavigationClient interface disconnection seems promising, but
  //   unfortunately doesn't work for cross-process browser-initiated
  //   navigations where the commit NavigationClient isn't set up until
  //   ready-to-commit time, after proxy/view creation.
  //
  // As a longer-term solution, it might be possible to set up the commit
  // NavigationClient earlier and the corresponding `Timeline` directly on it.
  // However, this will also be tricky, since the current NavigationClient
  // lifetime also impacts web compatibility, by defining when a
  // renderer-initiated navigation may be canceled by JavaScript - see comments
  // on `RenderFrameImpl::navigation_client_impl_`.
  timeline.lazy_cleanup_timer_.Start(
      FROM_HERE, kLazyCleanupTimeout,
      base::BindOnce(
          [](const base::UnguessableToken& token) {
            RendererNavigationMetricsManager::Instance().timelines_.erase(
                token);
          },
          navigation_metrics_token));

  // Remember that this process has started at least one navigation.
  timeline.is_first_navigation_in_this_process = !has_first_navigation_started_;
  has_first_navigation_started_ = true;

  return timeline;
}

void RendererNavigationMetricsManager::AddCreateViewEvent(
    const std::optional<base::UnguessableToken>& navigation_metrics_token,
    const base::TimeTicks& start_time,
    const base::TimeDelta& elapsed_time) {
  if (!base::FeatureList::IsEnabled(kEnableRendererNavigationTimeline)) {
    return;
  }

  // Don't record any metrics if this event was not for a navigation.
  if (!navigation_metrics_token) {
    return;
  }

  auto& timeline = GetOrCreateTimeline(*navigation_metrics_token);
  timeline.create_view_events.emplace_back(start_time,
                                           start_time + elapsed_time);
}

void RendererNavigationMetricsManager::AddCreateRemoteChildrenEvent(
    const std::optional<base::UnguessableToken>& navigation_metrics_token,
    const base::TimeTicks& start_time,
    const base::TimeDelta& elapsed_time) {
  if (!base::FeatureList::IsEnabled(kEnableRendererNavigationTimeline)) {
    return;
  }

  // Don't record any metrics if this event was not for a navigation.
  if (!navigation_metrics_token) {
    return;
  }

  auto& timeline = GetOrCreateTimeline(*navigation_metrics_token);
  timeline.create_remote_children_events.emplace_back(
      start_time, start_time + elapsed_time);
}

void RendererNavigationMetricsManager::AddCreateFrameEvent(
    const std::optional<base::UnguessableToken>& navigation_metrics_token,
    const base::TimeTicks& start_time,
    const base::TimeDelta& elapsed_time) {
  if (!base::FeatureList::IsEnabled(kEnableRendererNavigationTimeline)) {
    return;
  }

  // Don't record any metrics if this event was not for a navigation.
  if (!navigation_metrics_token) {
    return;
  }

  auto& timeline = GetOrCreateTimeline(*navigation_metrics_token);

  // Typically, there's one CreateFrame call per navigation, corresponding to
  // the provisional frame that will eventually commit the navigation. However,
  // it's possible that a navigation will pick a different RenderFrame at
  // response time, which could end up being created in the same renderer
  // process. In this case, for now, capture the start/end times of the latest
  // CreateFrame call (which is more relevant for navigation latency), by
  // overwriting the start/end times if they already exist. In the future,
  // these calls could potentially be tracked as separate events.
  timeline.create_frame_event.emplace(start_time, start_time + elapsed_time);
}

void RendererNavigationMetricsManager::MarkCommitStart(
    const base::UnguessableToken& navigation_metrics_token) {
  if (!base::FeatureList::IsEnabled(kEnableRendererNavigationTimeline)) {
    return;
  }

  GetOrCreateTimeline(navigation_metrics_token).commit_start =
      base::TimeTicks().Now();
}

void RendererNavigationMetricsManager::RecordTraceEventsAndMetrics(
    const RendererNavigationMetricsManager::Timeline& timeline,
    const GURL& url) {
  CHECK(!timeline.navigation_start.is_null())
      << "Navigation start time not found for " << url;

  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  // The `render_thread` may be null in tests.
  if (!render_thread) {
    return;
  }

  // Record these trace events in a global "Navigations" track, so that it can
  // be found under "Global Track Events". This complements events logged
  // from the browser process into the same track.
  constexpr uint64_t kGlobalInstantTrackId = 0;
  static perfetto::NamedTrack track(
      "Navigation: Timelines (Renderer)",
      base::trace_event::GetNextGlobalTraceId(),
      perfetto::Track::Global(kGlobalInstantTrackId));

  // Define a helper to log both a trace event slice and a corresponding metric
  // for one stage of a navigation.
  //
  // Note: A similar helper exists to log browser-side navigation timeline
  // events in RecordNavigationTraceEventsAndMetrics(). When adding new code
  // here, consider whether the browser-side helper also needs to be updated.
  // It might be desirable to merge the two helpers in the future.
  auto log_trace_event_and_uma =
      [&](perfetto::StaticString name, const base::TimeTicks& begin_time,
          const base::TimeTicks& end_time,
          const std::optional<std::string>& histogram_name = std::nullopt,
          const std::optional<std::string>& url = std::nullopt) {
        if (begin_time.is_null() || end_time.is_null()) {
          return;
        }

        TRACE_EVENT_BEGIN(
            "navigation", name, track, begin_time,
            [&](perfetto::EventContext& ctx) {
              if (!url.has_value()) {
                return;
              }
              perfetto::protos::pbzero::PageLoad* page_load =
                  ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                      ->set_page_load();
              page_load->set_url(*url);
            });
        TRACE_EVENT_END("navigation", track, end_time);

        // When provided, `histogram_name` is used to avoid including variable
        // or sensitive data in the reported metric name. For example, `name`
        // may include the navigation URL when measuring the start-to-finish
        // time, but we only want to use that for trace events and omit the
        // URL in metric names for UMA.
        base::UmaHistogramTimes(
            base::StrCat({"Navigation.Renderer.Timeline.",
                          histogram_name.value_or(std::string(name.value)),
                          ".Duration"}),
            end_time - begin_time);
      };

  // Actual navigation events are logged below in contiguous (or nested)
  // intervals.
  // TODO(crbug.com/405437928): Overlapping navigations may incorrectly appear
  // to be nested, using the wrong end times.
  log_trace_event_and_uma("Renderer Navigation", timeline.navigation_start,
                          timeline.commit_end,
                          /*histogram_name=*/"Total");

  // Emit a trace event with url in the name for convenience. Do this in a
  // separate trace event from the one above, since events with dynamic strings
  // are filtered out in some traces, and the event above would still be useful
  // in that case.
  // TODO(crbug.com/415720503): Remove once Perfetto navigation plugins surfaces
  // urls.
  std::string top_level_trace_event_name = "URL: " + url.spec();
  TRACE_EVENT_BEGIN("navigation",
                    perfetto::DynamicString(top_level_trace_event_name), track,
                    timeline.navigation_start);
  TRACE_EVENT_END("navigation", track, timeline.commit_end);

  // It's possible that the process was still starting when the navigation
  // started. In that case, record an event which measures the time for the
  // process to finish starting up and become ready for processing IPCs, and
  // treat that point as the starting point for the next event.
  base::TimeTicks process_ready_time = render_thread->run_loop_start_time();
  if (timeline.navigation_start < process_ready_time) {
    log_trace_event_and_uma("WaitingForProcessReady", timeline.navigation_start,
                            process_ready_time);
  } else if (timeline.is_first_navigation_in_this_process) {
    // If this was the first navigation in this renderer process, and the
    // process was ready before the navigation started, record a zero-sized
    // event for WaitingForProcessReady. This allows measuring how much of a
    // problem process startup costs are for navigations in a freshly created
    // process.
    log_trace_event_and_uma("WaitingForProcessReady", timeline.navigation_start,
                            timeline.navigation_start);
  }

  // Create an event for each CreateView IPC. There could be multiple if there
  // are multiple pages on the navigating frame's opener chain. There could also
  // be no CreateView IPCs at all, for example if a subframe navigates
  // same-origin.
  for (const auto& event : timeline.create_view_events) {
    log_trace_event_and_uma("CreateView", event.start, event.end);
  }

  // Create an event for each CreateRemoteChildren IPC, which creates all
  // subframe proxies for a particular page/frame tree. There could be multiple
  // of these IPCs per navigation if there are multiple pages on the navigating
  // frame's opener chain. Note that main frame proxies are created as part of
  // CreateView, and while there is also a CreateRemoteChild IPC to create
  // an individual proxy, it is not currently used in the navigation flow, so it
  // is not traced here.
  for (const auto& event : timeline.create_remote_children_events) {
    log_trace_event_and_uma("CreateChildProxies", event.start, event.end);
  }

  // Create an event for processing the CreateFrame IPC. Note that CreateFrame
  // may not happen if the navigation is staying in a previous RenderFrame, e.g.
  // for browser-initiated same-document navigations, navigations out of the
  // initial empty document, or same-site navigations when RenderDocument is
  // turned off.
  if (timeline.create_frame_event) {
    log_trace_event_and_uma("CreateFrame", timeline.create_frame_event->start,
                            timeline.create_frame_event->end);
  }

  // Record a metric to measure how ready the renderer process is to process a
  // navigation commit IPC.

  if (timeline.commit_sent < process_ready_time) {
    // The commit IPC was sent before the process was ready. This implies that
    // we should've started the renderer process earlier.
    RecordProcessReadiness(timeline.is_main_frame,
                           RendererProcessReadiness::kProcessNotReady);
  } else if (timeline.create_frame_event &&
             timeline.commit_sent < timeline.create_frame_event->end) {
    // Renderer was ready, but the prerequisite work of creating the
    // view/proxy/frame objects wasn't completed before the commit IPC was sent.
    // This implies that the speculative RenderFrameHost should've been created
    // earlier. Note that CreateFrame's end time is used here because it is the
    // last view/proxy/frame creation IPC to be processed; not having
    // CreateFrame for this navigation implies there also was no view or proxy
    // creation, and the navigation commit wasn't blocked waiting for any of
    // them.
    RecordProcessReadiness(timeline.is_main_frame,
                           RendererProcessReadiness::kViewProxyFrameNotReady);
  } else {
    // Renderer was ready to process the commit IPC without waiting for process
    // startup or prerequisite frame/proxy/view creation IPCs.
    RecordProcessReadiness(timeline.is_main_frame,
                           RendererProcessReadiness::kReadyToProcessCommitIPC);
  }

  log_trace_event_and_uma("CommitToDidCommit", timeline.commit_start,
                          timeline.commit_end);
}

void RendererNavigationMetricsManager::ProcessNavigationCommit(
    const base::UnguessableToken& navigation_metrics_token,
    const GURL& url,
    const base::TimeTicks& navigation_start_time,
    const base::TimeTicks& commit_sent_time,
    bool is_main_frame) {
  if (!base::FeatureList::IsEnabled(kEnableRendererNavigationTimeline)) {
    return;
  }

  auto it = timelines_.find(navigation_metrics_token);
  // The timeline may not exist for synchronous about:blank commits or
  // renderer-initiated same-document navigations. For now, do not record
  // anything for these cases.
  if (it == timelines_.end()) {
    return;
  }

  auto& timeline = it->second;
  timeline.navigation_start = navigation_start_time;
  timeline.commit_sent = commit_sent_time;
  timeline.commit_end = base::TimeTicks().Now();
  timeline.is_main_frame = is_main_frame;
  RecordTraceEventsAndMetrics(timeline, url);

  // Remove the timeline from the map and cancel the cleanup timer.
  timeline.lazy_cleanup_timer_.Stop();
  timelines_.erase(it);
}

}  // namespace content
