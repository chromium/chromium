// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_confirmation_dialog_chromeos.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/i18n/message_formatter.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "components/user_manager/user_type.h"
#include "remoting/base/string_resources.h"
#include "remoting/host/chromeos/features.h"
#include "remoting/host/it2me/it2me_confirmation_dialog.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

namespace remoting {

namespace {

using base::test::TestFuture;
using DialogStyle = It2MeConfirmationDialog::DialogStyle;
using remoting::features::kEnableCrdSharedSessionToUnattendedDevice;

constexpr char kTestingRemoteEmail[] = "remote@gmail.com";
constexpr char kModalDialogClassName[] = "MessageBoxView";
constexpr char kModalDialogContentClassName[] = "Label";
constexpr char kTestUserEmail[] = "user@test.com";
const std::vector<std::string> kMessageBoxLabelHierarchy = {
    "DialogClientView", "MessageBoxView", "ScrollView", "ScrollView::Viewport",
    "BoxLayoutView"};

// The lock and login screen use the same container for showing the modal
// dialog.
ash::ShellWindowId kLockScreen = ash::kShellWindowId_LockSystemModalContainer;
ash::ShellWindowId kLoginScreen = ash::kShellWindowId_LockSystemModalContainer;
ash::ShellWindowId kUserSessionScreen =
    ash::kShellWindowId_SystemModalContainer;

void LoadUiTestResources() {
  base::FilePath ui_test_pak_path;
  ASSERT_TRUE(base::PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
  ui::ResourceBundle::CleanupSharedInstance();
  ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);
  ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources("en-US");
}

gfx::NativeView GetParentContainer(ash::ShellWindowId container) {
  return ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                  container);
}

gfx::NativeView GetNativeViewByClassName(gfx::NativeView container,
                                         const std::string& name) {
  for (auto& child : container->children()) {
    if (child->GetName() == name) {
      return child;
    }
  }

  return nullptr;
}

gfx::NativeView GetModalDialogInsideParent(ash::ShellWindowId parent) {
  return GetNativeViewByClassName(GetParentContainer(parent),
                                  kModalDialogClassName);
}

bool DialogVisibleInParentContainer(ash::ShellWindowId parent) {
  auto dialog = GetModalDialogInsideParent(parent);
  return dialog != nullptr && dialog->IsVisible();
}

views::View* FindChildViewByClassName(views::View* parent,
                                      const std::string& name) {
  if (!parent) {
    return nullptr;
  }

  for (const auto& child : parent->children()) {
    if (child->GetClassName() == name) {
      return child;
    }
  }

  return nullptr;
}

views::View* FindChildViewRecursivelyByClassName(
    views::View* container,
    const std::vector<std::string>& hierarchy) {
  views::View* parent = container;

  for (const auto& className : hierarchy) {
    views::View* child = FindChildViewByClassName(parent, className);

    if (child == nullptr) {
      return nullptr;
    }

    parent = child;
  }

  return parent;
}

}  // namespace

