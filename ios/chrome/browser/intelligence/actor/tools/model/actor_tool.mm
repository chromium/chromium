// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"

#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/actor_browser_utils.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/actor_tool_utils.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"

namespace actor {

ActorTool::TabResolutionResult::TabResolutionResult() = default;

ActorTool::TabResolutionResult::TabResolutionResult(
    const TabResolutionResult&) = default;

ActorTool::TabResolutionResult& ActorTool::TabResolutionResult::operator=(
    const TabResolutionResult&) = default;

ActorTool::TabResolutionResult::~TabResolutionResult() = default;

base::WeakPtr<web::WebFrame> ActorTool::GetTargetWebFrame() const {
  return nullptr;
}

// static
base::expected<ActorTool::TabResolutionResult, ToolExecutionResult>
ActorTool::ResolveTab(int32_t tab_id, ProfileIOS* profile) {
  // TODO(crbug.com/520098751) - determine if we should support incognito here.
  BrowserAndIndex browser_and_index = FindBrowserAndIndexFromProfile(
      profile, web::WebStateID::FromSerializedValue(tab_id),
      /*include_incognito=*/false);

  if (browser_and_index.tab_index == WebStateList::kInvalidIndex ||
      !browser_and_index.browser) {
    return base::unexpected(
        ToolExecutionResult(InternalToolErrorCode::kCreationTargetTabNotFound));
  }

  web::WebState* web_state =
      browser_and_index.browser->GetWebStateList()->GetWebStateAt(
          browser_and_index.tab_index);
  if (!web_state) {
    return base::unexpected(
        ToolExecutionResult(InternalToolErrorCode::kCreationMissingWebState));
  }

  TabResolutionResult result;
  UrlLoadingBrowserAgent* url_loader =
      UrlLoadingBrowserAgent::FromBrowser(browser_and_index.browser);
  if (url_loader) {
    result.url_loader = url_loader->AsWeakPtr();
  }
  result.tab_index = browser_and_index.tab_index;
  result.web_state = web_state->GetWeakPtr();
  return result;
}

}  // namespace actor
