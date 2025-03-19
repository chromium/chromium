// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_confirmation_dialog_chromeos.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/message_formatter.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/session_manager/session_manager_types.h"
#include "remoting/base/string_resources.h"
#include "remoting/host/chromeos/features.h"
#include "remoting/host/chromeos/message_box.h"
#include "remoting/host/it2me/it2me_confirmation_dialog.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/views/window/dialog_delegate.h"

namespace remoting {

namespace {

using session_manager::SessionState;

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

std::u16string GetAutoAcceptMessage(const std::string& remote_user_email,
                                    base::TimeDelta time_left) {
  std::u16string auto_accept_message =
      base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(
              IDS_SHARE_CONFIRM_DIALOG_MESSAGE_ADMIN_INITIATED_CRD_UNATTENDED),
          remote_user_email);
  auto_accept_message.append(u"\n\n");
  auto_accept_message.append(l10n_util::GetPluralStringFUTF16(
      IDS_CRD_AUTO_ACCEPT_COUNTDOWN, time_left.InSeconds()));

  return auto_accept_message;
}

std::u16string GetTitle() {
  return l10n_util::GetStringUTF16(IDS_MODE_IT2ME);
}

std::u16string GetConfirmButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_CONFIRM);
}

std::u16string GetAutoAcceptConfirmButtonLabel() {
  return l10n_util::GetStringUTF16(
      IDS_SHARE_CONFIRM_DIALOG_CONFIRM_CRD_UNATTENDED);
}

std::u16string GetDeclineButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_DECLINE);
}

ash::SessionControllerImpl* session_controller() {
  return ash::Shell::Get()->session_controller();
}

ash::ShellWindowId GetParentContainerId() {
  switch (session_controller()->GetSessionState()) {
    case SessionState::LOCKED:
    case SessionState::LOGIN_PRIMARY:
    case SessionState::LOGIN_SECONDARY:
    case SessionState::LOGGED_IN_NOT_ACTIVE:
      return ash::kShellWindowId_LockSystemModalContainer;

    case SessionState::ACTIVE:
      return ash::kShellWindowId_SystemModalContainer;

    case SessionState::OOBE:
    case SessionState::RMA:
    case SessionState::UNKNOWN:
      NOTREACHED() << "CRD is not supported for the session state:"
                   << static_cast<int>(session_controller()->GetSessionState());
  }
}

gfx::NativeView GetParentContainer() {
  return ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                  GetParentContainerId());
}

class CountdownTimer {
 public:
  using CountdownCallback = base::RepeatingCallback<void(base::TimeDelta)>;

  explicit CountdownTimer(base::TimeDelta time,
                          CountdownCallback countdown_callback,
                          base::OnceClosure completion_callback)
      : time_left_(time),
        countdown_callback_(std::move(countdown_callback)),
        completion_callback_(std::move(completion_callback)) {}

  CountdownTimer(const CountdownTimer&) = delete;
  CountdownTimer& operator=(const CountdownTimer&) = delete;

  void Start() {
    countdown_timer_.Start(FROM_HERE, /*delay=*/base::Seconds(1),
                           base::BindRepeating(&CountdownTimer::RunEverySecond,
                                               weak_factory_.GetWeakPtr()));
  }

 private:
  void RunEverySecond() {
    time_left_ = time_left_ - base::Seconds(1);

    if (time_left_ < base::Seconds(1)) {
      countdown_timer_.Stop();
      std::move(completion_callback_).Run();
      return;
    }

    countdown_callback_.Run(time_left_);
  }

  base::RepeatingTimer countdown_timer_;

  base::TimeDelta time_left_;
  CountdownCallback countdown_callback_;
  base::OnceClosure completion_callback_;

  base::WeakPtrFactory<CountdownTimer> weak_factory_{this};
};

class MessageLabelProvider {
 public:
  MessageLabelProvider(const std::string& remote_user_email,
                       It2MeConfirmationDialog::DialogStyle style,
                       base::TimeDelta auto_accept_timeout)
      : remote_user_email_(remote_user_email),
        style_(style),
        auto_accept_timeout_(auto_accept_timeout) {}
  ~MessageLabelProvider() = default;

