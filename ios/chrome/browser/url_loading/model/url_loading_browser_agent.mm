// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"

#import "base/compiler_specific.h"
#import "base/debug/dump_without_crashing.h"
#import "base/immediate_crash.h"
#import "base/strings/string_number_conversions.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/browser/crash_report/model/crash_reporter_url_observer.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/prerender/model/prerender_service.h"
#import "ios/chrome/browser/prerender/model/prerender_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/scene_url_loading_service.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/url_loading/model/url_loading_util.h"
#import "ios/chrome/browser/web/model/load_timing_tab_helper.h"
#import "net/base/url_util.h"

BROWSER_USER_DATA_KEY_IMPL(UrlLoadingBrowserAgent)

namespace {

// Rapidly starts leaking memory by 10MB blocks.
void StartLeakingMemory() {
  static NSMutableArray* memory = nil;
  if (!memory) {
    memory = [[NSMutableArray alloc] init];
  }

  // Store block of memory into NSArray to ensure that compiler does not throw
  // away unused code.
  NSUInteger leak_size = 10 * 1024 * 1024;
  int* leak = new int[leak_size];
  [memory addObject:[NSData dataWithBytes:leak length:leak_size]];

  base::ThreadPool::PostTask(FROM_HERE, base::BindOnce(&StartLeakingMemory));
}

// Helper method for inducing intentional freezes, leaks and crashes, in a
// separate function so it will show up in stack traces. If a delay parameter is
// present, the main thread will be frozen for that number of seconds. If a
// crash parameter is "true" (which is the default value), the browser will
// crash after this delay. If a crash parameter is "later", the browser will
// crash in another thread (nsexception only).  Any other value will not
// trigger a crash.
NOINLINE void InduceBrowserCrash(const GURL& url) {
  std::string delay_string;
  if (net::GetValueForKeyInQuery(url, "delay", &delay_string)) {
    int delay = 0;
    if (base::StringToInt(delay_string, &delay) && delay > 0) {
      sleep(delay);
    }
  }

  std::string dump_without_crashing;
  if (net::GetValueForKeyInQuery(url, "dwc", &dump_without_crashing) &&
      (dump_without_crashing == "" || dump_without_crashing == "true")) {
    base::debug::DumpWithoutCrashing();
    return;
  }

#if !TARGET_IPHONE_SIMULATOR  // Leaking memory does not cause UTE on simulator.
  std::string leak_string;
  if (net::GetValueForKeyInQuery(url, "leak", &leak_string) &&
      (leak_string == "" || leak_string == "true")) {
    StartLeakingMemory();
    return;
  }
#endif

  std::string exception;
  if (net::GetValueForKeyInQuery(url, "nsexception", &exception) &&
      (exception == "" || exception == "true")) {
    NSArray* empty_array = @[];
    [empty_array objectAtIndex:42];
    return;
  }

  if (net::GetValueForKeyInQuery(url, "nsexception", &exception) &&
      exception == "later") {
    dispatch_async(
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
          NSArray* empty_array = @[];
          [empty_array objectAtIndex:42];
        });
    return;
  }

  std::string crash_string;
  if (!net::GetValueForKeyInQuery(url, "crash", &crash_string) ||
      (crash_string == "" || crash_string == "true")) {
    // Induce an intentional crash in the browser process.
    base::ImmediateCrash();
  }
}
}  // namespace

UrlLoadingBrowserAgent::UrlLoadingBrowserAgent(Browser* browser)
    : browser_(browser),
      notifier_(UrlLoadingNotifierBrowserAgent::FromBrowser(browser_)) {
  DCHECK(notifier_);
}

UrlLoadingBrowserAgent::~UrlLoadingBrowserAgent() {}

void UrlLoadingBrowserAgent::SetSceneService(
    SceneUrlLoadingService* scene_service) {
  scene_service_ = scene_service;
}

void UrlLoadingBrowserAgent::SetDelegate(id<URLLoadingDelegate> delegate) {
  delegate_ = delegate;
}

