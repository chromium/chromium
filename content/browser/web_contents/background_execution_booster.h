// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_BACKGROUND_EXECUTION_BOOSTER_H_
#define CONTENT_BROWSER_WEB_CONTENTS_BACKGROUND_EXECUTION_BOOSTER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/child_process_id.h"

namespace content {

class RenderFrameHost;
class RenderProcessHost;
class WebContents;

// Keeps the primary main frame renderer process of a hidden `WebContents`
// running at foreground scheduling priority
// (`base::Process::Priority::kUserBlocking`, preventing OS-level background
// process suppression such as macOS App Nap) while keeping the `WebContents`
// hidden so the compositor pipeline does not run unless captured (e.g. for
// screenshots).
class BackgroundExecutionBooster
    : public WebContentsUserData<BackgroundExecutionBooster>,
      private WebContentsObserver {
 public:
  BackgroundExecutionBooster(const BackgroundExecutionBooster&) = delete;
  BackgroundExecutionBooster& operator=(const BackgroundExecutionBooster&) =
      delete;
  ~BackgroundExecutionBooster() override;

 private:
  friend class WebContentsUserData<BackgroundExecutionBooster>;

  explicit BackgroundExecutionBooster(WebContents* web_contents);

  // WebContentsObserver:
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;

  void SetTrackedProcess(RenderProcessHost* new_process);

  ChildProcessId tracked_process_id_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_BACKGROUND_EXECUTION_BOOSTER_H_
