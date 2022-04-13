// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_confirmation_dialog.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>

#include "base/i18n/message_formatter.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "remoting/base/string_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/lock_screen/fake_lock_screen_controller.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace remoting {

namespace {
using base::test::TestFuture;
constexpr char kTestingRemoteEmail[] = "remote@gmail.com";
}  // namespace

class It2MeConfirmationDialogChromeOSTest : public testing::Test {
 public:
  void SetUp() override {
    message_center::MessageCenter::Initialize(
        std::make_unique<message_center::FakeLockScreenController>());
    dialog = CreateEnterpriseDialog();
  }

  void TearDown() override {
    message_center::MessageCenter::Shutdown();
    dialog.release();
  }

  message_center::MessageCenter& message_center() const {
    return *message_center::MessageCenter::Get();
  }

  std::unique_ptr<It2MeConfirmationDialog> CreateEnterpriseDialog() {
    It2MeConfirmationDialogFactory enterprise_factory{
        It2MeConfirmationDialog::DialogStyle::kEnterprise};
    return enterprise_factory.Create();
  }

  int GetVisibleNotificationsCount() {
    return message_center().GetVisibleNotifications().size();
  }

  const message_center::Notification* GetFirstNotification() {
    const message_center::NotificationList::Notifications& notifications =
        message_center().GetVisibleNotifications();
    if (notifications.size() == 0)
      return nullptr;

    return *notifications.cbegin();
  }

  int FindIndex(const std::vector<message_center::ButtonInfo>& array,
                const std::u16string& button_title) {
    auto button_iter =
        std::find_if(array.cbegin(), array.cend(),
                     [button_title](const message_center::ButtonInfo& button) {
                       return button.title == button_title;
                     });
    if (button_iter == array.cend())
      return -1;

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
    notification->delegate()->Click(button_index, absl::nullopt);
  }

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

  It2MeConfirmationDialog::ResultCallback DoNothingCallback() {
    return It2MeConfirmationDialog::ResultCallback();
  }

 protected:
  std::unique_ptr<It2MeConfirmationDialog> dialog;

 private:
  base::test::SingleThreadTaskEnvironment environment_;
};

TEST_F(It2MeConfirmationDialogChromeOSTest, NotificationShouldHaveDesiredText) {
  dialog->Show(kTestingRemoteEmail, DoNothingCallback());

  const message_center::Notification* notification = GetFirstNotification();
  ASSERT_NE(notification, nullptr);
  ASSERT_GT(notification->message().size(), 0llu);
  EXPECT_EQ(message_center().NotificationCount(), 1llu);
  EXPECT_EQ(notification->message(),
            FormatMessage(kTestingRemoteEmail,
                          It2MeConfirmationDialog::DialogStyle::kEnterprise));
}

TEST_F(It2MeConfirmationDialogChromeOSTest, NotificationShouldBePersistent) {
  dialog->Show(kTestingRemoteEmail, DoNothingCallback());

  const message_center::Notification* notification = GetFirstNotification();
  ASSERT_NE(notification, nullptr);
  ASSERT_EQ(notification->priority(),
            message_center::NotificationPriority::SYSTEM_PRIORITY);
}

TEST_F(It2MeConfirmationDialogChromeOSTest,
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

TEST_F(It2MeConfirmationDialogChromeOSTest,
       NotificationShouldBeRemovedAndReturnCancelAfterUserCancels) {
  TestFuture<It2MeConfirmationDialog::Result> result_future;
  dialog->Show(kTestingRemoteEmail, result_future.GetCallback());
  ClickOnFirstNotification(
      l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_DECLINE));

  EXPECT_EQ(GetVisibleNotificationsCount(), 0);
  EXPECT_EQ(result_future.Get(), It2MeConfirmationDialog::Result::CANCEL);
}

TEST_F(It2MeConfirmationDialogChromeOSTest,
       NotificationShouldBeRemovedAndReturnOkAfterUserConfirms) {
  TestFuture<It2MeConfirmationDialog::Result> result_future;
  dialog->Show(kTestingRemoteEmail, result_future.GetCallback());
  ClickOnFirstNotification(
      l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_CONFIRM));

  EXPECT_EQ(GetVisibleNotificationsCount(), 0);
  EXPECT_EQ(result_future.Get(), It2MeConfirmationDialog::Result::OK);
}

}  // namespace remoting
