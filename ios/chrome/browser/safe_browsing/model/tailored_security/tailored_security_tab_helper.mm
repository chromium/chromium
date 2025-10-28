// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/tailored_security/tailored_security_tab_helper.h"

#import "base/check.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/browser/tailored_security_service/tailored_security_notification_result.h"
#import "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"
#import "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_observer_util.h"
#import "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_util.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/safe_browsing/model/tailored_security/tailored_security_service_infobar_delegate.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/welcome_back/model/features.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper.h"
#import "ios/web/public/navigation/navigation_context.h"

#pragma mark - TailoredSecurityTabHelper

TailoredSecurityTabHelper::TailoredSecurityTabHelper(
    web::WebState* web_state,
    safe_browsing::TailoredSecurityService* service)
    : service_(service), web_state_(web_state) {
  CHECK(web_state_);
  if (service_) {
    tailored_security_service_observation_.Observe(service_);
  }

  web_state_observation_.Observe(web_state_);
  UpdateFocusAndURL(web_state_->IsVisible(), web_state_->GetLastCommittedURL());
}

TailoredSecurityTabHelper::~TailoredSecurityTabHelper() {
  if (service_) {
    if (has_query_request_) {
      service_->RemoveQueryRequest();
      has_query_request_ = false;
    }
  }
}

#pragma mark - TailoredSecurityServiceObserver

void TailoredSecurityTabHelper::OnTailoredSecurityBitChanged(
    bool enabled,
    base::Time previous_update) {
  if (!enabled || !web_state_->IsVisible()) {
    return;
  }
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!safe_browsing::CanShowUnconsentedTailoredSecurityDialog(
          sync_service, profile->GetPrefs())) {
    return;
  }

  if (base::Time::Now() - previous_update <=
      safe_browsing::kThresholdForInFlowNotification) {
    profile->GetPrefs()->SetBoolean(
        prefs::kAccountTailoredSecurityShownNotification, true);
    ShowInfoBar(safe_browsing::TailoredSecurityServiceMessageState::
                    kUnconsentedAndFlowEnabled);
  }
}

void TailoredSecurityTabHelper::OnTailoredSecurityServiceDestroyed() {
  tailored_security_service_observation_.Reset();
  service_ = nullptr;
}

void TailoredSecurityTabHelper::OnSyncNotificationMessageRequest(
    bool is_enabled) {
  // Notification shouldn't show for non-visible WebStates.
  if (!web_state_->IsVisible()) {
    return;
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  SetSafeBrowsingState(
      profile->GetPrefs(),
      is_enabled ? safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION
                 : safe_browsing::SafeBrowsingState::STANDARD_PROTECTION,
      /*is_esb_enabled_by_account_integration=*/is_enabled);

  if (is_enabled) {
    ShowInfoBar(safe_browsing::TailoredSecurityServiceMessageState::
                    kConsentedAndFlowEnabled);
    // Notify Welcome Back to remove Enhanced Safe Browsing from the eligible
    // features.
    if (IsWelcomeBackEnabled()) {
      MarkWelcomeBackFeatureUsed(BestFeaturesItemType::kEnhancedSafeBrowsing);
    }
  } else {
    ShowInfoBar(safe_browsing::TailoredSecurityServiceMessageState::
                    kConsentedAndFlowDisabled);
  }

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
  web_state_observation_.Reset();
  web_state_ = nullptr;
}

#pragma mark - infobars::InfoBarManager::Observer

void TailoredSecurityTabHelper::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                                 bool animate) {
  if (infobar == infobar_) {
    infobar_manager_scoped_observation_.Reset();
    infobar_ = nullptr;
  }
}

#pragma mark - Private methods

void TailoredSecurityTabHelper::UpdateFocusAndURL(bool focused,
                                                  const GURL& url) {
  DCHECK(web_state_);
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!safe_browsing::CanShowUnconsentedTailoredSecurityDialog(
          sync_service, profile->GetPrefs())) {
    return;
  }

  if (service_) {
    bool should_query =
        focused && safe_browsing::CanQueryTailoredSecurityForUrl(url);
    bool old_should_query =
        focused_ && safe_browsing::CanQueryTailoredSecurityForUrl(last_url_);
    if (should_query && !old_should_query) {
      has_query_request_ = service_->AddQueryRequest();
    }
    if (!should_query && old_should_query && has_query_request_) {
      service_->RemoveQueryRequest();
      has_query_request_ = false;
    }
  }

  focused_ = focused;
  last_url_ = url;
}

void TailoredSecurityTabHelper::ShowInfoBar(
    safe_browsing::TailoredSecurityServiceMessageState message_state) {
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state_);
  if (infobar_) {
    // Previous infobars can continue to exist if the infobar was dismissed
    // without any user action. For example, this happens when an infobar has an
    // expired dismissal. Therefore, we remove it to ensure the new infobar is
    // properly observed.
    infobar_manager->RemoveInfoBar(infobar_);
    DCHECK(!infobar_);
  }
  infobar_manager_scoped_observation_.Observe(infobar_manager);

  std::unique_ptr<safe_browsing::TailoredSecurityServiceInfobarDelegate>
      delegate = std::make_unique<
          safe_browsing::TailoredSecurityServiceInfobarDelegate>(message_state,
                                                                 web_state_);

  std::unique_ptr<infobars::InfoBar> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeTailoredSecurityService, std::move(delegate));
  infobar_ = infobar_manager->AddInfoBar(std::move(infobar),
                                         /*replace_existing=*/true);
}
