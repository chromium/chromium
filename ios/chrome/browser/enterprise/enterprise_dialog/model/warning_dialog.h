// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_ENTERPRISE_DIALOG_MODEL_WARNING_DIALOG_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_ENTERPRISE_DIALOG_MODEL_WARNING_DIALOG_H_

#import <UIKit/UIKit.h>

#import <string_view>

// TODO(crbug.com/481679210): handle new download cases when UX design is
// finalized.
namespace enterprise {

// Represents the type of warning dialog, based on the action or policy that
// triggered it. This will change the strings and buttons in the warning dialog
// accordingly.
enum DialogType {
  kClipboardPasteWarn,
  kClipboardCopyWarn,
  kClipboardShareWarn,
  kClipboardActionWarn,

  // Triggered by downloading from Save prompt.
  kDownloadSaveWarn,

  // Triggered by downloading from Share sheet.
  kDownloadShareWarn
};

// The warning dialog shown to the user when an Enterprise Policy is triggered.
struct WarningDialog {
  NSString* title;
  NSString* label;
  NSString* ok_button_id;
  NSString* cancel_button_id;
};

// Get the warning dialog according to the dialog type and the organization
// domain.
WarningDialog GetWarningDialog(DialogType type,
                               std::string_view organization_domain);

}  // namespace enterprise

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_ENTERPRISE_DIALOG_MODEL_WARNING_DIALOG_H_
