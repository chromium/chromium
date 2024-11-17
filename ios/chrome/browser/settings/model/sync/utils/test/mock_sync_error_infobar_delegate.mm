// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/model/sync/utils/test/mock_sync_error_infobar_delegate.h"

#import "ui/gfx/image/image.h"

MockSyncErrorInfoBarDelegate::MockSyncErrorInfoBarDelegate(
    ProfileIOS* profile,
    id<SyncPresenter> presenter,
    std::u16string title_text,
    std::u16string message_text,
    std::u16string button_label_text,
    bool use_icon_background_tint)
    : SyncErrorInfoBarDelegate(profile, presenter) {
  ON_CALL(*this, GetTitleText).WillByDefault(testing::Return(title_text));
  ON_CALL(*this, GetMessageText).WillByDefault(testing::Return(message_text));
  ON_CALL(*this, GetButtonLabel)
      .WillByDefault(testing::Return(button_label_text));
  ON_CALL(*this, UseIconBackgroundTint)
      .WillByDefault(testing::Return(use_icon_background_tint));
  ON_CALL(*this, GetIcon)
      .WillByDefault(testing::Return(
          ui::ImageModel::FromImage(gfx::Image([[UIImage alloc] init]))));
}

MockSyncErrorInfoBarDelegate::~MockSyncErrorInfoBarDelegate() = default;