  std::u16string GetMessageLabel() const {
    if (!auto_accept_timeout_.is_zero()) {
      return GetAutoAcceptMessage(remote_user_email_,
                                  /*time_left=*/auto_accept_timeout_);
    }

    return FormatMessage(remote_user_email_, style_);
  }

  std::u16string GetAutoAcceptMessageLabel(base::TimeDelta time_left) const {
    return GetAutoAcceptMessage(remote_user_email_, time_left);
  }

 private:
  const std::string remote_user_email_;
  const It2MeConfirmationDialog::DialogStyle style_;
  const base::TimeDelta auto_accept_timeout_;
};

}  // namespace

class It2MeConfirmationDialogChromeOS::Core : public ash::SessionObserver {
 public:
  explicit Core(const std::u16string& title_label,
                const MessageLabelProvider message_label_provider_,
                const std::u16string& ok_label,
                const std::u16string& cancel_label,
                const std::optional<ui::ImageModel> icon,
                const base::TimeDelta auto_accept_timeout,
                ResultCallback callback);
  ~Core() override;

  void ShowConfirmationDialog();

  views::DialogDelegate& GetDialogDelegate();

 private:
  void OnConfirmationDialogResult(MessageBox::Result result);

  // Implements ash::SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  bool HasAutoAcceptTimeout();
  void StartAutoAcceptCountDown();
  void UpdateCountdownInDialogUI(base::TimeDelta count);
  void OnCountdownComplete();

  std::unique_ptr<CountdownTimer> countdown_timer_;

  ResultCallback callback_;
  std::unique_ptr<MessageBox> message_box_;

  const MessageLabelProvider message_label_provider_;
  const base::TimeDelta auto_accept_timeout_;

  base::WeakPtrFactory<It2MeConfirmationDialogChromeOS::Core> weak_factory_{
      this};
};

It2MeConfirmationDialogChromeOS::Core::Core(
    const std::u16string& title_label,
    const MessageLabelProvider message_label_provider,
    const std::u16string& ok_label,
    const std::u16string& cancel_label,
    const std::optional<ui::ImageModel> icon,
    const base::TimeDelta auto_accept_timeout,
    ResultCallback callback)
    : message_label_provider_(message_label_provider),
      auto_accept_timeout_(auto_accept_timeout) {
  session_controller()->AddObserver(this);
  message_box_ = std::make_unique<MessageBox>(
      /*title_label=*/title_label,
      /*message_label=*/message_label_provider_.GetMessageLabel(),
      /*ok_label=*/ok_label,
      /*cancel_label=*/cancel_label,
      /*icon=*/icon,
      /*result_callback=*/
      base::BindOnce(
          &It2MeConfirmationDialogChromeOS::Core::OnConfirmationDialogResult,
          weak_factory_.GetWeakPtr()));
  callback_ = std::move(callback);
}

It2MeConfirmationDialogChromeOS::Core::~Core() {
  session_controller()->RemoveObserver(this);
}

void It2MeConfirmationDialogChromeOS::Core::ShowConfirmationDialog() {
  // Ensure the message box remains visible when the user logs in/out.
  message_box_->ShowInParentContainer(GetParentContainer());

  if (HasAutoAcceptTimeout()) {
    StartAutoAcceptCountDown();
  }
}

void It2MeConfirmationDialogChromeOS::Core::OnConfirmationDialogResult(
    MessageBox::Result result) {
  std::move(callback_).Run(result == MessageBox::Result::OK ? Result::OK
                                                            : Result::CANCEL);
}

void It2MeConfirmationDialogChromeOS::Core::OnSessionStateChanged(
    session_manager::SessionState state) {
  message_box_->ChangeParentContainer(GetParentContainer());
}

views::DialogDelegate&
It2MeConfirmationDialogChromeOS::Core::GetDialogDelegate() {
  return message_box_->GetDialogDelegate();
}

bool It2MeConfirmationDialogChromeOS::Core::HasAutoAcceptTimeout() {
  return !auto_accept_timeout_.is_zero();
}

