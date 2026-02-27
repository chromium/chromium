// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/enterprise_dialog/model/warning_dialog.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace enterprise {

using WarningDialogTest = PlatformTest;

TEST_F(WarningDialogTest, GetWarningDialog_Paste) {
  WarningDialog dialog = GetWarningDialog(DialogType::kClipboardPasteWarn, "");
  EXPECT_TRUE([dialog.title
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_CLIPBOARD_PASTE_WARN_TITLE)]);
  EXPECT_TRUE([dialog.label
      isEqualToString:l10n_util::GetNSString(IDS_DATA_CONTROLS_WARNED_LABEL)]);
  EXPECT_TRUE([dialog.ok_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_PASTE_WARN_CONTINUE_BUTTON)]);
  EXPECT_TRUE([dialog.cancel_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_PASTE_WARN_CANCEL_BUTTON)]);
}

TEST_F(WarningDialogTest, GetWarningDialog_Copy) {
  WarningDialog dialog = GetWarningDialog(DialogType::kClipboardCopyWarn, "");
  EXPECT_TRUE([dialog.title
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_CLIPBOARD_COPY_WARN_TITLE)]);
  EXPECT_TRUE([dialog.label
      isEqualToString:l10n_util::GetNSString(IDS_DATA_CONTROLS_WARNED_LABEL)]);
  EXPECT_TRUE([dialog.ok_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_COPY_WARN_CONTINUE_BUTTON)]);
  EXPECT_TRUE([dialog.cancel_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_COPY_WARN_CANCEL_BUTTON)]);
}

TEST_F(WarningDialogTest, GetWarningDialog_Share) {
  WarningDialog dialog = GetWarningDialog(DialogType::kClipboardShareWarn, "");
  EXPECT_TRUE([dialog.title
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_CLIPBOARD_SHARE_WARN_TITLE)]);
  EXPECT_TRUE([dialog.label
      isEqualToString:l10n_util::GetNSString(IDS_DATA_CONTROLS_WARNED_LABEL)]);
  EXPECT_TRUE([dialog.ok_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_SHARE_WARN_CONTINUE_BUTTON)]);
  EXPECT_TRUE([dialog.cancel_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_SHARE_WARN_CANCEL_BUTTON)]);
}

TEST_F(WarningDialogTest, GetWarningDialog_Action) {
  WarningDialog dialog = GetWarningDialog(DialogType::kClipboardActionWarn, "");
  EXPECT_TRUE([dialog.title
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_CLIPBOARD_ACTION_WARN_TITLE)]);
  EXPECT_TRUE([dialog.label
      isEqualToString:l10n_util::GetNSString(IDS_DATA_CONTROLS_WARNED_LABEL)]);
  EXPECT_TRUE([dialog.ok_button_id
      isEqualToString:l10n_util::GetNSString(IDS_CONTINUE)]);
  EXPECT_TRUE([dialog.cancel_button_id
      isEqualToString:l10n_util::GetNSString(IDS_CANCEL)]);
}

TEST_F(WarningDialogTest, GetWarningDialog_Paste_WithDomain) {
  WarningDialog dialog =
      GetWarningDialog(DialogType::kClipboardPasteWarn, "google.com");
  EXPECT_TRUE([dialog.title
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_CLIPBOARD_PASTE_WARN_TITLE)]);
  EXPECT_TRUE([dialog.label
      isEqualToString:l10n_util::GetNSStringF(
                          IDS_DATA_CONTROLS_WARNED_LABEL_WITH_DOMAIN,
                          u"google.com")]);
  EXPECT_TRUE([dialog.ok_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_PASTE_WARN_CONTINUE_BUTTON)]);
  EXPECT_TRUE([dialog.cancel_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_PASTE_WARN_CANCEL_BUTTON)]);
}