class It2MeConfirmationDialogChromeOSTest
    : public ash::AshTestBase,
      public testing::WithParamInterface<DialogStyle> {
 public:
  It2MeConfirmationDialogChromeOSTest()
      : ash::AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ash::AshTestBase::SetUp();

    // It2MeConfirmationDialogChromeOS requires the UI resource bundle.
    LoadUiTestResources();
  }

  void TearDown() override {
    dialog_.reset();
    ash::AshTestBase::TearDown();
  }

  void EnableFeature(const base::Feature& feature) {
    feature_list_.Reset();
    feature_list_.InitAndEnableFeature(feature);
  }

  void DisableFeature(const base::Feature& feature) {
    feature_list_.Reset();
    feature_list_.InitAndDisableFeature(feature);
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

  std::u16string GetAutoAcceptConfirmButtonLabel() {
    return l10n_util::GetStringUTF16(
        IDS_SHARE_CONFIRM_DIALOG_CONFIRM_CRD_UNATTENDED);
  }

  It2MeConfirmationDialog::ResultCallback DoNothingCallback() {
    return It2MeConfirmationDialog::ResultCallback();
  }

  void CreateAndShowDialog(const std::string& remote_user_email,
                           It2MeConfirmationDialog::ResultCallback callback) {
    CreateAndShowDialog(remote_user_email, std::move(callback),
                        base::TimeDelta());
  }

  void CreateAndShowDialog(const std::string& remote_user_email,
                           It2MeConfirmationDialog::ResultCallback callback,
                           base::TimeDelta auto_accept_timeout) {
    dialog_ = std::make_unique<It2MeConfirmationDialogChromeOS>(
        /*style=*/GetParam(), auto_accept_timeout);
    dialog_->Show(remote_user_email, std::move(callback));
  }

  views::DialogDelegate& GetDialogDelegate() {
    return dialog_->GetDialogDelegateForTest();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<It2MeConfirmationDialogChromeOS> dialog_;
};

class It2MeConfirmationDialogChromeOSTestWithCrdUnattendedDisabled
    : public It2MeConfirmationDialogChromeOSTest {
 public:
  void SetUp() override {
    DisableFeature(kEnableCrdSharedSessionToUnattendedDevice);
    It2MeConfirmationDialogChromeOSTest::SetUp();
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
    auto button_iter = std::ranges::find(array, button_title,
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
};

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedDisabled,
       NotificationShouldHaveDesiredText) {
  CreateAndShowDialog(kTestingRemoteEmail, DoNothingCallback());

  const message_center::Notification* notification = GetFirstNotification();
  ASSERT_NE(notification, nullptr);
  ASSERT_GT(notification->message().size(), 0llu);
  EXPECT_EQ(message_center().NotificationCount(), 1llu);
  EXPECT_EQ(notification->message(),
            FormatMessage(kTestingRemoteEmail, GetParam()));
}

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedDisabled,
       NotificationShouldBePersistent) {
  CreateAndShowDialog(kTestingRemoteEmail, DoNothingCallback());

  const message_center::Notification* notification = GetFirstNotification();
  ASSERT_NE(notification, nullptr);
  ASSERT_EQ(notification->priority(),
            message_center::NotificationPriority::SYSTEM_PRIORITY);
}

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedDisabled,
       NotificationShouldBeShownInDoNotDisturbMode) {
  CreateAndShowDialog(kTestingRemoteEmail, DoNothingCallback());

  const message_center::Notification* notification = GetFirstNotification();
  ASSERT_NE(notification, nullptr);

  // Notifications are shown in do-not-disturb mode only if their warning level
  // is `CRITICAL_WARNING`. See NotificationList::PushNotification().
  ASSERT_EQ(notification->system_notification_warning_level(),
            message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
}

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedDisabled,
       NotificationShouldHaveConfirmAndCancelButton) {
  CreateAndShowDialog(kTestingRemoteEmail, DoNothingCallback());

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

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedDisabled,
       NotificationShouldBeRemovedAndReturnCancelAfterUserCancels) {
  TestFuture<It2MeConfirmationDialog::Result> result_future;
  CreateAndShowDialog(kTestingRemoteEmail, result_future.GetCallback());
  ClickOnFirstNotification(
      l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_DECLINE));

  EXPECT_EQ(GetVisibleNotificationsCount(), 0);
  EXPECT_EQ(result_future.Get(), It2MeConfirmationDialog::Result::CANCEL);
}

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedDisabled,
       NotificationShouldBeRemovedAndReturnOkAfterUserConfirms) {
  TestFuture<It2MeConfirmationDialog::Result> result_future;
  CreateAndShowDialog(kTestingRemoteEmail, result_future.GetCallback());
  ClickOnFirstNotification(
      l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_CONFIRM));

  EXPECT_EQ(GetVisibleNotificationsCount(), 0);
  EXPECT_EQ(result_future.Get(), It2MeConfirmationDialog::Result::OK);
}

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedDisabled,
       ShouldNotAutoAcceptNotification) {
  TestFuture<It2MeConfirmationDialog::Result> result_future;
  CreateAndShowDialog(kTestingRemoteEmail, result_future.GetCallback(),
                      base::Seconds(10));

  EXPECT_EQ(GetVisibleNotificationsCount(), 1);

  task_environment()->FastForwardBy(base::Seconds(11));

  EXPECT_EQ(GetVisibleNotificationsCount(), 1);
}

