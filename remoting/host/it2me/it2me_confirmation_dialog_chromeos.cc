// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/i18n/message_formatter.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "remoting/base/string_resources.h"
#include "remoting/host/chromeos/message_box.h"
#include "remoting/host/it2me/it2me_confirmation_dialog.h"
#include "ui/base/l10n/l10n_util.h"

namespace remoting {

class It2MeConfirmationDialogChromeOS : public It2MeConfirmationDialog {
 public:
  It2MeConfirmationDialogChromeOS();
  ~It2MeConfirmationDialogChromeOS() override;

  // It2MeConfirmationDialog implementation.
  void Show(const std::string& remote_user_email,
            const ResultCallback& callback) override;

 private:
  // Handles result from |message_box_|.
  void OnMessageBoxResult(MessageBox::Result result);

  std::unique_ptr<MessageBox> message_box_;
  ResultCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(It2MeConfirmationDialogChromeOS);
};

It2MeConfirmationDialogChromeOS::It2MeConfirmationDialogChromeOS() = default;

It2MeConfirmationDialogChromeOS::~It2MeConfirmationDialogChromeOS() = default;

void It2MeConfirmationDialogChromeOS::Show(const std::string& remote_user_email,
                                           const ResultCallback& callback) {
  DCHECK(!remote_user_email.empty());
  callback_ = callback;

  message_box_ = std::make_unique<MessageBox>(
      l10n_util::GetStringUTF16(IDS_MODE_IT2ME),
      base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(
              IDS_SHARE_CONFIRM_DIALOG_MESSAGE_WITH_USERNAME),
          base::UTF8ToUTF16(remote_user_email)),
      l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_CONFIRM),
      l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_DECLINE),
      base::Bind(&It2MeConfirmationDialogChromeOS::OnMessageBoxResult,
                 base::Unretained(this)));
}

void It2MeConfirmationDialogChromeOS::OnMessageBoxResult(
    MessageBox::Result result) {
  std::move(callback_).Run(result == MessageBox::OK ? Result::OK
                                                    : Result::CANCEL);
}

std::unique_ptr<It2MeConfirmationDialog>
It2MeConfirmationDialogFactory::Create() {
  return std::make_unique<It2MeConfirmationDialogChromeOS>();
}

}  // namespace remoting
