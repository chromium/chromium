// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_NAVIGATION_METRICS_MANAGER_H_
#define CONTENT_RENDERER_RENDERER_NAVIGATION_METRICS_MANAGER_H_

#include <map>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

// This class collects start and end times of different events pertaining to
// navigations in the renderer process. These events include RenderView/WebView
// creation, proxy creation, (possibly provisional) frame creation, and
// navigation commit. The main goal is to output a trace with the renderer's
// view of navigation events, as well as to record metrics for durations of
// those events. This is done by calling `RecordTraceEventsAndMetrics()` when a
// navigation finishes committing. The
// RendererNavigationMetricsManager::Timeline struct below keeps all the
// necessary timestamps for a single navigation.
//
// Multiple navigations may be in progress in the same renderer process, so this
// class keeps a map of Timelines keyed by "navigation metrics tokens", which
// are passed in by the browser process in IPCs related to navigations. Each
// navigation has a unique navigation metrics token, with the source of truth
// stored in the NavigationRequest.
//
// If a navigation starts creating view/frame/proxy objects but ends up not
// committing in this process, such as after a cross-process redirect or if the
// navigation is explicitly canceled by the user or another navigation, its
// timeline data is cleaned up lazily. In particular, a navigation's timestamps
// are cleaned up if the navigation hasn't committed after a 5-minute timeout -
// see note in RendererNavigationMetricsManager::GetOrCreateTimeline() about how
// this works, and why using explicit cancellation signals is hard. The goal is
// to change this into explicit cleanup in the future by improving signals for
// navigation cancellations and making them aware of navigation metrics tokens.
//
// There is one global RendererNavigationMetricsManager object in each renderer
// process, accessible via the `Instance()` method below.
class CONTENT_EXPORT RendererNavigationMetricsManager {
 public:
  RendererNavigationMetricsManager();

  static RendererNavigationMetricsManager& Instance();

  // This struct keeps a set of timestamps for performing a particular
  // navigation in the renderer process. It is created when the very first IPC
  // associated with this navigation is processed, which is typically
  // CreateView or CreateFrame.
  struct Timeline {
    Timeline();
    ~Timeline();

    // The time at which this navigation was started, without any beforeunload
    // adjustments. Note that navigation start might have happened in another
    // process (e.g., browser process or another renderer process).
    base::TimeTicks navigation_start;

    // Helper struct that holds a start/end time for a particular event.
    struct TimelineEvent {
      base::TimeTicks start;
      base::TimeTicks end;
    };

    // A set of <start, end> timestamps for processing all CreateView IPCs for
    // this navigation. These IPCs set up the blink::WebView and main frame
    // frame or proxy for the navigating frame and its opener Pages/FrameTrees,
    // if any.
    std::vector<TimelineEvent> create_view_events;

    // A set of <start, end> timestamps for processing all CreateRemoteChildren
    // IPCs that create all necessary subframe proxies for this navigation.
    // There is one pair  of timestamps for each Page/FrameTree that needed
    // subframe proxies, including the navigating frame's page as well as
    // its opener chain.
    std::vector<TimelineEvent> create_remote_children_events;

    // Start and end times for the CreateFrame IPC. This should exist for most
    // navigations, but could end up nullopt when a navigation stays in the same
    // RenderFrame, such as for same-document navigations or navigations out of
    // an initial blank document.
    std::optional<TimelineEvent> create_frame_event;

    // The time at which the CommitNavigation IPC was sent to this renderer
    // process from the browser process.
    base::TimeTicks commit_sent;

    // The time at which this navigation started processing the CommitNavigation
    // IPC.
    base::TimeTicks commit_start;

    // The time at which this navigation finished processing the
    // CommitNavigation IPC.
    base::TimeTicks commit_end;

    // A timer that's set at the time this Timeline object is created (i.e.,
    // when the very first IPC for this navigation is processed), to clean up
    // the Timeline if this navigation ends up never committing. See a note
    // about this in RendererNavigationMetricsManager::GetOrCreateTimeline().
    base::OneShotTimer lazy_cleanup_timer_;

    // Whether this Timeline is for the first navigation in this renderer
    // process. This is needed to report a single zero-sized
    // WaitingForProcessReady event in cases that the renderer is ready before
    // the first navigation begins, to give a sense of how often a process
    // is ready to go when it's needed for a navigation.
    bool is_first_navigation_in_this_process;

