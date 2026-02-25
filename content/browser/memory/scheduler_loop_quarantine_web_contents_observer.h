// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_SCHEDULER_LOOP_QUARANTINE_WEB_CONTENTS_OBSERVER_H_
#define CONTENT_BROWSER_MEMORY_SCHEDULER_LOOP_QUARANTINE_WEB_CONTENTS_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
class NavigationHandle;

// An observer that gets notified on ReadyToCommitNavigation. We use this signal
// to setup the scheduler loop quarantine.
//
// The SchedulerLoopQuarantine is a feature (called MiracleObject or
// PartitionAlloc with Advanced Checks PA/AC), the goal of this feature it to
// prevent UaF from being exploitable. It does this by overriding free/delete
// calls and instead zapping the memory and quarantining it until the task ends.
// However UaFs before a renderer (with proper website content in a WebContents)
// exist are not exploitable by renderers and thus lower priority. To avoid
// impact on start up metrics we use this observer to delay initializing until
// after the first WebContents is ready to commit a navigation.
//
// This class attaches to each WebContent and for the first none "about:blank"
// web contents navigation it will (if the feature is enabled) configure the
// SchedulerLoopQuarantine, and then block future observers from configuring it
// or being attached to webcontents from then on. The static method allows
// inspecting of that state.
//
// Important: It must be called from the browser UI thread, and receive messages
// like all WebContentsObservers do on that thread.
class CONTENT_EXPORT SchedulerLoopQuarantineWebContentsObserver
    : public WebContentsObserver,
      public WebContentsUserData<SchedulerLoopQuarantineWebContentsObserver> {
 public:
  explicit SchedulerLoopQuarantineWebContentsObserver(
      WebContents* web_contents);
  ~SchedulerLoopQuarantineWebContentsObserver() override;

  SchedulerLoopQuarantineWebContentsObserver(
      const SchedulerLoopQuarantineWebContentsObserver&) = delete;
  SchedulerLoopQuarantineWebContentsObserver& operator=(
      const SchedulerLoopQuarantineWebContentsObserver&) = delete;

  // WebContentsObserver methods:
  // Note on the navigation lifecycle hooks used in this class:
  //
  // This observer behaves asymmetrically depending on whether it is the *first*
  // navigation in the browser or a *subsequent* navigation:
  //
  // 1. First Navigation (Startup): We delay enabling the PartitionAlloc memory
  //    quarantine until the browser has finished its initial startup and is
  //    actually committing the first webpage, to avoid regressing startup
  //    metrics. Because of this, `DidStartNavigation` exits early, and
  //    `ReadyToCommitNavigation` does the one-time global setup and then
  //    immediately pauses the quarantine for the remainder of that navigation.
  //
  // 2. Subsequent Navigations: Once the quarantine is configured and running
  //    globally (`g_reconfiguration_done` == true), we want to pause it as
  //    early as possible for all new navigations to prevent performance
  //    regressions during the network phase and tab switching. Thus,
  //    `DidStartNavigation` creates the exclusion immediately, and
  //    `ReadyToCommitNavigation` does nothing (exits early).
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;

  static void MaybeCreateForWebContents(WebContents* web_contents);

  // Check if any SchedulerLoopQuarantineWebContentsObserver has already
  // configured the SchedulerLoopQuarantine.
  // Public for testing only.
  static bool AlreadyTriggeredReconfiguration();
  // Resets the reconfiguration state for testing purposes.
  static void ResetForTesting();

 private:
  friend class WebContentsUserData<SchedulerLoopQuarantineWebContentsObserver>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};
}  // namespace content
#endif  // CONTENT_BROWSER_MEMORY_SCHEDULER_LOOP_QUARANTINE_WEB_CONTENTS_OBSERVER_H_
