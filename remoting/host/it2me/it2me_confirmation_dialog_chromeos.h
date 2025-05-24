// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IT2ME_IT2ME_CONFIRMATION_DIALOG_CHROMEOS_H_
#define REMOTING_HOST_IT2ME_IT2ME_CONFIRMATION_DIALOG_CHROMEOS_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "remoting/host/it2me/it2me_confirmation_dialog.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/window/dialog_delegate.h"

namespace remoting {

class It2MeConfirmationDialogChromeOS : public It2MeConfirmationDialog {
 public:
  explicit It2MeConfirmationDialogChromeOS(DialogStyle style);
  It2MeConfirmationDialogChromeOS(DialogStyle style,
                                  base::TimeDelta auto_accept_timeout);

  It2MeConfirmationDialogChromeOS(const It2MeConfirmationDialogChromeOS&) =
      delete;
  It2MeConfirmationDialogChromeOS& operator=(
      const It2MeConfirmationDialogChromeOS&) = delete;

  ~It2MeConfirmationDialogChromeOS() override;

  // It2MeConfirmationDialog implementation.
  void Show(const std::string& remote_user_email,
            ResultCallback callback) override;

  views::DialogDelegate& GetDialogDelegateForTest();

 private:
  class Core;

  void ShowConfirmationNotification(const std::string& remote_user_email);
  void OnConfirmationNotificationResult(std::optional<int> button_index);

  void OnConfirmationDialogResult(Result result);

  const gfx::VectorIcon& GetIcon() const;
  const ui::ImageModel GetDialogIcon() const;
  std::u16string GetShareButtonLabel() const;

  std::unique_ptr<Core> core_;

  ResultCallback callback_;
  DialogStyle style_;
  base::TimeDelta auto_accept_timeout_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_IT2ME_CONFIRMATION_DIALOG_CHROMEOS_H_
