// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"

#import "build/blink_buildflags.h"
#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/url_loading/model/new_tab_animation_tab_helper.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/navigation/navigation_util.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "ios/web/public/web_state.h"

namespace TabInsertion {
Params::Params() = default;
Params::~Params() = default;
}  // namespace TabInsertion

BROWSER_USER_DATA_KEY_IMPL(TabInsertionBrowserAgent)

namespace {

// Returns whether `index` is valid for insertion in `browser`.
bool IsIndexValidForBrowser(Browser* browser, int index) {
  if (index == TabInsertion::kPositionAutomatically) {
    return true;
  }

  return index >= 0 && index <= browser->GetWebStateList()->count();
}

// Returns whether the tab can be created in an unrealized state or
// not according to `web_load_params` and `tab_insertion_params`.
bool MustCreateRealizedWebState(
    const web::NavigationManager::WebLoadParams& web_load_params,
    const TabInsertion::Params& tab_insertion_params) {
  return tab_insertion_params.instant_load || web_load_params.post_data != nil;
}

}  // namespace

TabInsertionBrowserAgent::TabInsertionBrowserAgent(Browser* browser)
    : browser_(browser) {
  DCHECK(browser_);
}

TabInsertionBrowserAgent::~TabInsertionBrowserAgent() = default;

web::WebState* TabInsertionBrowserAgent::InsertWebState(
    const web::NavigationManager::WebLoadParams& web_load_params,
    const TabInsertion::Params& tab_insertion_params) {
  DCHECK(IsIndexValidForBrowser(browser_.get(), tab_insertion_params.index));

  WebStateList* const web_state_list = browser_->GetWebStateList();
  ProfileIOS* const profile = browser_->GetProfile();

  std::unique_ptr<web::WebState> web_state;
  web::WebState::CreateParams create_params(profile);
  create_params.created_with_opener = tab_insertion_params.opened_by_dom;

  // Check whether the tab must be created as realized or not.
  if (MustCreateRealizedWebState(web_load_params, tab_insertion_params)) {
    web_state = web::WebState::Create(create_params);
  } else {
    web::proto::WebStateStorage storage = web::CreateWebStateStorage(
        web_load_params, tab_insertion_params.placeholder_title,
        tab_insertion_params.opened_by_dom, web::UserAgentType::MOBILE,
        base::Time::Now());

    // Ask the SessionRestorationService to create an unrealized WebState
    // that can be inserted into the WebStateList of `browser_`.
    web_state =
        SessionRestorationServiceFactory::GetForProfile(profile)
            ->CreateUnrealizedWebState(browser_.get(), std::move(storage));
  }
  DCHECK(web_state);

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

  WebStateList::InsertionParams params =
      WebStateList::InsertionParams::Automatic();
  if (tab_insertion_params.index != TabInsertion::kPositionAutomatically) {
    params = WebStateList::InsertionParams::AtIndex(tab_insertion_params.index);
  } else if (!ui::PageTransitionCoreTypeIs(web_load_params.transition_type,
                                           ui::PAGE_TRANSITION_LINK)) {
    params = WebStateList::InsertionParams::AtIndex(web_state_list->count());
  }

  bool should_activate =
      !tab_insertion_params.in_background || web_state_list->empty();

  params.Activate(should_activate)
      .InheritOpener(tab_insertion_params.inherit_opener)
      .WithOpener(WebStateOpener(tab_insertion_params.parent));
  if (tab_insertion_params.insert_pinned) {
    params.Pinned();
  }
  if (tab_insertion_params.insert_in_group && tab_insertion_params.tab_group) {
    params.InGroup(tab_insertion_params.tab_group.get());
  }
  web::WebState* web_state_ptr = web_state.get();
  web_state_list->InsertWebState(std::move(web_state), params);
  if (tab_insertion_params.insert_in_group && !tab_insertion_params.tab_group) {
    web_state_list->CreateGroup(
        {web_state_list->GetIndexOfWebState(web_state_ptr)},
        tab_groups::TabGroupVisualData{
            u"", TabGroup::DefaultColorForNewTabGroup(web_state_list)},
        tab_groups::TabGroupId::GenerateNew());
  }
  return web_state_ptr;
}

web::WebState* TabInsertionBrowserAgent::InsertWebStateOpenedByDOM(
    web::WebState* parent) {
  web::WebState::CreateParams create_params(browser_->GetProfile());
  create_params.created_with_opener = YES;
#if BUILDFLAG(USE_BLINK)
  create_params.opener_web_state = parent;
#endif
  std::unique_ptr<web::WebState> web_state =
      web::WebState::Create(create_params);

  web::WebState* web_state_ptr = web_state.get();
  WebStateList* web_state_list = browser_->GetWebStateList();
  web_state_list->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(web_state_list->count())
          .Activate()
          .WithOpener(WebStateOpener(parent)));
  return web_state_ptr;
}
