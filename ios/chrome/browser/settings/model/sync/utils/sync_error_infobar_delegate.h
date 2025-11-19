// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_SYNC_ERROR_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_SYNC_ERROR_INFOBAR_DELEGATE_H_

#import <memory>
#import <string>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_observer.h"

class ProfileIOS;
enum class SyncErrorInfoBarTrigger;
@protocol SyncPresenter;

namespace infobars {
class InfoBarManager;
}

// Defines a period of time when the infobar should not be displayed again
// after a previous dismissal.
inline constexpr base::TimeDelta kSyncErrorInfobarTimeout = base::Hours(24);

// Shows a sync error in an infobar.
class SyncErrorInfoBarDelegate : public ConfirmInfoBarDelegate,
                                 public syncer::SyncServiceObserver {
 public:
  SyncErrorInfoBarDelegate(ProfileIOS* profile,
                           id<SyncPresenter> presenter,
                           SyncErrorInfoBarTrigger trigger);

  SyncErrorInfoBarDelegate(const SyncErrorInfoBarDelegate&) = delete;
  SyncErrorInfoBarDelegate& operator=(const SyncErrorInfoBarDelegate&) = delete;

  ~SyncErrorInfoBarDelegate() override;

  // Creates a sync error infobar and adds it to `infobar_manager`.
  static bool Create(infobars::InfoBarManager* infobar_manager,
                     ProfileIOS* profile,
                     id<SyncPresenter> presenter,
                     SyncErrorInfoBarTrigger trigger);

  // InfoBarDelegate implementation.
  InfoBarIdentifier GetIdentifier() const override;

  // ConfirmInfoBarDelegate implementation.
  std::u16string GetMessageText() const override;
  std::u16string GetTitleText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  void InfoBarDismissed() override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  // Called when the infobar is dismissed through timing out.
  void InfoBarDismissedByTimeout() const;

  // Whether the infobar should display a password error icon instead of the
  // default sync error icon.
  bool DisplayPasswordErrorIcon() const;

 private:
  const raw_ptr<ProfileIOS> profile_;
  base::ScopedObservation<syncer::SyncService, SyncErrorInfoBarDelegate>
      sync_observation_{this};
  const id<SyncPresenter> presenter_;
  const SyncErrorInfoBarTrigger trigger_;
  syncer::SyncService::UserActionableError error_state_;
  std::u16string title_;
  std::u16string message_;
  std::u16string button_text_;
  bool infobar_is_relevant_ = YES;
};

#endif  // IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_SYNC_ERROR_INFOBAR_DELEGATE_H_
