// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync/utils/sync_error_infobar_delegate.h"

#import <UIKit/UIKit.h>

#import <utility>

#import "base/check.h"
#import "base/memory/ptr_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_delegate.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/sync/driver/sync_service.h"
#import "components/sync/driver/sync_service_utils.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/infobars/infobar_utils.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_presenter.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Sync error icon.
NSString* const kGoogleServicesSyncErrorImage = @"google_services_sync_error";

}  // namespace

// static
bool SyncErrorInfoBarDelegate::Create(infobars::InfoBarManager* infobar_manager,
                                      ChromeBrowserState* browser_state,
                                      id<SyncPresenter> presenter) {
  DCHECK(infobar_manager);
  std::unique_ptr<ConfirmInfoBarDelegate> delegate(
      new SyncErrorInfoBarDelegate(browser_state, presenter));
  return !!infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::move(delegate)));
}

SyncErrorInfoBarDelegate::SyncErrorInfoBarDelegate(
    ChromeBrowserState* browser_state,
    id<SyncPresenter> presenter)
    : browser_state_(browser_state), presenter_(presenter) {
  DCHECK(!browser_state->IsOffTheRecord());
  icon_ = gfx::Image([UIImage imageNamed:kGoogleServicesSyncErrorImage]);
  SyncSetupService* sync_setup_service =
      SyncSetupServiceFactory::GetForBrowserState(browser_state);
  DCHECK(sync_setup_service);
  // Set all of the UI based on the sync state at the same time to ensure
  // they all correspond to the same sync error.
  error_state_ = sync_setup_service->GetSyncServiceState();
  message_ = base::SysNSStringToUTF16(
      GetSyncErrorMessageForBrowserState(browser_state_));
  button_text_ = base::SysNSStringToUTF16(
      GetSyncErrorButtonTitleForBrowserState(browser_state_));

  // Register for sync status changes.
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForBrowserState(browser_state_);
  sync_service->AddObserver(this);
}

SyncErrorInfoBarDelegate::~SyncErrorInfoBarDelegate() {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForBrowserState(browser_state_);
  sync_service->RemoveObserver(this);
}

infobars::InfoBarDelegate::InfoBarIdentifier
SyncErrorInfoBarDelegate::GetIdentifier() const {
  return SYNC_ERROR_INFOBAR_DELEGATE_IOS;
}

std::u16string SyncErrorInfoBarDelegate::GetMessageText() const {
  return message_;
}

int SyncErrorInfoBarDelegate::GetButtons() const {
  return button_text_.empty() ? BUTTON_NONE : BUTTON_OK;
}

std::u16string SyncErrorInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK(button == BUTTON_OK);
  return button_text_;
}

ui::ImageModel SyncErrorInfoBarDelegate::GetIcon() const {
  return ui::ImageModel::FromImage(icon_);
}

bool SyncErrorInfoBarDelegate::UseIconBackgroundTint() const {
  return false;
}

bool SyncErrorInfoBarDelegate::Accept() {
  if (error_state_ == SyncSetupService::kSyncServiceSignInNeedsUpdate) {
    [presenter_ showReauthenticateSignin];
  } else if (ShouldShowSyncSettings(error_state_)) {
    [presenter_ showAccountSettings];
  } else if (error_state_ == SyncSetupService::kSyncServiceNeedsPassphrase) {
    [presenter_ showSyncPassphraseSettings];
  } else if (error_state_ ==
             SyncSetupService::kSyncServiceNeedsTrustedVaultKey) {
    [presenter_
        showTrustedVaultReauthForFetchKeysWithTrigger:
            syncer::TrustedVaultUserActionTriggerForUMA::kNewTabPageInfobar];
  } else if (error_state_ ==
             SyncSetupService::kSyncServiceTrustedVaultRecoverabilityDegraded) {
    [presenter_
        showTrustedVaultReauthForDegradedRecoverabilityWithTrigger:
            syncer::TrustedVaultUserActionTriggerForUMA::kNewTabPageInfobar];
  }
  return false;
}

void SyncErrorInfoBarDelegate::OnStateChanged(syncer::SyncService* sync) {
  // If the inforbar is in the process of being removed, nothing must be done.
  infobars::InfoBar* infobar = this->infobar();
  if (!infobar)
    return;
  SyncSetupService* sync_setup_service =
      SyncSetupServiceFactory::GetForBrowserState(browser_state_);
  SyncSetupService::SyncServiceState new_error_state =
      sync_setup_service->GetSyncServiceState();
  if (error_state_ == new_error_state)
    return;
  error_state_ = new_error_state;
  if (IsTransientSyncError(new_error_state)) {
    infobar->RemoveSelf();
  } else {
    infobars::InfoBarManager* infobar_manager = infobar->owner();
    if (infobar_manager) {
      std::unique_ptr<ConfirmInfoBarDelegate> new_infobar_delegate(
          new SyncErrorInfoBarDelegate(browser_state_, presenter_));
      infobar_manager->ReplaceInfoBar(
          infobar, CreateConfirmInfoBar(std::move(new_infobar_delegate)));
    }
  }
}
