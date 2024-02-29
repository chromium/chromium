// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_confirmation_dialog.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

#include "base/i18n/message_formatter.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "remoting/base/string_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/lock_screen/fake_lock_screen_controller.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace remoting {

namespace {

using base::test::TestFuture;
using DialogStyle = It2MeConfirmationDialog::DialogStyle;
constexpr char kTestingRemoteEmail[] = "remote@gmail.com";

}  // namespace

class It2MeConfirmationDialogChromeOSTest
    : public testing::TestWithParam<DialogStyle> {
 public:
  void SetUp() override {
    dialog = CreateDialog(GetParam());
    message_center::MessageCenter::Initialize(
        std::make_unique<message_center::FakeLockScreenController>());
  }

  void TearDown() override {
    dialog.reset();
    message_center::MessageCenter::Shutdown();
  }

  message_center::MessageCenter& message_center() const {
    return *message_center::MessageCenter::Get();
  }

  int GetVisibleNotificationsCount() {
    return message_center().GetVisibleNotifications().size();
  }

  const message_center::Notification* GetFirstNotification() {
    const message_center::NotificationList::Notifications& notifications =
        message_center().GetVisibleNotifications();
    if (notifications.size() == 0) {
      return nullptr;
    }

    return *notifications.cbegin();
  }

  int FindIndex(const std::vector<message_center::ButtonInfo>& array,
                const std::u16string& button_title) {
    auto button_iter = base::ranges::find(array, button_title,
                                          &message_center::ButtonInfo::title);
    if (button_iter == array.cend()) {
      return -1;
    }

    return std::distance(array.cbegin(), button_iter);
  }

  int FindButtonIndex(const message_center::Notification& notification,
                      const std::u16string& button_title) {
    const std::vector<message_center::ButtonInfo>& buttons =
        notification.buttons();

    return FindIndex(buttons, button_title);
  }

  void ClickOnFirstNotification(const std::u16string& button_title) {
    const message_center::Notification* notification = GetFirstNotification();
    ASSERT_NE(notification, nullptr);
    const int button_index = FindButtonIndex(*notification, button_title);
    ASSERT_GE(button_index, 0);
    notification->delegate()->Click(button_index, std::nullopt);
  }

  std::u16string FormatMessage(const std::string& remote_user_email,
                               DialogStyle style) {
    int message_id = (style == DialogStyle::kEnterprise
                          ? IDS_SHARE_CONFIRM_DIALOG_MESSAGE_ADMIN_INITIATED
                          : IDS_SHARE_CONFIRM_DIALOG_MESSAGE_WITH_USERNAME);

    return base::i18n::MessageFormatter::FormatWithNumberedArgs(
        l10n_util::GetStringUTF16(message_id),
        base::UTF8ToUTF16(remote_user_email),
        l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_DECLINE),
        l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_CONFIRM));
  }

  It2MeConfirmationDialog::ResultCallback DoNothingCallback() {
    return It2MeConfirmationDialog::ResultCallback();
  }

 protected:
  std::unique_ptr<It2MeConfirmationDialog> dialog;

 private:
  std::unique_ptr<It2MeConfirmationDialog> CreateDialog(
      DialogStyle dialog_style) {
    It2MeConfirmationDialogFactory dialog_factory{dialog_style};
    return dialog_factory.Create();
  }
  base::test::SingleThreadTaskEnvironment environment_;
};

TEST_P(It2MeConfirmationDialogChromeOSTest, NotificationShouldHaveDesiredText) {
  dialog->Show(kTestingRemoteEmail, DoNothingCallback());

  const message_center::Notification* notification = GetFirstNotification();
  ASSERT_NE(notification, nullptr);
  ASSERT_GT(notification->message().size(), 0llu);
  EXPECT_EQ(message_center().NotificationCount(), 1llu);
  EXPECT_EQ(notification->message(),
            FormatMessage(kTestingRemoteEmail, GetParam()));
}

TEST_P(It2MeConfirmationDialogChromeOSTest, NotificationShouldBePersistent) {
  dialog->Show(kTestingRemoteEmail, DoNothingCallback());

  const message_center::Notification* notification = GetFirstNotification();
  ASSERT_NE(notification, nullptr);
  ASSERT_EQ(notification->priority(),
            message_center::NotificationPriority::SYSTEM_PRIORITY);
}

TEST_P(It2MeConfirmationDialogChromeOSTest,
       NotificationShouldBeShownInDoNotDisturbMode) {
  dialog->Show(kTestingRemoteEmail, DoNothingCallback());

  const message_center::Notification* notification = GetFirstNotification();
  ASSERT_NE(notification, nullptr);

  // Notifications are shown in do-not-disturb mode only if their warning level
  // is `CRITICAL_WARNING`. See NotificationList::PushNotification().
  ASSERT_EQ(notification->system_notification_warning_level(),
            message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
}

TEST_P(It2MeConfirmationDialogChromeOSTest,
       NotificationShouldHaveConfirmAndCancelButton) {
  dialog->Show(kTestingRemoteEmail, DoNothingCallback());

  const message_center::Notification* notification = GetFirstNotification();
  ASSERT_NE(notification, nullptr);
  ASSERT_GE(FindButtonIndex(
                *notification,
                l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_DECLINE)),
            0);  // Cancel button
  ASSERT_GE(FindButtonIndex(
                *notification,
                l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_CONFIRM)),
            0);  // Confirm button
}

TEST_P(It2MeConfirmationDialogChromeOSTest,
       NotificationShouldBeRemovedAndReturnCancelAfterUserCancels) {
  TestFuture<It2MeConfirmationDialog::Result> result_future;
  dialog->Show(kTestingRemoteEmail, result_future.GetCallback());
  ClickOnFirstNotification(
      l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_DECLINE));

  EXPECT_EQ(GetVisibleNotificationsCount(), 0);
  EXPECT_EQ(result_future.Get(), It2MeConfirmationDialog::Result::CANCEL);
}

TEST_P(It2MeConfirmationDialogChromeOSTest,
       NotificationShouldBeRemovedAndReturnOkAfterUserConfirms) {
  TestFuture<It2MeConfirmationDialog::Result> result_future;
  dialog->Show(kTestingRemoteEmail, result_future.GetCallback());
  ClickOnFirstNotification(
      l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_CONFIRM));

  EXPECT_EQ(GetVisibleNotificationsCount(), 0);
  EXPECT_EQ(result_future.Get(), It2MeConfirmationDialog::Result::OK);
}

INSTANTIATE_TEST_SUITE_P(EnterpriseDialog,
                         It2MeConfirmationDialogChromeOSTest,
                         testing::Values(DialogStyle::kEnterprise));

INSTANTIATE_TEST_SUITE_P(ConsumerDialog,
                         It2MeConfirmationDialogChromeOSTest,
                         testing::Values(DialogStyle::kConsumer));

}  // namespace remoting
