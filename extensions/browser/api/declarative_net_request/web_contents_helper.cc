// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/web_contents_helper.h"
#include "base/check.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"

namespace extensions::declarative_net_request {

namespace {

RulesetManager* GetRulesetManager(content::BrowserContext* context) {
  DCHECK(context);

  auto* rules_monitor_service =
      declarative_net_request::RulesMonitorService::Get(context);

  // RulesMonitorService can be null in unit tests.
  return rules_monitor_service ? rules_monitor_service->ruleset_manager()
                               : nullptr;
}

}  // namespace

WebContentsHelper::WebContentsHelper(content::WebContents* web_contents)
    : ruleset_manager_(GetRulesetManager(web_contents->GetBrowserContext())) {
  if (ruleset_manager_) {
    Observe(web_contents);
  }
}

WebContentsHelper::~WebContentsHelper() = default;

void WebContentsHelper::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(ruleset_manager_);
  ruleset_manager_->OnRenderFrameCreated(render_frame_host);
}

void WebContentsHelper::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(ruleset_manager_);
  ruleset_manager_->OnRenderFrameDeleted(render_frame_host);
}

void WebContentsHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(ruleset_manager_);
  if (!navigation_handle->HasCommitted()) {
    return;
  }

  ruleset_manager_->OnDidFinishNavigation(navigation_handle);
}

}  // namespace extensions::declarative_net_request
