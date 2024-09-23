// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_TEST_MOCK_SYNC_ERROR_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_TEST_MOCK_SYNC_ERROR_INFOBAR_DELEGATE_H_

#import <UIKit/UIKit.h>

#import <string>

#import "ios/chrome/browser/settings/model/sync/utils/sync_error_infobar_delegate.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "ui/base/models/image_model.h"

@protocol SyncPresenter;

// Mock version of SyncErrorInfoBarDelegate.
class MockSyncErrorInfoBarDelegate : public SyncErrorInfoBarDelegate {
 public:
  MockSyncErrorInfoBarDelegate(ProfileIOS* profile,
                               id<SyncPresenter> presenter,
                               std::u16string title_text = u"",
                               std::u16string message_text = u"",
                               std::u16string button_label_text = u"",
                               bool use_icon_background_tint = true);

  ~MockSyncErrorInfoBarDelegate() override;

  MOCK_METHOD(bool, Accept, (), (override));
  MOCK_METHOD(void, InfoBarDismissed, (), (override));
  MOCK_METHOD(std::u16string, GetTitleText, (), (const, override));
  MOCK_METHOD(std::u16string, GetMessageText, (), (const, override));
  MOCK_METHOD(std::u16string,
              GetButtonLabel,
              (InfoBarButton button),
              (const, override));
  MOCK_METHOD(bool, UseIconBackgroundTint, (), (const, override));
  MOCK_METHOD(ui::ImageModel, GetIcon, (), (const, override));
};

#endif  // IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_TEST_MOCK_SYNC_ERROR_INFOBAR_DELEGATE_H_
