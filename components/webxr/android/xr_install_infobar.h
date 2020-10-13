// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBXR_ANDROID_XR_INSTALL_INFOBAR_H_
#define COMPONENTS_WEBXR_ANDROID_XR_INSTALL_INFOBAR_H_

#include "base/callback.h"
#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"

namespace webxr {

// An interface derived from InfoBarDelegate implemented by objects wishing to
// control a ConfirmInfoBar.
class XrInstallInfoBar : public ConfirmInfoBarDelegate {
 public:
  // Constructor for XrInstallInfoBar, the callback is guaranteed to be called,
  // if the InfoBar is accepted, cancelled, or dismissed. The Callback will be
  // passed a bool indicating whether the result of the InfoBar was "accepted."
  XrInstallInfoBar(InfoBarIdentifier identifier,
                   int icon_id,
                   int message_id,
                   int ok_button_id,
                   base::OnceCallback<void(bool)> install_callback);
  ~XrInstallInfoBar() override;

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;

 private:
  // Called when the OK button is pressed. If this function returns true, the
  // infobar is then immediately closed. Subclasses MUST NOT return true if in
  // handling this call something triggers the infobar to begin closing.
  bool Accept() override;

  // Called when the Cancel button is pressed. If this function returns true,
  // the infobar is then immediately closed. Subclasses MUST NOT return true if
  // in handling this call something triggers the infobar to begin closing.
  bool Cancel() override;

  // Called when the user clicks on the close button to dismiss the infobar.
  void InfoBarDismissed() override;

  const infobars::InfoBarDelegate::InfoBarIdentifier identifier_;
  const int icon_id_;
  const int message_id_;
  const int ok_button_id_;
  base::OnceCallback<void(bool)> install_callback_;
};
}  // namespace webxr

#endif  // COMPONENTS_WEBXR_ANDROID_XR_INSTALL_INFOBAR_H_
