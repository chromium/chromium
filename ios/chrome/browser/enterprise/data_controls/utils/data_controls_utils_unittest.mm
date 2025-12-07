// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/utils/data_controls_utils.h"

#import "components/strings/grit/components_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace data_controls {

using DataControlsUtilsTest = PlatformTest;

TEST_F(DataControlsUtilsTest, GetWarningDialog_Paste) {
  WarningDialog dialog =
      GetWarningDialog(DataControlsDialog::Type::kClipboardPasteWarn, "");
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

TEST_F(DataControlsUtilsTest, GetWarningDialog_Copy) {
  WarningDialog dialog =
      GetWarningDialog(DataControlsDialog::Type::kClipboardCopyWarn, "");
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

TEST_F(DataControlsUtilsTest, GetWarningDialog_Share) {
  WarningDialog dialog =
      GetWarningDialog(DataControlsDialog::Type::kClipboardShareWarn, "");
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

TEST_F(DataControlsUtilsTest, GetWarningDialog_Action) {
  WarningDialog dialog =
      GetWarningDialog(DataControlsDialog::Type::kClipboardActionWarn, "");
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

TEST_F(DataControlsUtilsTest, GetWarningDialog_Paste_WithDomain) {
  WarningDialog dialog = GetWarningDialog(
      DataControlsDialog::Type::kClipboardPasteWarn, "google.com");
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

TEST_F(DataControlsUtilsTest, GetWarningDialog_Copy_WithDomain) {
  WarningDialog dialog = GetWarningDialog(
      DataControlsDialog::Type::kClipboardCopyWarn, "google.com");
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

TEST_F(DataControlsUtilsTest, GetWarningDialog_Share_WithDomain) {
  WarningDialog dialog = GetWarningDialog(
      DataControlsDialog::Type::kClipboardShareWarn, "google.com");
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

TEST_F(DataControlsUtilsTest, GetWarningDialog_Action_WithDomain) {
  WarningDialog dialog = GetWarningDialog(
      DataControlsDialog::Type::kClipboardActionWarn, "google.com");
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

}  // namespace data_controls