class It2MeConfirmationDialogChromeOSTestWithCrdUnattendedEnabled
    : public It2MeConfirmationDialogChromeOSTest {
 public:
  void SetUp() override {
    EnableFeature(kEnableCrdSharedSessionToUnattendedDevice);
    It2MeConfirmationDialogChromeOSTest::SetUp();
  }

  void SimulateDeviceOnLockScreen() {
    BlockUserSession(UserSessionBlockReason::BLOCKED_BY_LOCK_SCREEN);
  }

  void SimulateDeviceOnLoginScreen() {
    BlockUserSession(UserSessionBlockReason::BLOCKED_BY_LOGIN_SCREEN);
  }

  const std::u16string GetDialogTitle() {
    return GetDialogDelegate().GetWindowTitle();
  }

  const std::u16string GetDialogMessage() {
    views::View* parent = GetDialogDelegate().GetBubbleFrameView();

    views::View* label_view_parent =
        FindChildViewRecursivelyByClassName(parent, kMessageBoxLabelHierarchy);

    views::View* label_view = FindChildViewByClassName(
        label_view_parent, kModalDialogContentClassName);

    if (label_view) {
      const auto* content = static_cast<views::Label*>(label_view);
      return static_cast<std::u16string>(content->GetText());
    }

    return u"dialog-not-found";
  }

  void AcceptDialog() { GetDialogDelegate().AcceptDialog(); }
  void DeclineDialog() { GetDialogDelegate().CancelDialog(); }

  const std::u16string GetAcceptButtonLabel() {
    return GetDialogDelegate().GetDialogButtonLabel(
        ui::mojom::DialogButton::kOk);
  }
};

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedEnabled,
       ShowDialogOnLoginScreenIfNoUserIsSignedIn) {
  SimulateDeviceOnLoginScreen();

  CreateAndShowDialog(kTestingRemoteEmail, DoNothingCallback());

  ASSERT_TRUE(DialogVisibleInParentContainer(kLoginScreen));
}

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedEnabled,
       ShowDialogOnLockScreenIfTheDeviceIsLocked) {
  SimulateDeviceOnLockScreen();

  CreateAndShowDialog(kTestingRemoteEmail, DoNothingCallback());

  ASSERT_TRUE(DialogVisibleInParentContainer(kLockScreen));
}

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedEnabled,
       ShowDialogOnUserSessionScreenDuringActiveUserSession) {
  SimulateUserLogin({kTestUserEmail});

  CreateAndShowDialog(kTestingRemoteEmail, DoNothingCallback());

  ASSERT_TRUE(DialogVisibleInParentContainer(kUserSessionScreen));
}

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedEnabled,
       DialogShouldMoveToUserSessionScreenOnUserLogin) {
  SimulateDeviceOnLoginScreen();

  CreateAndShowDialog(kTestingRemoteEmail, DoNothingCallback());

  ASSERT_TRUE(DialogVisibleInParentContainer(kLoginScreen));
  ASSERT_FALSE(DialogVisibleInParentContainer(kUserSessionScreen));

  SimulateUserLogin({kTestUserEmail});

  ASSERT_FALSE(DialogVisibleInParentContainer(kLoginScreen));
  ASSERT_TRUE(DialogVisibleInParentContainer(kUserSessionScreen));
}

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedEnabled,
       DialogShouldMoveToLoginScreenOnUserLogout) {
  SimulateUserLogin({kTestUserEmail});

  CreateAndShowDialog(kTestingRemoteEmail, DoNothingCallback());

  ASSERT_TRUE(DialogVisibleInParentContainer(kUserSessionScreen));
  ASSERT_FALSE(DialogVisibleInParentContainer(kLoginScreen));

  ClearLogin();

  ASSERT_FALSE(DialogVisibleInParentContainer(kUserSessionScreen));
  ASSERT_TRUE(DialogVisibleInParentContainer(kLoginScreen));
}

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedEnabled,
       DialogShouldMoveToUserSessionScreenOnSessionUnlock) {
  SimulateDeviceOnLockScreen();

  CreateAndShowDialog(kTestingRemoteEmail, DoNothingCallback());

  ASSERT_TRUE(DialogVisibleInParentContainer(kLockScreen));
  ASSERT_FALSE(DialogVisibleInParentContainer(kUserSessionScreen));

  SimulateUserLogin({kTestUserEmail});

  ASSERT_FALSE(DialogVisibleInParentContainer(kLockScreen));
  ASSERT_TRUE(DialogVisibleInParentContainer(kUserSessionScreen));
}

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedEnabled,
       DialogShouldMoveToLockSessionScreenWhenSessionIsLocked) {
  SimulateUserLogin({kTestUserEmail});

  CreateAndShowDialog(kTestingRemoteEmail, DoNothingCallback());

  ASSERT_TRUE(DialogVisibleInParentContainer(kUserSessionScreen));
  ASSERT_FALSE(DialogVisibleInParentContainer(kLockScreen));

  SimulateDeviceOnLockScreen();

  ASSERT_FALSE(DialogVisibleInParentContainer(kUserSessionScreen));
  ASSERT_TRUE(DialogVisibleInParentContainer(kLockScreen));
}

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedEnabled,
       ModalDialogShouldHaveTitle) {
  CreateAndShowDialog(kTestingRemoteEmail, DoNothingCallback());

  ASSERT_EQ(GetDialogTitle(), l10n_util::GetStringUTF16(IDS_MODE_IT2ME));
}

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedEnabled,
       ModalDialogShouldHaveFormattedMessage) {
  CreateAndShowDialog(kTestingRemoteEmail, DoNothingCallback());

  ASSERT_EQ(GetDialogMessage(),
            FormatMessage(kTestingRemoteEmail, /*style=*/GetParam()));
}

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedEnabled,
       ModalDialogShouldCorrectTextForAutoAcceptSessions) {
  CreateAndShowDialog(kTestingRemoteEmail, DoNothingCallback(),
                      /*auto_accept_timeout=*/base::Seconds(30));

  ASSERT_EQ(GetDialogMessage(),
            GetAutoAcceptMessage(kTestingRemoteEmail,
                                 /*time_left=*/base::Seconds(30)));
}

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedEnabled,
       ModalDialogShouldCountdownTextForAutoAcceptSessions) {
  CreateAndShowDialog(kTestingRemoteEmail, DoNothingCallback(),
                      /*auto_accept_timeout=*/base::Seconds(30));

  task_environment()->FastForwardBy(base::Seconds(29));

  ASSERT_EQ(GetDialogMessage(),
            GetAutoAcceptMessage(kTestingRemoteEmail,
                                 /*time_left=*/base::Seconds(1)));
}

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedEnabled,
       ModalDialogAcceptButtonShouldHaveAutoAcceptButtonLabel) {
  CreateAndShowDialog(kTestingRemoteEmail, DoNothingCallback(),
                      /*auto_accept_timeout=*/base::Seconds(30));

  ASSERT_EQ(GetAcceptButtonLabel(), GetAutoAcceptConfirmButtonLabel());
}

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedEnabled,
       TestDialogResultWhenDialogIsAccepted) {
  TestFuture<It2MeConfirmationDialog::Result> result_future;
  CreateAndShowDialog(kTestingRemoteEmail, result_future.GetCallback());

  AcceptDialog();

  ASSERT_EQ(result_future.Get(), It2MeConfirmationDialog::Result::OK);
}

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedEnabled,
       TestDialogResultWhenDialogIsDeclined) {
  TestFuture<It2MeConfirmationDialog::Result> result_future;
  CreateAndShowDialog(kTestingRemoteEmail, result_future.GetCallback());

  DeclineDialog();

  ASSERT_EQ(result_future.Get(), It2MeConfirmationDialog::Result::CANCEL);
}

