// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/sad_tab_tab_helper.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import <memory>

#import "base/check_op.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/utils/notification_observer_bridge.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/web/model/features.h"
#import "ios/chrome/browser/web/model/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/model/sad_tab_tab_helper_delegate.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

namespace {
// Returns true if the application is in UIApplicationStateActive state.
bool IsApplicationStateActive() {
  return UIApplication.sharedApplication.applicationState ==
         UIApplicationStateActive;
}
}  // namespace

SadTabTabHelper::SadTabTabHelper(web::WebState* web_state,
                                 base::TimeDelta repeat_failure_interval)
    : web_state_(web_state), repeat_failure_interval_(repeat_failure_interval) {
  web_state_->AddObserver(this);
  if (web_state_->IsRealized()) {
    CreateNotificationObserver();
  }
}

SadTabTabHelper::~SadTabTabHelper() {
  DCHECK(!web_state_);
}

void SadTabTabHelper::SetDelegate(id<SadTabTabHelperDelegate> delegate) {
  delegate_ = delegate;
  if (delegate_ && showing_sad_tab_ && web_state_->IsVisible()) {
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
  if (showing_sad_tab_) {
    DCHECK(delegate_);
    [delegate_ sadTabTabHelper:this
        didShowForRepeatedFailure:repeated_failure_];
  }
}

void SadTabTabHelper::WasHidden(web::WebState* web_state) {
  if (showing_sad_tab_) {
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

  // Only show Sad Tab if renderer has crashed in a tab currently visible to the
  // user and only if application is active. Otherwise simpy reloading the page
  // is a better user experience.
  if (!web_state->IsVisible()) {
    requires_reload_on_becoming_visible_ = true;
    return;
  }

  if (!IsApplicationStateActive()) {
    requires_reload_on_becoming_active_ = true;
    return;
  }

  OnVisibleCrash(web_state->GetLastCommittedURL());

  if (repeated_failure_) {
    PresentSadTab();
  } else {
    web_state->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                              true /* check_for_repost */);
  }
}

void SadTabTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // The sad tab is removed when a new navigation begins.
  SetIsShowingSadTab(false);
  // NO-OP is fine if `delegate_` is nil since the `delegate_` will be updated
  // when it is set.
  [delegate_ sadTabTabHelperDismissSadTab:this];
}

void SadTabTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_EQ(web_state_, web_state);
  if (navigation_context->GetUrl().host() == kChromeUICrashHost &&
      navigation_context->GetUrl().scheme() == kChromeUIScheme) {
    OnVisibleCrash(navigation_context->GetUrl());
    PresentSadTab();
  }
}

void SadTabTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

void SadTabTabHelper::WebStateRealized(web::WebState* web_state) {
  CHECK(!background_notification_observer_, base::NotFatalUntil::M125);
  CreateNotificationObserver();
}

void SadTabTabHelper::CreateNotificationObserver() {
  base::RepeatingCallback<void(NSNotification*)> backgrounding_closure =
      base::IgnoreArgs<NSNotification*>(base::BindRepeating(
          &SadTabTabHelper::OnAppDidBecomeActive, weak_factory_.GetWeakPtr()));

  background_notification_observer_ = [[NSNotificationCenter defaultCenter]
      addObserverForName:UIApplicationDidBecomeActiveNotification
                  object:nil
                   queue:nil
              usingBlock:base::CallbackToBlock(backgrounding_closure)];
}

void SadTabTabHelper::OnVisibleCrash(const GURL& url_causing_failure) {
  // Is this failure a repeat-failure requiring the presentation of the Feedback
  // UI rather than the Reload UI?
  base::TimeDelta seconds_since_last_failure =
      last_failed_timer_ ? last_failed_timer_->Elapsed()
                         : base::TimeDelta::Max();

  repeated_failure_ =
      (url_causing_failure.EqualsIgnoringRef(last_failed_url_) &&
       seconds_since_last_failure < repeat_failure_interval_);

  last_failed_url_ = url_causing_failure;
  last_failed_timer_ = std::make_unique<base::ElapsedTimer>();
}

void SadTabTabHelper::PresentSadTab() {
  // NO-OP is fine if `delegate_` is nil since the `delegate_` will be updated
  // when it is set.
  [delegate_ sadTabTabHelper:this
      presentSadTabForWebState:web_state_
               repeatedFailure:repeated_failure_];

  SetIsShowingSadTab(true);

  bool is_pdf = web_state_->GetContentsMimeType() == "application/pdf";
  bool is_chrome_external_file_url =
      last_failed_url_.host() == kChromeUIExternalFileHost &&
      last_failed_url_.scheme() == kChromeUIScheme;
  UMA_HISTOGRAM_BOOLEAN("IOS.SadTab.FileIsPDF", is_pdf);
  UMA_HISTOGRAM_BOOLEAN("IOS.SadTab.URLIsChromeExternalFile",
                        is_chrome_external_file_url);
}

void SadTabTabHelper::SetIsShowingSadTab(bool showing_sad_tab) {
  if (showing_sad_tab_ != showing_sad_tab) {
    showing_sad_tab_ = showing_sad_tab;
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

WEB_STATE_USER_DATA_KEY_IMPL(SadTabTabHelper)
