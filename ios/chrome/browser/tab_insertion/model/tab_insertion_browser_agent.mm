// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"

#import "build/blink_buildflags.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/url_loading/new_tab_animation_tab_helper.h"
#import "ios/web/common/features.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_certificate_policy_cache_storage.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/web_state.h"

namespace TabInsertion {
Params::Params() = default;
Params::~Params() = default;
}  // namespace TabInsertion

BROWSER_USER_DATA_KEY_IMPL(TabInsertionBrowserAgent)

namespace {

// Creates an unrealized web state to be restored without load.
std::unique_ptr<web::WebState> CreateUnrealizedWebStateForTab(
    const web::NavigationManager::WebLoadParams& web_load_params,
    const std::u16string& title,
    web::WebState::CreateParams create_params,
    base::Time creation_time) {
  // Creates the tab.
  CRWNavigationItemStorage* item_storage =
      [[CRWNavigationItemStorage alloc] init];
  item_storage.URL = web_load_params.url;
  item_storage.virtualURL = web_load_params.virtual_url;
  item_storage.referrer = web_load_params.referrer;
  item_storage.timestamp = creation_time;
  item_storage.title = title;
  item_storage.HTTPRequestHeaders = web_load_params.extra_headers;

  // Creates the session with the tab.
  CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
  session_storage.stableIdentifier = [[NSUUID UUID] UUIDString];
  session_storage.uniqueIdentifier = web::WebStateID::NewUnique();
  session_storage.hasOpener = create_params.created_with_opener;
  session_storage.lastCommittedItemIndex = 0;
  session_storage.creationTime = creation_time;
  session_storage.userAgentType = web::UserAgentType::MOBILE;
  session_storage.certPolicyCacheStorage =
      [[CRWSessionCertificatePolicyCacheStorage alloc] init];
  session_storage.itemStorages = @[ item_storage ];

  return web::WebState::CreateWithStorageSession(create_params,
                                                 session_storage);
}

}  // namespace

TabInsertionBrowserAgent::TabInsertionBrowserAgent(Browser* browser)
    : browser_state_(browser->GetBrowserState()),
      web_state_list_(browser->GetWebStateList()) {}

TabInsertionBrowserAgent::~TabInsertionBrowserAgent() {}

web::WebState* TabInsertionBrowserAgent::InsertWebState(
    const web::NavigationManager::WebLoadParams& web_load_params,
    const TabInsertion::Params& tab_insertion_params) {
  DCHECK(tab_insertion_params.index == TabInsertion::kPositionAutomatically ||
         (tab_insertion_params.index >= 0 &&
          tab_insertion_params.index <= web_state_list_->count()));

  int insertion_index = WebStateList::kInvalidIndex;
  int insertion_flags = WebStateList::INSERT_NO_FLAGS;
  if (tab_insertion_params.index != TabInsertion::kPositionAutomatically) {
    DCHECK_LE(tab_insertion_params.index, INT_MAX);
    insertion_index = static_cast<int>(tab_insertion_params.index);
    insertion_flags |= WebStateList::INSERT_FORCE_INDEX;
  } else if (!ui::PageTransitionCoreTypeIs(web_load_params.transition_type,
                                           ui::PAGE_TRANSITION_LINK)) {
    insertion_index = web_state_list_->count();
    insertion_flags |= WebStateList::INSERT_FORCE_INDEX;
  }

  if (!tab_insertion_params.in_background) {
    insertion_flags |= WebStateList::INSERT_ACTIVATE;
  }

  if (tab_insertion_params.inherit_opener) {
    insertion_flags |= WebStateList::INSERT_INHERIT_OPENER;
  }

  std::unique_ptr<web::WebState> web_state;
  web::WebState::CreateParams create_params(browser_state_);
  create_params.created_with_opener = tab_insertion_params.opened_by_dom;

  // TODO(crbug.com/1383087): Needs an API in SessionRestorationService.
  if (tab_insertion_params.instant_load ||
      web::features::UseSessionSerializationOptimizations()) {
    web_state = web::WebState::Create(create_params);
  } else {
    web_state = CreateUnrealizedWebStateForTab(
        web_load_params, tab_insertion_params.placeholder_title, create_params,
        base::Time::Now());
  }

  if (tab_insertion_params.should_show_start_surface) {
    NewTabPageTabHelper::CreateForWebState(web_state.get());
    NewTabPageTabHelper::FromWebState(web_state.get())
        ->SetShowStartSurface(true);
  }

  if (tab_insertion_params.should_skip_new_tab_animation) {
    NewTabAnimationTabHelper::CreateForWebState(web_state.get());
    NewTabAnimationTabHelper::FromWebState(web_state.get())
        ->DisableNewTabAnimation();
  }

  if (web_state->IsRealized()) {
    web_state->GetNavigationManager()->LoadURLWithParams(web_load_params);
  }

  int inserted_index = web_state_list_->InsertWebState(
      insertion_index, std::move(web_state), insertion_flags,
      WebStateOpener(tab_insertion_params.parent));

  return web_state_list_->GetWebStateAt(inserted_index);
}

web::WebState* TabInsertionBrowserAgent::InsertWebStateOpenedByDOM(
    web::WebState* parent) {
  web::WebState::CreateParams createParams(browser_state_);
  createParams.created_with_opener = YES;
#if BUILDFLAG(USE_BLINK)
  createParams.opener_web_state = parent;
#endif
  std::unique_ptr<web::WebState> web_state =
      web::WebState::Create(createParams);
  int insertion_flags =
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE;
  int inserted_index = web_state_list_->InsertWebState(
      web_state_list_->count(), std::move(web_state), insertion_flags,
      WebStateOpener(parent));

  return web_state_list_->GetWebStateAt(inserted_index);
}
