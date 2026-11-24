// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/background_execution_booster.h"

#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace content {

BackgroundExecutionBooster::BackgroundExecutionBooster(
    WebContents* web_contents)
    : WebContentsUserData<BackgroundExecutionBooster>(*web_contents),
      WebContentsObserver(web_contents) {
  SetTrackedProcess(web_contents->GetPrimaryMainFrame()->GetProcess());
}

BackgroundExecutionBooster::~BackgroundExecutionBooster() {
  SetTrackedProcess(nullptr);
}

void BackgroundExecutionBooster::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  if (new_host->IsInPrimaryMainFrame()) {
    SetTrackedProcess(new_host->GetProcess());
  }
}

void BackgroundExecutionBooster::SetTrackedProcess(
    RenderProcessHost* new_process) {
  ChildProcessId new_id = new_process ? new_process->GetID() : ChildProcessId();
  if (tracked_process_id_ == new_id) {
    return;
  }
  if (RenderProcessHost* old_process =
          RenderProcessHost::FromID(tracked_process_id_)) {
    static_cast<RenderProcessHostImpl*>(old_process)
        ->OnBoostForBackgroundExecutionRemoved();
  }
  tracked_process_id_ = new_id;
  if (new_process) {
    // Chrome's `PerformanceManager` aggressively throttles processes with only
    // hidden frames to `kBestEffort` (putting them into OS-level background
    // states, e.g. macOS App Nap). Hidden targets and BiDi mappers need to
    // execute quickly without being throttled. Applying this boost bypasses
    // PerformanceManager's override and forces the process back to
    // `kUserBlocking` without marking the `WebContents` visible or running the
    // compositor pipeline.
    static_cast<RenderProcessHostImpl*>(new_process)
        ->OnBoostForBackgroundExecutionAdded();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(BackgroundExecutionBooster);

}  // namespace content