void UrlLoadingBrowserAgent::SetIncognitoLoader(
    UrlLoadingBrowserAgent* loader) {
  incognito_loader_ = loader;
}

void UrlLoadingBrowserAgent::Load(const UrlLoadParams& params) {
  // Apply any override load strategy and dispatch.
  switch (params.load_strategy) {
    case UrlLoadStrategy::ALWAYS_NEW_FOREGROUND_TAB: {
      UrlLoadParams fixed_params = params;
      fixed_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
      Dispatch(fixed_params);
      break;
    }
    case UrlLoadStrategy::NORMAL: {
      Dispatch(params);
      break;
    }
  }
}

void UrlLoadingBrowserAgent::Dispatch(const UrlLoadParams& params) {
  // Then dispatch.
  switch (params.disposition) {
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
      LoadUrlInNewTab(params);
      break;
    case WindowOpenDisposition::CURRENT_TAB:
      LoadUrlInCurrentTab(params);
      break;
    case WindowOpenDisposition::SWITCH_TO_TAB:
      SwitchToTab(params);
      break;
    default:
      DCHECK(false) << "Unhandled url loading disposition.";
      break;
  }
}

void UrlLoadingBrowserAgent::LoadUrlInCurrentTab(const UrlLoadParams& params) {
  web::NavigationManager::WebLoadParams web_params = params.web_params;

  ProfileIOS* profile = browser_->GetProfile();

  notifier_->TabWillLoadUrl(web_params.url, web_params.transition_type);

  WebStateList* web_state_list = browser_->GetWebStateList();
  web::WebState* current_web_state = web_state_list->GetActiveWebState();

  // NOTE: This check for the Crash Host URL is here to avoid the URL from
  // ending up in the history causing the app to crash at every subsequent
  // restart.
  if (web_params.url.host() == kChromeUIBrowserCrashHost) {
    CrashReporterURLObserver::GetSharedInstance()->RecordURL(
        web_params.url, current_web_state, /*pending=*/true);
    InduceBrowserCrash(web_params.url);
    // Under a debugger, the app can continue working even after the CHECK.
    // Adding a return avoids adding the crash url to history.
    notifier_->TabFailedToLoadUrl(web_params.url, web_params.transition_type);
    return;
  }

  PrerenderService* prerender_service =
      PrerenderServiceFactory::GetForProfile(profile);

  // Some URLs are not allowed while in incognito.  If we are in incognito and
  // load a disallowed URL, instead create a new tab not in the incognito state.
  // Also if there's no current web state, that means there is no current tab
  // to open in, so this also redirects to a new tab.
  if (!current_web_state ||
      (profile->IsOffTheRecord() && !IsURLAllowedInIncognito(web_params.url))) {
    if (prerender_service) {
      prerender_service->CancelPrerender();
    }
    notifier_->TabFailedToLoadUrl(web_params.url, web_params.transition_type);

    if (!current_web_state) {
      UrlLoadParams fixed_params = params;
      fixed_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
      fixed_params.in_incognito = profile->IsOffTheRecord();
      Load(fixed_params);
    } else {
      UrlLoadParams fixed_params = UrlLoadParams::InNewTab(web_params);
      fixed_params.in_incognito = NO;
      fixed_params.append_to = OpenPosition::kCurrentTab;
      Load(fixed_params);
    }
    return;
  }

  // Ask the prerender service to load this URL if it can, and return if it does
  // so.
  if (prerender_service &&
      prerender_service->MaybeLoadPrerenderedURL(
          web_params.url, web_params.transition_type, browser_)) {
    notifier_->TabDidPrerenderUrl(web_params.url, web_params.transition_type);
    return;
  }

  const bool typed_or_generated_transition =
      PageTransitionCoreTypeIs(web_params.transition_type,
                               ui::PAGE_TRANSITION_TYPED) ||
      PageTransitionCoreTypeIs(web_params.transition_type,
                               ui::PAGE_TRANSITION_GENERATED);
  if (typed_or_generated_transition) {
    LoadTimingTabHelper::FromWebState(current_web_state)->DidInitiatePageLoad();
  }

  // If this is a reload initiated from the omnibox.
  // TODO(crbug.com/41323528): Add DCHECK to verify that whenever urlToLoad is
  // the same as the old url, the transition type is ui::PAGE_TRANSITION_RELOAD.
  if (PageTransitionCoreTypeIs(web_params.transition_type,
                               ui::PAGE_TRANSITION_RELOAD)) {
    current_web_state->GetNavigationManager()->Reload(
        web::ReloadType::NORMAL, true /* check_for_repost */);
    notifier_->TabDidReloadUrl(web_params.url, web_params.transition_type);
    return;
  }

  current_web_state->GetNavigationManager()->LoadURLWithParams(web_params);

  notifier_->TabDidLoadUrl(web_params.url, web_params.transition_type);
}

