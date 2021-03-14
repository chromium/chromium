// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webxr/android/xr_install_infobar.h"

#include <string>

#include "base/callback.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "ui/base/l10n/l10n_util.h"

namespace webxr {

XrInstallInfoBar::XrInstallInfoBar(
    InfoBarIdentifier identifier,
    int icon_id,
    int message_id,
    int ok_button_id,
    base::OnceCallback<void(bool)> install_callback)
    : identifier_(identifier),
      icon_id_(icon_id),
      message_id_(message_id),
      ok_button_id_(ok_button_id),
      install_callback_(std::move(install_callback)) {}

XrInstallInfoBar::~XrInstallInfoBar() = default;

infobars::InfoBarDelegate::InfoBarIdentifier XrInstallInfoBar::GetIdentifier()
    const {
  return identifier_;
}

int XrInstallInfoBar::GetIconId() const {
  return icon_id_;
}

int XrInstallInfoBar::GetButtons() const {
  return BUTTON_OK;
}

std::u16string XrInstallInfoBar::GetButtonLabel(InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(ok_button_id_);
}

std::u16string XrInstallInfoBar::GetMessageText() const {
  return l10n_util::GetStringUTF16(message_id_);
}

bool XrInstallInfoBar::Accept() {
  std::move(install_callback_).Run(true);
  return true;
}

bool XrInstallInfoBar::Cancel() {
  std::move(install_callback_).Run(false);
  return true;
}

void XrInstallInfoBar::InfoBarDismissed() {
  std::move(install_callback_).Run(false);
}
}  // namespace webxr
