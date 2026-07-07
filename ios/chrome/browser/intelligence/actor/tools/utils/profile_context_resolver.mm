// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/utils/profile_context_resolver.h"

#import "ios/chrome/browser/intelligence/actor/tools/utils/actor_browser_utils.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"

namespace actor {

ProfileContextResolver::ProfileContextResolver(ProfileIOS* profile)
    : profile_(profile) {
  CHECK(profile_);
}

ProfileContextResolver::~ProfileContextResolver() = default;

ProfileContextResolver::TabResolutionResult::TabResolutionResult() = default;

ProfileContextResolver::TabResolutionResult::TabResolutionResult(
    const TabResolutionResult&) = default;

ProfileContextResolver::TabResolutionResult::TabResolutionResult(
    TabResolutionResult&&) = default;

ProfileContextResolver::TabResolutionResult&
ProfileContextResolver::TabResolutionResult::operator=(
    const TabResolutionResult&) = default;

ProfileContextResolver::TabResolutionResult&
ProfileContextResolver::TabResolutionResult::operator=(TabResolutionResult&&) =
    default;

ProfileContextResolver::TabResolutionResult::~TabResolutionResult() = default;

base::expected<ProfileContextResolver::TabResolutionResult, ToolExecutionResult>
ProfileContextResolver::ResolveTab(int32_t tab_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/520098751): determine if we should support incognito here.
  BrowserAndIndex browser_and_index = FindBrowserAndIndexFromProfile(
      profile_, web::WebStateID::FromSerializedValue(tab_id),
      /*include_incognito=*/false);

  if (browser_and_index.tab_index == WebStateList::kInvalidIndex ||
      !browser_and_index.browser) {
    return base::unexpected(
        ToolExecutionResult(mojom::ActionResultCode::kTabWentAway));
  }

  WebStateList* web_state_list = browser_and_index.browser->GetWebStateList();
  if (!web_state_list) {
    return base::unexpected(
        ToolExecutionResult(mojom::ActionResultCode::kTabWentAway));
  }

  web::WebState* web_state =
      web_state_list->GetWebStateAt(browser_and_index.tab_index);
  if (!web_state) {
    return base::unexpected(
        ToolExecutionResult(mojom::ActionResultCode::kTabWentAway));
  }

  TabResolutionResult result;
  UrlLoadingBrowserAgent* url_loader =
      UrlLoadingBrowserAgent::FromBrowser(browser_and_index.browser);
  if (url_loader) {
    result.url_loader = url_loader->AsWeakPtr();
  }
  result.tab_index = browser_and_index.tab_index;
  result.web_state = web_state->GetWeakPtr();
  result.web_state_list = web_state_list->AsWeakPtr();
  return result;
}

}  // namespace actor