void It2MeConfirmationDialogChromeOS::Core::StartAutoAcceptCountDown() {
  countdown_timer_ = std::make_unique<CountdownTimer>(
      /*time=*/auto_accept_timeout_,
      /*countdown_callback=*/
      base::BindRepeating(
          &It2MeConfirmationDialogChromeOS::Core::UpdateCountdownInDialogUI,
          weak_factory_.GetWeakPtr()),
      /*completion_callback=*/
      base::BindOnce(
          &It2MeConfirmationDialogChromeOS::Core::OnCountdownComplete,
          weak_factory_.GetWeakPtr()));
  countdown_timer_->Start();
}

void It2MeConfirmationDialogChromeOS::Core::UpdateCountdownInDialogUI(
    base::TimeDelta time_left) {
  message_box_->SetMessageLabel(
      message_label_provider_.GetAutoAcceptMessageLabel(time_left));
}

void It2MeConfirmationDialogChromeOS::Core::OnCountdownComplete() {
  weak_factory_.InvalidateWeakPtrs();
  countdown_timer_.reset();
  // `callback_` will lead to destruction of `this`, so no member fields should
  // be accessed after this point.
  std::move(callback_).Run(Result::OK);
}

It2MeConfirmationDialogChromeOS::It2MeConfirmationDialogChromeOS(
    DialogStyle style,
    base::TimeDelta auto_accept_timeout)
    : style_(style), auto_accept_timeout_(auto_accept_timeout) {}

It2MeConfirmationDialogChromeOS::~It2MeConfirmationDialogChromeOS() {
  message_center::MessageCenter::Get()->RemoveNotification(
      kConfirmationNotificationId,
      /*by_user=*/false);
}

void It2MeConfirmationDialogChromeOS::Show(const std::string& remote_user_email,
                                           ResultCallback callback) {
  DCHECK(!remote_user_email.empty());
  callback_ = std::move(callback);

  if (base::FeatureList::IsEnabled(
          remoting::features::kEnableCrdSharedSessionToUnattendedDevice)) {
    core_ = std::make_unique<It2MeConfirmationDialogChromeOS::Core>(
        /*title_label=*/GetTitle(),
        /*message_label_provider=*/
        MessageLabelProvider(remote_user_email, style_, auto_accept_timeout_),
        /*ok_label=*/GetShareButtonLabel(),
        /*cancel_label=*/GetDeclineButtonLabel(),
        /*icon=*/GetDialogIcon(),
        /*auto_accept_timeout=*/auto_accept_timeout_,
        /*callback=*/
        base::BindOnce(
            &It2MeConfirmationDialogChromeOS::OnConfirmationDialogResult,
            base::Unretained(this)));
    core_->ShowConfirmationDialog();
  } else {
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

void It2MeConfirmationDialogChromeOS::OnConfirmationDialogResult(
    Result result) {
  core_.reset();
  std::move(callback_).Run(result);
}

const gfx::VectorIcon& It2MeConfirmationDialogChromeOS::GetIcon() const {
  switch (style_) {
    case It2MeConfirmationDialog::DialogStyle::kConsumer:
      return gfx::VectorIcon::EmptyIcon();
    case It2MeConfirmationDialog::DialogStyle::kEnterprise:
      return chromeos::kEnterpriseIcon;
  }
}

const ui::ImageModel It2MeConfirmationDialogChromeOS::GetDialogIcon() const {
  return ui::ImageModel::FromVectorIcon(GetIcon());
}

std::u16string It2MeConfirmationDialogChromeOS::GetShareButtonLabel() const {
  if (!auto_accept_timeout_.is_zero()) {
    return GetAutoAcceptConfirmButtonLabel();
  }

  return GetConfirmButtonLabel();
}

views::DialogDelegate&
It2MeConfirmationDialogChromeOS::GetDialogDelegateForTest() {
  CHECK_IS_TEST();
  return core_->GetDialogDelegate();
}

std::unique_ptr<It2MeConfirmationDialog>
It2MeConfirmationDialogFactory::Create() {
  return std::make_unique<It2MeConfirmationDialogChromeOS>(
      dialog_style_, auto_accept_timeout_);
}

}  // namespace remoting