void UrlLoadingBrowserAgent::SwitchToTab(const UrlLoadParams& params) {
  DCHECK(scene_service_);

  web::NavigationManager::WebLoadParams web_params = params.web_params;

  WebStateList* web_state_list = browser_->GetWebStateList();
  NSInteger new_web_state_index =
      web_state_list->GetIndexOfInactiveWebStateWithURL(web_params.url);
  bool old_tab_is_ntp_without_history =
      IsNTPWithoutHistory(web_state_list->GetActiveWebState());

  if (new_web_state_index == WebStateList::kInvalidIndex) {
    // If the tab containing the URL has been closed.
    if (old_tab_is_ntp_without_history) {
      // It is NTP, just load the URL.
      Load(UrlLoadParams::InCurrentTab(web_params));
    } else {
      // Load the URL in foreground.
      ProfileIOS* profile = browser_->GetProfile();
      UrlLoadParams new_tab_params =
          UrlLoadParams::InNewTab(web_params.url, web_params.virtual_url);
      new_tab_params.web_params.referrer = web::Referrer();
      new_tab_params.in_incognito = profile->IsOffTheRecord();
      new_tab_params.append_to = OpenPosition::kCurrentTab;
      scene_service_->LoadUrlInNewTab(new_tab_params);
    }
    return;
  }

  notifier_->WillSwitchToTabWithUrl(web_params.url, new_web_state_index);

  NSInteger old_web_state_index = web_state_list->active_index();
  web_state_list->ActivateWebStateAt(new_web_state_index);

  // Close the tab if it is NTP with no back/forward history to avoid having
  // empty tabs.
  if (old_tab_is_ntp_without_history) {
    web_state_list->CloseWebStateAt(old_web_state_index,
                                    WebStateList::CLOSE_USER_ACTION);
  }

  notifier_->DidSwitchToTabWithUrl(web_params.url, new_web_state_index);
}

