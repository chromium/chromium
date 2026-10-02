// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/hidden_target_manager.h"

#include "content/browser/devtools/web_contents_devtools_agent_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/child_process_id.h"

namespace content::protocol {

namespace {

// Keeps the hidden target's current primary main frame renderer process marked
// used and running at foreground scheduling priority
// (`base::Process::Priority::kUserBlocking`, preventing OS-level background
// process suppression such as macOS App Nap) while keeping `WebContents`
// hidden so the compositor pipeline does not run unless screenshotting.
class HiddenTargetWebContentsObserver
    : public WebContentsObserver,
      public WebContentsUserData<HiddenTargetWebContentsObserver> {
 public:
  ~HiddenTargetWebContentsObserver() override { SetTrackedProcess(nullptr); }

  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override {
    if (new_host->IsInPrimaryMainFrame()) {
      SetTrackedProcess(new_host->GetProcess());
    }
  }

 private:
  friend class WebContentsUserData<HiddenTargetWebContentsObserver>;

  explicit HiddenTargetWebContentsObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        WebContentsUserData<HiddenTargetWebContentsObserver>(*web_contents) {
    SetTrackedProcess(web_contents->GetPrimaryMainFrame()->GetProcess());
  }

  void SetTrackedProcess(RenderProcessHost* new_process) {
    ChildProcessId new_id =
        new_process ? new_process->GetID() : ChildProcessId();
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
      // Chrome's `PerformanceManager` aggressively throttles processes
      // with only hidden frames to `kBestEffort` (putting them into OS-level
      // background states, e.g. macOS App Nap). However, hidden targets need
      // to execute quickly without being throttled. Applying this boost bypasses
      // PerformanceManager's override and forces the process back to
      // `kUserBlocking` without marking the `WebContents` visible or running
      // the compositor pipeline.
      static_cast<RenderProcessHostImpl*>(new_process)
          ->OnBoostForBackgroundExecutionAdded();
    }
  }

  ChildProcessId tracked_process_id_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(HiddenTargetWebContentsObserver);

}  // namespace

HiddenTargetManager::HiddenTargetManager() = default;

HiddenTargetManager::~HiddenTargetManager() = default;

void HiddenTargetManager::CloseContents(content::WebContents* source) {
  // Erasing the web contents from the map deletes its unique_ptr, which is
  // what actually cleans up and destroys the hidden tab.
  hidden_web_contents_.erase(source);
}

std::string HiddenTargetManager::CreateHiddenTarget(
    const GURL& url,
    BrowserContext* browser_context) {
  WebContents::CreateParams create_params(browser_context);
  // Keep the WebContents hidden (`Visibility::HIDDEN`) so the compositor
  // pipeline does not run unless explicitly captured (e.g. for screenshots).
  create_params.initially_hidden = true;
  std::unique_ptr<WebContents> web_contents =
      WebContents::Create(create_params);
  // Required for the hidden WebContents to be properly disposed.
  web_contents->SetDelegate(this);

  // Start the initial navigation before calling `SetIsUsed()` below so
  // `IsSuitableHost()` allows the navigation to reuse the initial
  // `RenderProcessHost` instead of spawning a second process when `url`
  // requires a dedicated process (e.g. `data:` URLs).
  NavigationController::LoadURLParams load_params(url);
  web_contents->GetController().LoadURLWithParams(load_params);

  // The hidden target hosts a `window.cdp` binding wired to a trusted
  // browser-level DevTools session (BrowserToPageConnector). Its siteless
  // about:blank navigation never reaches SetIsUsed(), so the process would
  // otherwise be treated as a freely-reusable allows-any-site host and
  // unrelated web content could be co-scheduled with it at the process limit.
  // Mark the process used so IsSuitableHost() rejects it for sites that
  // require a dedicated process, and keep its scheduling priority at
  // foreground across any main-frame RenderProcessHost transitions.
  web_contents->GetPrimaryMainFrame()->GetProcess()->SetIsUsed();
  HiddenTargetWebContentsObserver::CreateForWebContents(web_contents.get());

  std::string target_id =
      content::DevToolsAgentHost::GetOrCreateFor(web_contents.get())->GetId();
  CHECK(!web_contents->GetPrimaryMainFrame()->GetProcess()->IsUnused())
      << "Hidden target process is unexpectedly unused";
  hidden_web_contents_.insert(std::move(web_contents));
  return target_id;
}

void HiddenTargetManager::Clear() {
  hidden_web_contents_.clear();
}

}  // namespace content::protocol
