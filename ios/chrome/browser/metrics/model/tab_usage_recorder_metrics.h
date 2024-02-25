// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_TAB_USAGE_RECORDER_METRICS_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_TAB_USAGE_RECORDER_METRICS_H_

namespace tab_usage_recorder {
// Histogram names (visible for testing only).

// The name of the histogram that records the state of the selected tab
// (i.e. the tab being switched to).
extern const char kSelectedTabHistogramName[];

// The name of the histogram that records the number of page loads before an
// evicted tab is selected.
extern const char kPageLoadsBeforeEvictedTabSelected[];

// The name of the histogram that records time intervals between restores of
// previously-evicted tabs.  The first restore seen in a session will record the
// time since the session started.
extern const char kTimeBetweenRestores[];

// The name of the histogram that records time intervals between the last
// restore of a previously-evicted tab and the end of the session.
extern const char kTimeAfterLastRestore[];

// Name of histogram to record the number of alive renderers when a renderer
// termination is received.
extern const char kRendererTerminationAliveRenderers[];

// Name of the histogram to record the total number of tabs when a renderer
// termination is received.
extern const char kRendererTerminationTotalTabCount[];

// Name of histogram to record the number of renderers that were alive shortly
// before a renderer termination. This metric is being recorded in case the OS
// kills renderers in batches.
extern const char kRendererTerminationRecentlyAliveRenderers[];

// Name of histogram for recording the state of the tab when the renderer is
// terminated.
extern const char kRendererTerminationStateHistogram[];

// The recently alive renderer count metric counts all renderers that were alive
// x seconds before a renderer termination. `kSecondsBeforeRendererTermination`
// specifies x.
extern const int kSecondsBeforeRendererTermination;

// enum for
// kSelectedTabHistogramName[] = "Tab.StatusWhenSwitchedBackToForeground"
// histogram.
enum TabStateWhenSelected {
  IN_MEMORY = 0,                  // Memory resident
  EVICTED = 1,                    // Evicted and reloaded
  EVICTED_DUE_TO_COLD_START = 2,  // Reloaded due to cold start
  PARTIALLY_EVICTED =
      3,  // Partially evicted (Currently, used only by Android.)
  EVICTED_DUE_TO_BACKGROUNDING =
      4,                         // Reloaded due to backgrounding (Deprecated)
  EVICTED_DUE_TO_INCOGNITO = 5,  // Reloaded due to incognito
  RELOADED_DUE_TO_COLD_START_FG_TAB_ON_START =
      6,  // Reloaded due to cold start (fg tab on start) (Android)
  RELOADED_DUE_TO_COLD_START_BG_TAB_ON_SWITCH =
      7,  // Reloaded due to cold start (bg tab on switch) (Android)
  LAZY_LOAD_FOR_OPEN_IN_NEW_TAB =
      8,  // Lazy load for 'Open in new tab' (Android)
  STOPPED_DUE_TO_LOADING_WHEN_BACKGROUNDING =
      9,  // Stopped due to page loading when backgrounding (Deprecated)
  EVICTED_DUE_TO_LOADING_WHEN_BACKGROUNDING =
      10,  // Evicted due to page loading when backgrounding (Deprecated)
  EVICTED_DUE_TO_RENDERER_TERMINATION =
      11,  // Evicted due to OS terminating the renderer
  TAB_STATE_COUNT = 12,
};

enum LoadDoneState {
  LOAD_FAILURE,
  LOAD_SUCCESS,
  LOAD_DONE_STATE_COUNT,
};

enum EvictedTabUserBehavior {
  USER_WAITED,
  USER_DID_NOT_WAIT,
  USER_LEFT_CHROME,
  USER_BEHAVIOR_COUNT,
};

// Enum corresponding to UMA's TabForegroundState, for
// Tab.StateAtRendererTermination. Must be kept in sync with the UMA enum.
enum RendererTerminationTabState {
  // These two values are for when the app is in the foreground.
  FOREGROUND_TAB_FOREGROUND_APP = 0,
  BACKGROUND_TAB_FOREGROUND_APP,
  // These are for when the app is in the background or inactive.
  FOREGROUND_TAB_BACKGROUND_APP,
  BACKGROUND_TAB_BACKGROUND_APP,
  TERMINATION_TAB_STATE_COUNT
};

}  // namespace tab_usage_recorder

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_TAB_USAGE_RECORDER_METRICS_H_
