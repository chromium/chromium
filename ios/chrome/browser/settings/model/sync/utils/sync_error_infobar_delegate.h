// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_SYNC_ERROR_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_SYNC_ERROR_INFOBAR_DELEGATE_H_

#import <memory>
#import <string>

#import "base/memory/raw_ptr.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_observer.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

@protocol SyncPresenter;

namespace infobars {
class InfoBarManager;
}

// Shows a sync error in an infobar.
class SyncErrorInfoBarDelegate : public ConfirmInfoBarDelegate,
                                 public syncer::SyncServiceObserver {
 public:
  SyncErrorInfoBarDelegate(ProfileIOS* profile, id<SyncPresenter> presenter);

  SyncErrorInfoBarDelegate(const SyncErrorInfoBarDelegate&) = delete;
  SyncErrorInfoBarDelegate& operator=(const SyncErrorInfoBarDelegate&) = delete;

  ~SyncErrorInfoBarDelegate() override;

  // Creates a sync error infobar and adds it to `infobar_manager`.
  static bool Create(infobars::InfoBarManager* infobar_manager,
                     ProfileIOS* profile,
                     id<SyncPresenter> presenter);

  // InfoBarDelegate implementation.
  InfoBarIdentifier GetIdentifier() const override;

  // ConfirmInfoBarDelegate implementation.
  std::u16string GetMessageText() const override;
  std::u16string GetTitleText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  raw_ptr<ProfileIOS> profile_;
  syncer::SyncService::UserActionableError error_state_;
  std::u16string title_;
  std::u16string message_;
  std::u16string button_text_;
  id<SyncPresenter> presenter_;
};

#endif  // IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_SYNC_ERROR_INFOBAR_DELEGATE_H_