void UrlLoadingBrowserAgent::LoadUrlInNewTab(const UrlLoadParams& params) {
  DCHECK(scene_service_);
  DCHECK(delegate_);
  DCHECK(browser_);

  ProfileIOS* profile = browser_->GetProfile();
  if (!IsAddNewTabAllowedByPolicy(profile->GetPrefs(), params.in_incognito)) {
    return;
  }

  if (params.in_incognito) {
    IncognitoReauthSceneAgent* reauth_agent =
        [IncognitoReauthSceneAgent agentFromScene:browser_->GetSceneState()];
    DCHECK(!reauth_agent.authenticationRequired);
  }

  ProfileIOS* active_profile =
      scene_service_->GetCurrentBrowser()->GetProfile();

  // Two UrlLoadingServices exist per scene, normal and incognito.  Handle two
  // special cases that need to be sent up to the SceneUrlLoadingService: 1) The
  // URL needs to be loaded by the UrlLoadingService for the other mode. 2) The
  // URL will be loaded in a foreground tab by this UrlLoadingService, but the
  // UI associated with this UrlLoadingService is not currently visible, so the
  // SceneUrlLoadingService needs to switch modes before loading the URL.
  if (params.in_incognito != profile->IsOffTheRecord() ||
      (!params.in_background() &&
       params.in_incognito != active_profile->IsOffTheRecord())) {
    // When sending a load request that switches modes, ensure the tab
    // ends up appended to the end of the model, not just next to what is
    // currently selected in the other mode. This is done with the `append_to`
    // parameter.
    UrlLoadParams scene_params = params;
    scene_params.append_to = OpenPosition::kLastTab;
    scene_service_->LoadUrlInNewTab(scene_params);
    return;
  }

  // Notify only after checking incognito match, otherwise the delegate will
  // take of changing the mode and try again. Notifying before the checks can
  // lead to be calling it twice, and calling 'did' below once.
  if (params.instant_load || !params.in_background()) {
    notifier_->NewTabWillLoadUrl(params.web_params.url, params.user_initiated);
  }

  if (!params.in_background()) {
    LoadUrlInNewTabImpl(params, std::nullopt);
  } else {
    __block void* hint = nullptr;
    __block UrlLoadParams saved_params = params;
    __block base::WeakPtr<UrlLoadingBrowserAgent> weak_ptr =
        weak_ptr_factory_.GetWeakPtr();

    if (params.append_to == OpenPosition::kCurrentTab) {
      hint = browser_->GetWebStateList()->GetActiveWebState();
    }

    [delegate_ animateOpenBackgroundTabFromParams:params
                                       completion:^{
                                         if (weak_ptr) {
                                           weak_ptr->LoadUrlInNewTabImpl(
                                               saved_params, hint);
                                         }
                                       }];
  }
}

void UrlLoadingBrowserAgent::LoadUrlInNewTabImpl(const UrlLoadParams& params,
                                                 std::optional<void*> hint) {
  web::WebState* parent_web_state = nullptr;
  if (params.append_to == OpenPosition::kCurrentTab) {
    parent_web_state = browser_->GetWebStateList()->GetActiveWebState();

    // Detect whether the active tab changed during the animation of opening
    // a tab in the background. This is only needed when opening in background
    // (thus the use of optional).
    //
    // This compare the value read before vs after the animation (as `void*`
    // to prevent trying to dereference a potentially dangling pointer). This
    // is not 100% fool proof as the WebState could have been destroyed, then
    // a new one allocated at the same address and inserted as the active tab.
    // However, this is highly likely to happen. Even if it were to happen, it
    // would be benign as the only drawback is that the wrong tab would be
    // selected upon closing the newly opened tab.
    if (hint && hint.value() != parent_web_state) {
      parent_web_state = nullptr;
    }
  }

  int insertion_index = TabInsertion::kPositionAutomatically;
  if (params.append_to == OpenPosition::kSpecifiedIndex) {
    insertion_index = params.insertion_index;
  }

  TabInsertionBrowserAgent* insertion_agent =
      TabInsertionBrowserAgent::FromBrowser(browser_);
  TabInsertion::Params insertion_params;
  insertion_params.parent = parent_web_state;
  insertion_params.index = insertion_index;
  insertion_params.instant_load = params.instant_load;
  insertion_params.in_background = params.in_background();
  insertion_params.inherit_opener = params.inherit_opener;
  insertion_params.should_skip_new_tab_animation = params.from_external;
  insertion_params.placeholder_title = params.placeholder_title;
  insertion_params.insert_pinned = params.load_pinned;
  insertion_params.insert_in_group = params.load_in_group;
  insertion_params.tab_group = params.tab_group;

  web::WebState* web_state =
      insertion_agent->InsertWebState(params.web_params, insertion_params);

  // If the tab was created as "unrealized" (e.g. `instant_load`
  // being false) then do not force a load. The tab will load
  // when it transition to "realized".
  if (web_state->IsRealized()) {
    web_state->GetNavigationManager()->LoadIfNecessary();
    notifier_->NewTabDidLoadUrl(params.web_params.url, params.user_initiated);
  }
}
