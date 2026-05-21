// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/hidden_target_manager.h"

#include "content/browser/devtools/web_contents_devtools_agent_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace content::protocol {

HiddenTargetManager::HiddenTargetManager() = default;

HiddenTargetManager::~HiddenTargetManager() = default;

void HiddenTargetManager::CloseContents(content::WebContents* source) {
  hidden_web_contents_.erase(source);
}

std::string HiddenTargetManager::CreateHiddenTarget(
    const GURL& url,
    BrowserContext* browser_context) {
  WebContents::CreateParams create_params(browser_context);
  std::unique_ptr<WebContents> web_contents =
      WebContents::Create(create_params);
  // Required for the hidden WebContents to be properly disposed.
  web_contents->SetDelegate(this);
  // The hidden target hosts a `window.cdp` binding wired to a trusted
  // browser-level DevTools session (BrowserToPageConnector). Its siteless
  // about:blank navigation never reaches SetIsUsed(), so the process would
  // otherwise be treated as a freely-reusable allows-any-site host and
  // unrelated web content could be co-scheduled with it at the process limit.
  // Mark the process used so IsSuitableHost() rejects it for sites that
  // require a dedicated process.
  web_contents->GetPrimaryMainFrame()->GetProcess()->SetIsUsed();

  NavigationController::LoadURLParams load_params(url);
  web_contents->GetController().LoadURLWithParams(load_params);

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
