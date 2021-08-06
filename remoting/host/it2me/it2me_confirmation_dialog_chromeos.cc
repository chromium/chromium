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
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "remoting/base/string_resources.h"
#include "remoting/host/chromeos/message_box.h"
#include "remoting/host/it2me/it2me_confirmation_dialog.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/paint_vector_icon.h"

namespace remoting {

namespace {

std::u16string FormatMessage(const std::string& remote_user_email,
                             It2MeConfirmationDialog::DialogStyle style) {
  int message_id = (style == It2MeConfirmationDialog::DialogStyle::kEnterprise
                        ? IDS_SHARE_CONFIRM_DIALOG_MESSAGE_ADMIN_INITIATED
                        : IDS_SHARE_CONFIRM_DIALOG_MESSAGE_WITH_USERNAME);

  return base::i18n::MessageFormatter::FormatWithNumberedArgs(
      l10n_util::GetStringUTF16(message_id),
      base::UTF8ToUTF16(remote_user_email),
      l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_DECLINE),
      l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_CONFIRM));
}

}  // namespace

class It2MeConfirmationDialogChromeOS : public It2MeConfirmationDialog {
 public:
  explicit It2MeConfirmationDialogChromeOS(DialogStyle style);
  ~It2MeConfirmationDialogChromeOS() override;

  // It2MeConfirmationDialog implementation.
  void Show(const std::string& remote_user_email,
            ResultCallback callback) override;

 private:
  // Handles result from |message_box_|.
  void OnMessageBoxResult(MessageBox::Result result);

  std::unique_ptr<MessageBox> message_box_;
  ResultCallback callback_;

  DialogStyle style_;

  DISALLOW_COPY_AND_ASSIGN(It2MeConfirmationDialogChromeOS);
};

It2MeConfirmationDialogChromeOS::It2MeConfirmationDialogChromeOS(
    DialogStyle style)
    : style_(style) {}

It2MeConfirmationDialogChromeOS::~It2MeConfirmationDialogChromeOS() = default;

void It2MeConfirmationDialogChromeOS::Show(const std::string& remote_user_email,
                                           ResultCallback callback) {
  DCHECK(!remote_user_email.empty());
  callback_ = std::move(callback);

  message_box_ = std::make_unique<MessageBox>(
      l10n_util::GetStringUTF16(IDS_MODE_IT2ME),
      FormatMessage(remote_user_email, style_),
      l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_CONFIRM),
      l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_DECLINE),
      base::BindOnce(&It2MeConfirmationDialogChromeOS::OnMessageBoxResult,
                     base::Unretained(this)));

  if (style_ == DialogStyle::kEnterprise) {
    message_box_->SetIcon(gfx::CreateVectorIcon(chromeos::kEnterpriseIcon, 20,
                                                gfx::kGoogleGrey800));
    message_box_->SetDefaultButton(ui::DialogButton::DIALOG_BUTTON_NONE);
  }

  message_box_->Show();
}

void It2MeConfirmationDialogChromeOS::OnMessageBoxResult(
    MessageBox::Result result) {
  std::move(callback_).Run(result == MessageBox::OK ? Result::OK
                                                    : Result::CANCEL);
}

std::unique_ptr<It2MeConfirmationDialog>
It2MeConfirmationDialogFactory::Create() {
  return std::make_unique<It2MeConfirmationDialogChromeOS>(dialog_style_);
}

}  // namespace remoting
