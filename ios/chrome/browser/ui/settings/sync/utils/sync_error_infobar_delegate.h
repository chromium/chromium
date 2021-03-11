// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SYNC_UTILS_SYNC_ERROR_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SYNC_UTILS_SYNC_ERROR_INFOBAR_DELEGATE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/sync/driver/sync_service_observer.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ui/gfx/image/image.h"

class ChromeBrowserState;
@protocol SyncPresenter;

namespace gfx {
class Image;
}

namespace infobars {
class InfoBarManager;
}

// Shows a sync error in an infobar.
class SyncErrorInfoBarDelegate : public ConfirmInfoBarDelegate,
                                 public syncer::SyncServiceObserver {
 public:
  SyncErrorInfoBarDelegate(ChromeBrowserState* browser_state,
                           id<SyncPresenter> presenter);
  ~SyncErrorInfoBarDelegate() override;

  // Creates a sync error infobar and adds it to |infobar_manager|.
  static bool Create(infobars::InfoBarManager* infobar_manager,
                     ChromeBrowserState* browser_state,
                     id<SyncPresenter> presenter);

  // InfoBarDelegate implementation.
  InfoBarIdentifier GetIdentifier() const override;

  // ConfirmInfoBarDelegate implementation.
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  gfx::Image GetIcon() const override;
  bool Accept() override;

  // ProfileSyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  gfx::Image icon_;
  ChromeBrowserState* browser_state_;
  SyncSetupService::SyncServiceState error_state_;
  base::string16 message_;
  base::string16 button_text_;
  id<SyncPresenter> presenter_;

  DISALLOW_COPY_AND_ASSIGN(SyncErrorInfoBarDelegate);
};

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SYNC_UTILS_SYNC_ERROR_INFOBAR_DELEGATE_H_