TEST_F(WarningDialogTest, GetWarningDialog_Copy_WithDomain) {
  WarningDialog dialog =
      GetWarningDialog(DialogType::kClipboardCopyWarn, "google.com");
  EXPECT_TRUE([dialog.title
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_CLIPBOARD_COPY_WARN_TITLE)]);
  EXPECT_TRUE([dialog.label
      isEqualToString:l10n_util::GetNSStringF(
                          IDS_DATA_CONTROLS_WARNED_LABEL_WITH_DOMAIN,
                          u"google.com")]);
  EXPECT_TRUE([dialog.ok_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_COPY_WARN_CONTINUE_BUTTON)]);
  EXPECT_TRUE([dialog.cancel_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_COPY_WARN_CANCEL_BUTTON)]);
}

TEST_F(WarningDialogTest, GetWarningDialog_Share_WithDomain) {
  WarningDialog dialog =
      GetWarningDialog(DialogType::kClipboardShareWarn, "google.com");
  EXPECT_TRUE([dialog.title
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_CLIPBOARD_SHARE_WARN_TITLE)]);
  EXPECT_TRUE([dialog.label
      isEqualToString:l10n_util::GetNSStringF(
                          IDS_DATA_CONTROLS_WARNED_LABEL_WITH_DOMAIN,
                          u"google.com")]);
  EXPECT_TRUE([dialog.ok_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_SHARE_WARN_CONTINUE_BUTTON)]);
  EXPECT_TRUE([dialog.cancel_button_id
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_SHARE_WARN_CANCEL_BUTTON)]);
}

TEST_F(WarningDialogTest, GetWarningDialog_Action_WithDomain) {
  WarningDialog dialog =
      GetWarningDialog(DialogType::kClipboardActionWarn, "google.com");
  EXPECT_TRUE([dialog.title
      isEqualToString:l10n_util::GetNSString(
                          IDS_DATA_CONTROLS_CLIPBOARD_ACTION_WARN_TITLE)]);
  EXPECT_TRUE([dialog.label
      isEqualToString:l10n_util::GetNSStringF(
                          IDS_DATA_CONTROLS_WARNED_LABEL_WITH_DOMAIN,
                          u"google.com")]);
  EXPECT_TRUE([dialog.ok_button_id
      isEqualToString:l10n_util::GetNSString(IDS_CONTINUE)]);
  EXPECT_TRUE([dialog.cancel_button_id
      isEqualToString:l10n_util::GetNSString(IDS_CANCEL)]);
}

TEST_F(WarningDialogTest, GetWarningDialog_Download_Save) {
  WarningDialog dialog = GetWarningDialog(DialogType::kDownloadSaveWarn, "");
  EXPECT_TRUE([dialog.title
      isEqualToString:l10n_util::GetNSString(
                          IDS_IOS_ENTERPRISE_FILE_SAVE_WARN_TITLE)]);
  EXPECT_TRUE([dialog.label
      isEqualToString:l10n_util::GetNSString(
                          IDS_IOS_ENTERPRISE_FILE_DOWNLOAD_WARN_LABEL)]);
  EXPECT_TRUE([dialog.ok_button_id
      isEqualToString:
          l10n_util::GetNSString(
              IDS_IOS_ENTERPRISE_FILE_DOWNLOAD_WARN_CONTINUE_BUTTON)]);
  EXPECT_TRUE([dialog.cancel_button_id
      isEqualToString:l10n_util::GetNSString(IDS_CANCEL)]);
}

TEST_F(WarningDialogTest, GetWarningDialog_Download_Share) {
  WarningDialog dialog = GetWarningDialog(DialogType::kDownloadShareWarn, "");
  EXPECT_TRUE([dialog.title
      isEqualToString:l10n_util::GetNSString(
                          IDS_IOS_ENTERPRISE_FILE_SHARE_WARN_TITLE)]);
  EXPECT_TRUE([dialog.label
      isEqualToString:l10n_util::GetNSString(
                          IDS_IOS_ENTERPRISE_FILE_DOWNLOAD_WARN_LABEL)]);
  EXPECT_TRUE([dialog.ok_button_id
      isEqualToString:
          l10n_util::GetNSString(
              IDS_IOS_ENTERPRISE_FILE_DOWNLOAD_WARN_CONTINUE_BUTTON)]);
  EXPECT_TRUE([dialog.cancel_button_id
      isEqualToString:l10n_util::GetNSString(IDS_CANCEL)]);
}

}  // namespace enterprise
