// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/model/sync/utils/sync_error_infobar_delegate.h"

#import <utility>

#import "base/check.h"
#import "base/memory/ptr_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_delegate.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_utils.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/model/infobar_utils.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_presenter.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_util.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

// static
bool SyncErrorInfoBarDelegate::Create(infobars::InfoBarManager* infobar_manager,
                                      ProfileIOS* profile,
                                      id<SyncPresenter> presenter) {
  DCHECK(infobar_manager);
  std::unique_ptr<SyncErrorInfoBarDelegate> delegate(
      new SyncErrorInfoBarDelegate(profile, presenter));
  std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeSyncError, std::move(delegate));
  return !!infobar_manager->AddInfoBar(std::move(infobar));
}

SyncErrorInfoBarDelegate::SyncErrorInfoBarDelegate(ProfileIOS* profile,
                                                   id<SyncPresenter> presenter)
    : profile_(profile), presenter_(presenter) {
  DCHECK(!profile->IsOffTheRecord());
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  DCHECK(sync_service);
  // Set all of the UI based on the sync state at the same time to ensure
  // they all correspond to the same sync error.
  error_state_ = sync_service->GetUserActionableError();
  title_ = GetSyncErrorInfoBarTitleForProfile(profile_);
  message_ = base::SysNSStringToUTF16(GetSyncErrorMessageForProfile(profile_));
  button_text_ =
      base::SysNSStringToUTF16(GetSyncErrorButtonTitleForProfile(profile_));

  // Register for sync status changes.
  sync_service->AddObserver(this);
}

SyncErrorInfoBarDelegate::~SyncErrorInfoBarDelegate() {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  sync_service->RemoveObserver(this);
}

infobars::InfoBarDelegate::InfoBarIdentifier
SyncErrorInfoBarDelegate::GetIdentifier() const {
  return SYNC_ERROR_INFOBAR_DELEGATE_IOS;
}

std::u16string SyncErrorInfoBarDelegate::GetMessageText() const {
  return message_;
}

std::u16string SyncErrorInfoBarDelegate::GetTitleText() const {
  return title_;
}

int SyncErrorInfoBarDelegate::GetButtons() const {
  return button_text_.empty() ? BUTTON_NONE : BUTTON_OK;
}

std::u16string SyncErrorInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK(button == BUTTON_OK);
  return button_text_;
}

bool SyncErrorInfoBarDelegate::Accept() {
  switch (error_state_) {
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
      [presenter_ showPrimaryAccountReauth];
      break;

    case syncer::SyncService::UserActionableError::kNone:
      DCHECK(ShouldShowSyncSettings(error_state_));
      [presenter_ showAccountSettings];
      break;

    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
      [presenter_ showSyncPassphraseSettings];
      break;

    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
      [presenter_
          showTrustedVaultReauthForFetchKeysWithTrigger:
              syncer::TrustedVaultUserActionTriggerForUMA::kNewTabPageInfobar];
      break;

    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      [presenter_
          showTrustedVaultReauthForDegradedRecoverabilityWithTrigger:
              syncer::TrustedVaultUserActionTriggerForUMA::kNewTabPageInfobar];
      break;
  }

  return false;
}

void SyncErrorInfoBarDelegate::OnStateChanged(syncer::SyncService* sync) {
  // If the inforbar is in the process of being removed, nothing must be done.
  infobars::InfoBar* infobar = this->infobar();
  if (!infobar) {
    return;
  }
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  syncer::SyncService::UserActionableError new_error_state =
      sync_service->GetUserActionableError();
  if (error_state_ == new_error_state) {
    return;
  }
  error_state_ = new_error_state;
  if (new_error_state == syncer::SyncService::UserActionableError::kNone) {
    infobar->RemoveSelf();
  } else {
    infobars::InfoBarManager* infobar_manager = infobar->owner();
    if (infobar_manager) {
      std::unique_ptr<ConfirmInfoBarDelegate> new_infobar_delegate(
          new SyncErrorInfoBarDelegate(profile_, presenter_));
      infobar_manager->ReplaceInfoBar(
          infobar, CreateConfirmInfoBar(std::move(new_infobar_delegate)));
    }
  }
}
