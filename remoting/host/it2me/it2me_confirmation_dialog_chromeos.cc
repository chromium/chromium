// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_confirmation_dialog.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "remoting/base/string_resources.h"
#include "remoting/host/chromeos/features.h"
#include "remoting/host/chromeos/message_box.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace remoting {

namespace {

constexpr char kConfirmationNotificationId[] = "CRD_CONFIRMATION_NOTIFICATION";
constexpr char kConfirmationNotifierId[] = "crd.confirmation_notification";

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

std::u16string GetTitle() {
  return l10n_util::GetStringUTF16(IDS_MODE_IT2ME);
}

std::u16string GetConfirmButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_CONFIRM);
}

std::u16string GetDeclineButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_DECLINE);
}

}  // namespace

class It2MeConfirmationDialogChromeOS : public It2MeConfirmationDialog {
 public:
  explicit It2MeConfirmationDialogChromeOS(DialogStyle style);

  It2MeConfirmationDialogChromeOS(const It2MeConfirmationDialogChromeOS&) =
      delete;
  It2MeConfirmationDialogChromeOS& operator=(
      const It2MeConfirmationDialogChromeOS&) = delete;

  ~It2MeConfirmationDialogChromeOS() override;

  // It2MeConfirmationDialog implementation.
  void Show(const std::string& remote_user_email,
            ResultCallback callback) override;

 private:
  class Core;

  void ShowConfirmationNotification(const std::string& remote_user_email);
  void OnConfirmationNotificationResult(std::optional<int> button_index);

  void ShowConfirmationDialog(const std::string& remote_user_email);
  void OnConfirmationDialogResult(MessageBox::Result result);

  const gfx::VectorIcon& GetIcon() const {
    switch (style_) {
      case DialogStyle::kConsumer:
        return gfx::VectorIcon::EmptyIcon();
      case DialogStyle::kEnterprise:
        return chromeos::kEnterpriseIcon;
    }
  }

  const ui::ImageModel GetDialogIcon() const {
    return ui::ImageModel::FromVectorIcon(GetIcon());
  }

  std::unique_ptr<Core> core_;

  ResultCallback callback_;
  DialogStyle style_;
};

class It2MeConfirmationDialogChromeOS::Core {
 public:
  explicit Core(const std::u16string& title_label,
                const std::u16string& message_label,
                const std::u16string& ok_label,
                const std::u16string& cancel_label,
                const std::optional<ui::ImageModel> icon,
                ResultCallback callback);

  void ShowConfirmationDialog();

 private:
  void OnConfirmationDialogResult(MessageBox::Result result);

  ResultCallback callback_;
  std::unique_ptr<MessageBox> message_box_;
};

It2MeConfirmationDialogChromeOS::Core::Core(
    const std::u16string& title_label,
    const std::u16string& message_label,
    const std::u16string& ok_label,
    const std::u16string& cancel_label,
    const std::optional<ui::ImageModel> icon,
    ResultCallback callback) {
  message_box_ = std::make_unique<MessageBox>(
      title_label, message_label, ok_label, cancel_label, icon,
      base::BindOnce(
          &It2MeConfirmationDialogChromeOS::Core::OnConfirmationDialogResult,
          base::Unretained(this)));
  callback_ = std::move(callback);
}

void It2MeConfirmationDialogChromeOS::Core::ShowConfirmationDialog() {
  message_box_->Show();
}

void It2MeConfirmationDialogChromeOS::Core::OnConfirmationDialogResult(
    MessageBox::Result result) {
  std::move(callback_).Run(result == MessageBox::Result::OK ? Result::OK
                                                            : Result::CANCEL);
}

It2MeConfirmationDialogChromeOS::It2MeConfirmationDialogChromeOS(
    DialogStyle style)
    : style_(style) {}

It2MeConfirmationDialogChromeOS::~It2MeConfirmationDialogChromeOS() {
  message_center::MessageCenter::Get()->RemoveNotification(
      kConfirmationNotificationId,
      /*by_user=*/false);
}

void It2MeConfirmationDialogChromeOS::Show(const std::string& remote_user_email,
                                           ResultCallback callback) {
  DCHECK(!remote_user_email.empty());

  if (base::FeatureList::IsEnabled(
          remoting::features::kEnableCrdSharedSessionToUnattendedDevice)) {
    core_ = std::make_unique<It2MeConfirmationDialogChromeOS::Core>(
        /*title=*/GetTitle(),
        /*message_label=*/FormatMessage(remote_user_email, style_),
        /*ok_button_label=*/GetConfirmButtonLabel(),
        /*cancel_button_label=*/GetDeclineButtonLabel(),
        /*icon=*/GetDialogIcon(),
        /*callback=*/std::move(callback));
    core_->ShowConfirmationDialog();
  } else {
    callback_ = std::move(callback);
    ShowConfirmationNotification(remote_user_email);
  }
}

void It2MeConfirmationDialogChromeOS::ShowConfirmationNotification(
    const std::string& remote_user_email) {
  message_center::RichNotificationData data;
  data.pinned = false;

  data.buttons.emplace_back(GetDeclineButtonLabel());
  data.buttons.emplace_back(GetConfirmButtonLabel());

  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, kConfirmationNotificationId,
          GetTitle(), FormatMessage(remote_user_email, style_), u"", GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kConfirmationNotifierId,
              ash::NotificationCatalogName::kIt2MeConfirmation),
          data,
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(&It2MeConfirmationDialogChromeOS::
                                      OnConfirmationNotificationResult,
                                  base::Unretained(this))),
          GetIcon(),
          // Warning level must be set to CRITICAL_WARNING to ensure this
          // notification is always shown, even when the user enabled
          // do-not-disturb mode.
          message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);

  // Set system priority so the notification is always shown and it will never
  // time out.
  notification->SetSystemPriority();
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void It2MeConfirmationDialogChromeOS::OnConfirmationNotificationResult(
    std::optional<int> button_index) {
  if (!button_index.has_value()) {
    return;  // This happens when the user clicks the notification itself.
  }

  // Note: |by_user| must be false, otherwise the notification will not actually
  // be removed but instead it will be moved into the message center bubble
  // (because the message was pinned).
  message_center::MessageCenter::Get()->RemoveNotification(
      kConfirmationNotificationId,
      /*by_user=*/false);

  std::move(callback_).Run(*button_index == 0 ? Result::CANCEL : Result::OK);
}

std::unique_ptr<It2MeConfirmationDialog>
It2MeConfirmationDialogFactory::Create() {
  return std::make_unique<It2MeConfirmationDialogChromeOS>(dialog_style_);
}

}  // namespace remoting
