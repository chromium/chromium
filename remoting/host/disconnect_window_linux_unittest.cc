// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "remoting/base/string_resources.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/host_window.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace remoting {

namespace {

class FakeClientSessionControl : public MockClientSessionControl {
 public:
  explicit FakeClientSessionControl(std::string client_jid)
      : client_jid_(std::move(client_jid)) {
    EXPECT_CALL(*this, client_jid())
        .WillRepeatedly(testing::ReturnRef(client_jid_));
  }

  ~FakeClientSessionControl() override = default;

  base::WeakPtr<ClientSessionControl> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  std::string client_jid_;
  base::WeakPtrFactory<FakeClientSessionControl> weak_factory_{this};
};

class TestResourceBundleDelegate : public ui::ResourceBundle::Delegate {
 public:
  TestResourceBundleDelegate() = default;
  ~TestResourceBundleDelegate() override = default;

  base::FilePath GetPathForResourcePack(
      const base::FilePath& pack_path,
      ui::ResourceScaleFactor scale_factor) override {
    return base::FilePath();
  }
  gfx::Image GetImageNamed(int resource_id) override { return gfx::Image(); }
  gfx::Image GetNativeImageNamed(int resource_id) override {
    return gfx::Image();
  }
  bool HasDataResource(int resource_id) const override { return false; }
  scoped_refptr<base::RefCountedMemory> LoadDataResourceBytes(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) override {
    return nullptr;
  }
  std::optional<std::string> LoadDataResourceString(int resource_id) override {
    return std::nullopt;
  }
  bool GetRawDataResource(int resource_id,
                          ui::ResourceScaleFactor scale_factor,
                          std::string_view* value) const override {
    return false;
  }
  bool GetLocalizedString(int message_id,
                          std::u16string* value) const override {
    if (message_id == IDS_MESSAGE_SHARED) {
      *value = u"Your desktop is shared with $1.";
      return true;
    }
    if (message_id == IDS_PRODUCT_NAME) {
      *value = u"Chrome Remote Desktop";
      return true;
    }
    if (message_id == IDS_STOP_SHARING_BUTTON) {
      *value = u"Stop Sharing";
      return true;
    }
    return false;
  }
};

bool InitializeGtk() {
#if GTK_CHECK_VERSION(3, 90, 0)
  return gtk_init_check();
#else
  return gtk_init_check(nullptr, nullptr);
#endif
}

}  // namespace

class DisconnectWindowLinuxTest : public testing::Test {
 public:
  DisconnectWindowLinuxTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        resource_bundle_(&resource_delegate_),
        resource_swapper_(&resource_bundle_) {}
  ~DisconnectWindowLinuxTest() override = default;

  void TearDown() override {
    // Pump the event loop to ensure GTK widget destruction and cleanup is
    // processed.
    task_environment_.RunUntilIdle();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestResourceBundleDelegate resource_delegate_;
  ui::ResourceBundle resource_bundle_;
  ui::ResourceBundle::SharedInstanceSwapperForTesting resource_swapper_;
};

TEST_F(DisconnectWindowLinuxTest, NormalEmailDoesNotCrash) {
  if (!InitializeGtk()) {
    GTEST_SKIP() << "No display available for GTK.";
  }

  FakeClientSessionControl session_control(
      "john.smith@gmail.com/chromoting_ftl");
  std::unique_ptr<HostWindow> window = HostWindow::CreateDisconnectWindow();
  ASSERT_TRUE(window);
  window->Start(session_control.GetWeakPtr());
  EXPECT_TRUE(session_control.GetWeakPtr());

  window.reset();
  task_environment_.RunUntilIdle();
}

TEST_F(DisconnectWindowLinuxTest, LongEmailDoesNotCrash) {
  if (!InitializeGtk()) {
    GTEST_SKIP() << "No display available for GTK.";
  }

  const std::string kLongEmail =
      "it-support-desk-remote-assistance-session-verification-operators@"
      "secure-remote-support-verification-and-customer-protection-desk."
      "assistance-operations-center-for-workspace-hosted-accounts-team."
      "example-corporation-global-technical-services-departments.com/"
      "chromoting_ftl";

  FakeClientSessionControl session_control(kLongEmail);
  std::unique_ptr<HostWindow> window = HostWindow::CreateDisconnectWindow();
  ASSERT_TRUE(window);
  window->Start(session_control.GetWeakPtr());
  EXPECT_TRUE(session_control.GetWeakPtr());

  window.reset();
  task_environment_.RunUntilIdle();
}

TEST_F(DisconnectWindowLinuxTest, WhitespaceAndUnicodeEmailDoesNotCrash) {
  if (!InitializeGtk()) {
    GTEST_SKIP() << "No display available for GTK.";
  }

  // Multi-byte Unicode with whitespace formatting
  const std::string kUnicodeEmail =
      "  \t\n  user_with_unicode_测试_🚀@example.com  \n/chromoting_ftl";

  FakeClientSessionControl session_control(kUnicodeEmail);
  std::unique_ptr<HostWindow> window = HostWindow::CreateDisconnectWindow();
  ASSERT_TRUE(window);
  window->Start(session_control.GetWeakPtr());
  EXPECT_TRUE(session_control.GetWeakPtr());

  window.reset();
  task_environment_.RunUntilIdle();
}

}  // namespace remoting