TEST_P(It2MeConfirmationDialogChromeOSTestWithCrdUnattendedEnabled,
       ShouldAutoAcceptDialogIfAutoAcceptTimeoutParamIsPresent) {
  TestFuture<It2MeConfirmationDialog::Result> result_future;
  CreateAndShowDialog(kTestingRemoteEmail, result_future.GetCallback(),
                      base::Seconds(10));

  ASSERT_TRUE(DialogVisibleInParentContainer(kUserSessionScreen));

  task_environment()->FastForwardBy(base::Seconds(10));

  ASSERT_FALSE(DialogVisibleInParentContainer(kUserSessionScreen));
  ASSERT_EQ(result_future.Get(), It2MeConfirmationDialog::Result::OK);
}

INSTANTIATE_TEST_SUITE_P(
    EnterpriseDialog,
    It2MeConfirmationDialogChromeOSTestWithCrdUnattendedDisabled,
    testing::Values(DialogStyle::kEnterprise));

INSTANTIATE_TEST_SUITE_P(
    ConsumerDialog,
    It2MeConfirmationDialogChromeOSTestWithCrdUnattendedDisabled,
    testing::Values(DialogStyle::kConsumer));

INSTANTIATE_TEST_SUITE_P(
    EnterpriseDialog,
    It2MeConfirmationDialogChromeOSTestWithCrdUnattendedEnabled,
    testing::Values(DialogStyle::kEnterprise));

INSTANTIATE_TEST_SUITE_P(
    ConsumerDialog,
    It2MeConfirmationDialogChromeOSTestWithCrdUnattendedEnabled,
    testing::Values(DialogStyle::kConsumer));

}  // namespace remoting
