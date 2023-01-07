// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/tailored_security/tailored_security_tab_helper.h"

#import "components/safe_browsing/core/browser/tailored_security_service/tailored_security_notification_result.h"
#import "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"
#import "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_observer_util.h"
#import "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_util.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/web/public/navigation/navigation_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - TailoredSecurityTabHelper

TailoredSecurityTabHelper::TailoredSecurityTabHelper(
    web::WebState* web_state,
    safe_browsing::TailoredSecurityService* service)
    : service_(service), web_state_(web_state) {
  bool focused = false;

  if (service_) {
    service_->AddObserver(this);
  }

  if (web_state_) {
    web_state_->AddObserver(this);
    focused = web_state_->IsVisible();
  }
  UpdateFocusAndURL(focused, web_state_->GetLastCommittedURL());
}

TailoredSecurityTabHelper::~TailoredSecurityTabHelper() {
  if (service_) {
    service_->RemoveObserver(this);
    if (has_query_request_) {
      service_->RemoveQueryRequest();
      has_query_request_ = false;
    }
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(TailoredSecurityTabHelper)

#pragma mark - TailoredSecurityServiceObserver

void TailoredSecurityTabHelper::OnTailoredSecurityBitChanged(
    bool enabled,
    base::Time previous_update) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  if (!enabled || !safe_browsing::CanShowUnconsentedTailoredSecurityDialog(
                      identity_manager, browser_state->GetPrefs()))
    return;

  if (base::Time::NowFromSystemTime() - previous_update <=
      base::Minutes(safe_browsing::kThresholdForInFlowNotificationMinutes)) {
    // TODO(crbug.com/1353363): Send signal to show InfoBar.
  }
}

void TailoredSecurityTabHelper::OnTailoredSecurityServiceDestroyed() {
  service_->RemoveObserver(this);
  service_ = nullptr;
}

void TailoredSecurityTabHelper::OnSyncNotificationMessageRequest(
    bool is_enabled) {
  if (!web_state_) {
    if (is_enabled) {
      // Record BrowserState/WebContents not being available.
      safe_browsing::RecordEnabledNotificationResult(
          TailoredSecurityNotificationResult::kNoWebContentsAvailable);
    }
    return;
  }

  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState());
  SetSafeBrowsingState(
      browser_state->GetPrefs(),
      is_enabled ? safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION
                 : safe_browsing::SafeBrowsingState::STANDARD_PROTECTION,
      /*is_esb_enabled_in_sync=*/is_enabled);

  // TODO(crbug.com/1353363): Send output to create InfoBar message.

  if (is_enabled) {
    safe_browsing::RecordEnabledNotificationResult(
        TailoredSecurityNotificationResult::kShown);
  }
}

#pragma mark - web::WebStateObserver
void TailoredSecurityTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  bool sameDocumentNavigation = navigation_context->IsSameDocument();
  if (!sameDocumentNavigation) {
    UpdateFocusAndURL(true, navigation_context->GetUrl());
  }
}

void TailoredSecurityTabHelper::WasShown(web::WebState* web_state) {
  UpdateFocusAndURL(true, last_url_);
}

void TailoredSecurityTabHelper::WasHidden(web::WebState* web_state) {
  UpdateFocusAndURL(false, last_url_);
}

void TailoredSecurityTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
  web_state_ = nullptr;
}

#pragma mark - Private methods

void TailoredSecurityTabHelper::UpdateFocusAndURL(bool focused,
                                                  const GURL& url) {
  DCHECK(web_state_);
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);

  if (!safe_browsing::CanShowUnconsentedTailoredSecurityDialog(
          identity_manager, browser_state->GetPrefs())) {
    return;
  }

  if (service_) {
    bool should_query =
        focused && safe_browsing::CanQueryTailoredSecurityForUrl(url);
    bool old_should_query =
        focused_ && safe_browsing::CanQueryTailoredSecurityForUrl(last_url_);
    if (should_query && !old_should_query) {
      service_->AddQueryRequest();
      has_query_request_ = true;
    }
    if (!should_query && old_should_query) {
      service_->RemoveQueryRequest();
      has_query_request_ = false;
    }
  }

  focused_ = focused;
  last_url_ = url;
}
