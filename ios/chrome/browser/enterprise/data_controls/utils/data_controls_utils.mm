// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/utils/data_controls_utils.h"

#import "base/notreached.h"
#import "base/strings/utf_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace data_controls {

WarningDialog GetWarningDialog(DataControlsDialog::Type type,
                               std::string_view organization_domain) {
  WarningDialog dialog;
  NSString* label =
      organization_domain.empty()
          ? l10n_util::GetNSString(IDS_DATA_CONTROLS_WARNED_LABEL)
          : l10n_util::GetNSStringF(IDS_DATA_CONTROLS_WARNED_LABEL_WITH_DOMAIN,
                                    base::UTF8ToUTF16(organization_domain));
  switch (type) {
    case DataControlsDialog::Type::kClipboardPasteWarn:
      dialog.title =
          l10n_util::GetNSString(IDS_DATA_CONTROLS_CLIPBOARD_PASTE_WARN_TITLE);
      dialog.label = label;
      dialog.ok_button_id =
          l10n_util::GetNSString(IDS_DATA_CONTROLS_PASTE_WARN_CONTINUE_BUTTON);
      dialog.cancel_button_id =
          l10n_util::GetNSString(IDS_DATA_CONTROLS_PASTE_WARN_CANCEL_BUTTON);
      break;

    case DataControlsDialog::Type::kClipboardCopyWarn:
      dialog.title =
          l10n_util::GetNSString(IDS_DATA_CONTROLS_CLIPBOARD_COPY_WARN_TITLE);
      dialog.label = label;
      dialog.ok_button_id =
          l10n_util::GetNSString(IDS_DATA_CONTROLS_COPY_WARN_CONTINUE_BUTTON);
      dialog.cancel_button_id =
          l10n_util::GetNSString(IDS_DATA_CONTROLS_COPY_WARN_CANCEL_BUTTON);
      break;

    case DataControlsDialog::Type::kClipboardShareWarn:
      dialog.title =
          l10n_util::GetNSString(IDS_DATA_CONTROLS_CLIPBOARD_SHARE_WARN_TITLE);
      dialog.label = label;
      dialog.ok_button_id =
          l10n_util::GetNSString(IDS_DATA_CONTROLS_SHARE_WARN_CONTINUE_BUTTON);
      dialog.cancel_button_id =
          l10n_util::GetNSString(IDS_DATA_CONTROLS_SHARE_WARN_CANCEL_BUTTON);
      break;

    case DataControlsDialog::Type::kClipboardActionWarn:
      dialog.title =
          l10n_util::GetNSString(IDS_DATA_CONTROLS_CLIPBOARD_ACTION_WARN_TITLE);
      dialog.label = label;
      dialog.ok_button_id = l10n_util::GetNSString(IDS_CONTINUE);
      dialog.cancel_button_id = l10n_util::GetNSString(IDS_CANCEL);
      break;

    case DataControlsDialog::Type::kClipboardPasteBlock:
    case DataControlsDialog::Type::kClipboardCopyBlock:
    case DataControlsDialog::Type::kClipboardShareBlock:
    case DataControlsDialog::Type::kClipboardActionBlock:
      // This case should not be reachable in practice.
      NOTREACHED();
  }

  return dialog;
}

}  // namespace data_controls
