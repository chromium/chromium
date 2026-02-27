// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/enterprise_dialog/model/warning_dialog.h"

#import "base/notimplemented.h"
#import "base/strings/utf_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace enterprise {

WarningDialog GetWarningDialog(DialogType type,
                               std::string_view organization_domain) {
  WarningDialog dialog;
  NSString* label =
      organization_domain.empty()
          ? l10n_util::GetNSString(IDS_DATA_CONTROLS_WARNED_LABEL)
          : l10n_util::GetNSStringF(IDS_DATA_CONTROLS_WARNED_LABEL_WITH_DOMAIN,
                                    base::UTF8ToUTF16(organization_domain));
  switch (type) {
    case DialogType::kClipboardPasteWarn:
      dialog.title =
          l10n_util::GetNSString(IDS_DATA_CONTROLS_CLIPBOARD_PASTE_WARN_TITLE);
      dialog.label = label;
      dialog.ok_button_id =
          l10n_util::GetNSString(IDS_DATA_CONTROLS_PASTE_WARN_CONTINUE_BUTTON);
      dialog.cancel_button_id =
          l10n_util::GetNSString(IDS_DATA_CONTROLS_PASTE_WARN_CANCEL_BUTTON);
      break;

    case DialogType::kClipboardCopyWarn:
      dialog.title =
          l10n_util::GetNSString(IDS_DATA_CONTROLS_CLIPBOARD_COPY_WARN_TITLE);
      dialog.label = label;
      dialog.ok_button_id =
          l10n_util::GetNSString(IDS_DATA_CONTROLS_COPY_WARN_CONTINUE_BUTTON);
      dialog.cancel_button_id =
          l10n_util::GetNSString(IDS_DATA_CONTROLS_COPY_WARN_CANCEL_BUTTON);
      break;

    case DialogType::kClipboardShareWarn:
      dialog.title =
          l10n_util::GetNSString(IDS_DATA_CONTROLS_CLIPBOARD_SHARE_WARN_TITLE);
      dialog.label = label;
      dialog.ok_button_id =
          l10n_util::GetNSString(IDS_DATA_CONTROLS_SHARE_WARN_CONTINUE_BUTTON);
      dialog.cancel_button_id =
          l10n_util::GetNSString(IDS_DATA_CONTROLS_SHARE_WARN_CANCEL_BUTTON);
      break;

    case DialogType::kClipboardActionWarn:
      dialog.title =
          l10n_util::GetNSString(IDS_DATA_CONTROLS_CLIPBOARD_ACTION_WARN_TITLE);
      dialog.label = label;
      dialog.ok_button_id = l10n_util::GetNSString(IDS_CONTINUE);
      dialog.cancel_button_id = l10n_util::GetNSString(IDS_CANCEL);
      break;

    case DialogType::kDownloadSaveWarn:
      dialog.title =
          l10n_util::GetNSString(IDS_IOS_ENTERPRISE_FILE_SAVE_WARN_TITLE);
      dialog.label =
          l10n_util::GetNSString(IDS_IOS_ENTERPRISE_FILE_DOWNLOAD_WARN_LABEL);
      dialog.ok_button_id = l10n_util::GetNSString(
          IDS_IOS_ENTERPRISE_FILE_DOWNLOAD_WARN_CONTINUE_BUTTON);
      dialog.cancel_button_id = l10n_util::GetNSString(IDS_CANCEL);
      break;

    case DialogType::kDownloadShareWarn:
      dialog.title =
          l10n_util::GetNSString(IDS_IOS_ENTERPRISE_FILE_SHARE_WARN_TITLE);
      dialog.label =
          l10n_util::GetNSString(IDS_IOS_ENTERPRISE_FILE_DOWNLOAD_WARN_LABEL);
      dialog.ok_button_id = l10n_util::GetNSString(
          IDS_IOS_ENTERPRISE_FILE_DOWNLOAD_WARN_CONTINUE_BUTTON);
      dialog.cancel_button_id = l10n_util::GetNSString(IDS_CANCEL);
      break;
  }

  return dialog;
}

}  // namespace enterprise