    // Whether this navigation was in a main frame, as defined by
    // RenderFrameImpl::IsMainFrame(). Useful for recording metrics for main
    // frames only. Note that this is not limited to outermost or primary main
    // frames.
    bool is_main_frame;
  };

  // The following methods are called to add timestamps for processing the
  // CreateView, CreateRemoteChildren, and CreateFrame IPCs, for a navigation
  // identified by `navigation_metrics_token`. Note that there might be multiple
  // CreateView and CreateRemoteChildren IPCs per navigation, since one is sent
  // for each page/FrameTree, and there could be multiple pages involved with
  // opener chains. In this case, AddCreateViewEvent and
  // AddCreateRemoteChildrenEvent might be called multiple times and will record
  // a list of <start,end> timestamps.
  //
  // The normal expected sequence of these events is:
  // - each page (if any) on the opener chain will generate a CreateView event
  //   followed by a CreateRemoteChildren event for any subframe proxies on that
  //   page.
  // - the page with the navigating frame will generate an optional CreateView
  //   event (if a main frame/proxy needs to be created), followed by an
  //   optional CreateRemoteChildren event (if any subframe proxies need to be
  //   created).
  // - the navigating frame will generate an optional CreateFrame event (when it
  //   navigates in a new RenderFrame).
  //
  // Note that a navigation could involve no CreateView IPCs at all (e.g., in
  // same-origin navigations). It could also involve no CreateFrame IPCs if the
  // navigation is staying in a previous RenderFrame, such as when performing a
  // same-document navigation, or navigating out of an initial blank document
  // where RenderDocument is not used.
  void AddCreateViewEvent(
      const std::optional<base::UnguessableToken>& navigation_metrics_token,
      const base::TimeTicks& start_time,
      const base::TimeDelta& elapsed_time);
  void AddCreateRemoteChildrenEvent(
      const std::optional<base::UnguessableToken>& navigation_metrics_token,
      const base::TimeTicks& start_time,
      const base::TimeDelta& elapsed_time);
  void AddCreateFrameEvent(
      const std::optional<base::UnguessableToken>& navigation_metrics_token,
      const base::TimeTicks& start_time,
      const base::TimeDelta& elapsed_time);

  // Set the time at which the renderer process started processing the
  // CommitNavigation IPC for the navigation identified by
  // `navigation_metrics_token`. This is not guaranteed to happen for all
  // navigations - in particular, renderer-initiated same-document navigations
  // and synchronous about:blank navigations do not involve a commit IPC from
  // the browser process.
  //
  // TODO(crbug.com/415821826): Consider still calling this at the start of
  // commit for those cases and recording a timeline for them as well.
  void MarkCommitStart(const base::UnguessableToken& navigation_metrics_token);

  // This is called when a navigation to `url` and identified by
  // `navigation_metrics_token` has finished committing in the renderer process.
  // This is a signal for this class to generate trace events and metrics using
  // all the timestamps collected so far for it. `navigation_start_time`
  // identifies the time at which this navigation was started, possibly in
  // another process. `commit_sent_time` is the time at which the
  // CommitNavigation IPC was sent by the browser process to this renderer
  // process.
  void ProcessNavigationCommit(
      const base::UnguessableToken& navigation_metrics_token,
      const GURL& url,
      const base::TimeTicks& navigation_start_time,
      const base::TimeTicks& commit_sent_time,
      bool is_main_frame);

 private:
  ~RendererNavigationMetricsManager();

  Timeline& GetOrCreateTimeline(
      const base::UnguessableToken& navigation_metrics_token);

  // Records trace events and metrics for a particular navigation, using
  // timestamps in the provided `timeline`. `url` is used to log a trace event
  // that contains the navigation's final URL for convenience.
  void RecordTraceEventsAndMetrics(
      const RendererNavigationMetricsManager::Timeline& timeline,
      const GURL& url);

  // A map of navigation metrics tokens to corresponding Timeline objects.
  std::map<base::UnguessableToken, Timeline> timelines_;

  // Whether or not a first navigation in this renderer process has started.
  // This is used to report a single zero-sized WaitingForProcessReady per
  // process in any cases that the process was ready before the first
  // navigation, which indicates how often the process is ready to go when it's
  // needed for a navigation.
  bool has_first_navigation_started_ = false;
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_NAVIGATION_METRICS_MANAGER_H_
