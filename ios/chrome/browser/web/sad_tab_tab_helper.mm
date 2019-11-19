// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/sad_tab_tab_helper.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#include "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#include "ios/chrome/browser/ui/fullscreen/scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/sad_tab_tab_helper_delegate.h"
#include "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const double SadTabTabHelper::kDefaultRepeatFailureInterval = 60.0f;

namespace {
// Returns true if the application is in UIApplicationStateActive state.
bool IsApplicationStateActive() {
  return UIApplication.sharedApplication.applicationState ==
         UIApplicationStateActive;
}
}  // namespace

SadTabTabHelper::SadTabTabHelper(web::WebState* web_state)
    : SadTabTabHelper(web_state, kDefaultRepeatFailureInterval) {}

SadTabTabHelper::SadTabTabHelper(web::WebState* web_state,
                                 double repeat_failure_interval)
    : web_state_(web_state), repeat_failure_interval_(repeat_failure_interval) {
  web_state_->AddObserver(this);
  AddApplicationDidBecomeActiveObserver();
}

SadTabTabHelper::~SadTabTabHelper() {
  DCHECK(!application_did_become_active_observer_);
  DCHECK(!web_state_);
}

void SadTabTabHelper::CreateForWebState(web::WebState* web_state) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(),
                           base::WrapUnique(new SadTabTabHelper(web_state)));
  }
}

void SadTabTabHelper::CreateForWebState(web::WebState* web_state,
                                        double repeat_failure_interval) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(),
                           base::WrapUnique(new SadTabTabHelper(
                               web_state, repeat_failure_interval)));
  }
}

void SadTabTabHelper::SetDelegate(id<SadTabTabHelperDelegate> delegate) {
  delegate_ = delegate;
  if (delegate_ && showing_sad_tab_ && web_state_->IsVisible()) {
    UpdateFullscreenDisabler();
    [delegate_ sadTabTabHelper:this
        didShowForRepeatedFailure:repeated_failure_];
  }
}

void SadTabTabHelper::WasShown(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  if (requires_reload_on_becoming_visible_) {
    ReloadTab();
    requires_reload_on_becoming_visible_ = false;
  }
  UpdateFullscreenDisabler();
  if (showing_sad_tab_) {
    DCHECK(delegate_);
    [delegate_ sadTabTabHelper:this
        didShowForRepeatedFailure:repeated_failure_];
  }
}

void SadTabTabHelper::WasHidden(web::WebState* web_state) {
  UpdateFullscreenDisabler();
  if (showing_sad_tab_) {
    DCHECK(delegate_);
    [delegate_ sadTabTabHelperDidHide:this];
  }
}

void SadTabTabHelper::RenderProcessGone(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);

  // Don't present a sad tab on top of an NTP.
  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(web_state);
  if (NTPHelper && NTPHelper->IsActive()) {
    return;
  }

  if (!web_state->IsVisible()) {
    requires_reload_on_becoming_visible_ = true;
    return;
  }

  if (!IsApplicationStateActive()) {
    requires_reload_on_becoming_active_ = true;
    return;
  }

  // Only show Sad Tab if renderer has crashed in a tab currently visible to the
  // user and only if application is active. Otherwise simpy reloading the page
  // is a better user experience.
  PresentSadTab(web_state->GetLastCommittedURL());
}

void SadTabTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // The sad tab is removed when a new navigation begins.
  SetIsShowingSadTab(false);
  // NO-OP is fine if |delegate_| is nil since the |delegate_| will be updated
  // when it is set.
  [delegate_ sadTabTabHelperDismissSadTab:this];
}

void SadTabTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_EQ(web_state_, web_state);
  if (navigation_context->GetUrl().host() == kChromeUICrashHost &&
      navigation_context->GetUrl().scheme() == kChromeUIScheme) {
    PresentSadTab(navigation_context->GetUrl());
  }
}

void SadTabTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
  RemoveApplicationDidBecomeActiveObserver();
}

void SadTabTabHelper::PresentSadTab(const GURL& url_causing_failure) {
  // Is this failure a repeat-failure requiring the presentation of the Feedback
  // UI rather than the Reload UI?
  double seconds_since_last_failure =
      last_failed_timer_ ? last_failed_timer_->Elapsed().InSecondsF() : DBL_MAX;

  repeated_failure_ =
      (url_causing_failure.EqualsIgnoringRef(last_failed_url_) &&
       seconds_since_last_failure < repeat_failure_interval_);

  // NO-OP is fine if |delegate_| is nil since the |delegate_| will be updated
  // when it is set.
  [delegate_ sadTabTabHelper:this
      presentSadTabForWebState:web_state_
               repeatedFailure:repeated_failure_];

  last_failed_url_ = url_causing_failure;
  last_failed_timer_ = std::make_unique<base::ElapsedTimer>();

  SetIsShowingSadTab(true);
}

void SadTabTabHelper::SetIsShowingSadTab(bool showing_sad_tab) {
  if (showing_sad_tab_ != showing_sad_tab) {
    showing_sad_tab_ = showing_sad_tab;
    UpdateFullscreenDisabler();
  }
}

void SadTabTabHelper::ReloadTab() {
  PagePlaceholderTabHelper::FromWebState(web_state_)
      ->AddPlaceholderForNextNavigation();
  web_state_->GetNavigationManager()->LoadIfNecessary();
}

void SadTabTabHelper::OnAppDidBecomeActive() {
  if (!requires_reload_on_becoming_active_)
    return;
  if (web_state_->IsVisible()) {
    ReloadTab();
  } else {
    requires_reload_on_becoming_visible_ = true;
  }
  requires_reload_on_becoming_active_ = false;
}

void SadTabTabHelper::AddApplicationDidBecomeActiveObserver() {
  application_did_become_active_observer_ =
      [[NSNotificationCenter defaultCenter]
          addObserverForName:UIApplicationDidBecomeActiveNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification*) {
                    OnAppDidBecomeActive();
                  }];
}

void SadTabTabHelper::RemoveApplicationDidBecomeActiveObserver() {
  if (application_did_become_active_observer_) {
    [[NSNotificationCenter defaultCenter]
        removeObserver:application_did_become_active_observer_];
    application_did_become_active_observer_ = nil;
  }
}

void SadTabTabHelper::UpdateFullscreenDisabler() {
  if (showing_sad_tab_ && web_state_->IsVisible()) {
    ios::ChromeBrowserState* browser_state =
        ios::ChromeBrowserState::FromBrowserState(
            web_state_->GetBrowserState());
    FullscreenController* fullscreen_controller =
        FullscreenControllerFactory::GetInstance()->GetForBrowserState(
            browser_state);
    if (fullscreen_controller) {
      fullscreen_disabler_ =
          std::make_unique<ScopedFullscreenDisabler>(fullscreen_controller);
    }
  } else {
    fullscreen_disabler_ = nullptr;
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(SadTabTabHelper)
