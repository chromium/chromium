// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/hidden_target_manager.h"

#include "content/browser/devtools/web_contents_devtools_agent_host.h"

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

  NavigationController::LoadURLParams load_params(url);
  web_contents->GetController().LoadURLWithParams(load_params);

  std::string target_id =
      content::DevToolsAgentHost::GetOrCreateFor(web_contents.get())->GetId();
  hidden_web_contents_.insert(std::move(web_contents));
  return target_id;
}

void HiddenTargetManager::Clear() {
  hidden_web_contents_.clear();
}

}  // namespace content::protocol
