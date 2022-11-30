// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/tab_usage_recorder_metrics.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace tab_usage_recorder {

// The histogram recording the state of the tab the user switches to.
const char kSelectedTabHistogramName[] =
    "Tab.StatusWhenSwitchedBackToForeground";

// The histogram to record the number of page loads before an evicted tab is
// selected.
const char kPageLoadsBeforeEvictedTabSelected[] =
    "Tab.PageLoadsSinceLastSwitchToEvictedTab";

// Records the time it takes for an evicted tab to reload.
const char kEvictedTabReloadTime[] = "Tab.RestoreTime";

// Records success vs failure of an evicted tab's reload.
const char kEvictedTabReloadSuccessRate[] = "Tab.RestoreResult";

// Records whether or not the user switched tabs before an evicted tab finished
// reloading.
const char kDidUserWaitForEvictedTabReload[] = "Tab.RestoreUserPersistence";

// The name of the histogram that records time intervals between tab restores.
const char kTimeBetweenRestores[] = "Tabs.TimeBetweenRestores";

// The name of the histogram that records time intervals since the last restore.
const char kTimeAfterLastRestore[] = "Tabs.TimeAfterLastRestore";

// Name of histogram to record whether a memory warning had been recently
// received when a renderer termination occurred.
const char kRendererTerminationSawMemoryWarning[] =
    "Tab.RendererTermination.RecentlyReceivedMemoryWarning";

// Name of histogram to record the number of alive renderers when a renderer
// termination is received.
const char kRendererTerminationAliveRenderers[] =
    "Tab.RendererTermination.AliveRenderersCount";

const char kRendererTerminationTotalTabCount[] =
    "Tab.RendererTermination.TotalTabCount";

// Name of histogram to record the number of renderers that were alive shortly
// before a renderer termination. This metric is being recorded in case the OS
// kills renderers in batches.
const char kRendererTerminationRecentlyAliveRenderers[] =
    "Tab.RendererTermination.RecentlyAliveRenderersCount";

// Name of histogram for recording the state of the tab when the renderer is
// terminated.
const char kRendererTerminationStateHistogram[] =
    "Tab.StateAtRendererTermination";

// The recently alive renderer count metric counts all renderers that were alive
// x seconds before a renderer termination. `kSecondsBeforeRendererTermination`
// specifies x.
const int kSecondsBeforeRendererTermination = 2;

}  // namespace tab_usage_recorder
