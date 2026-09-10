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

constexpr char16_t kWindowTitle16[] = u"Chrome Remote Desktop";
constexpr char16_t kMessageSharedText[] = u"Your desktop is shared with $1.";
constexpr char16_t kStopSharingText[] = u"Stop Sharing";

constexpr char kTestUserJid[] =
    "remote.user@gmail.com/chromoting_ftl_11111111-2222-3333-4444-555555555555";

constexpr char kLongUserJid[] =
    "it-support-desk-remote-assistance-session-verification-operators@"
    "secure-remote-support-verification-and-customer-protection-desk."
    "assistance-operations-center-for-workspace-hosted-accounts-team."
    "example-corporation-global-technical-services-departments.com/"
    "chromoting_ftl_11111111-2222-3333-4444-555555555555";

constexpr char kUnicodeUserJid[] =
    "  \t\n  user_with_unicode_测试_🚀@example.com  \n"
    "/chromoting_ftl_11111111-2222-3333-4444-555555555555";

constexpr char kAttackUserJid[] =
    "it.remote.assist.operator.session.9143a@example.com.fake.tld/"
    "chromoting_ftl_11111111-2222-3333-4444-555555555555";

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
      *value = kMessageSharedText;
      return true;
    }
    if (message_id == IDS_PRODUCT_NAME) {
      *value = kWindowTitle16;
      return true;
    }
    if (message_id == IDS_STOP_SHARING_BUTTON) {
      *value = kStopSharingText;
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

GtkWidget* FindLabel(GtkWidget* widget) {
  if (GTK_IS_BUTTON(widget)) {
    return nullptr;
  }
  if (GTK_IS_LABEL(widget)) {
    return widget;
  }
#if GTK_CHECK_VERSION(3, 90, 0)
  for (GtkWidget* child = gtk_widget_get_first_child(widget); child != nullptr;
       child = gtk_widget_get_next_sibling(child)) {
    GtkWidget* found = FindLabel(child);
    if (found) {
      return found;
    }
  }
#else
  if (GTK_IS_CONTAINER(widget)) {
    GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
    GtkWidget* found = nullptr;
    for (GList* iter = children; iter != nullptr; iter = g_list_next(iter)) {
      found = FindLabel(GTK_WIDGET(iter->data));
      if (found) {
        break;
      }
    }
    g_list_free(children);
    return found;
  }
#endif
  return nullptr;
}

GtkWidget* FindDisconnectWindowLabel() {
#if GTK_CHECK_VERSION(3, 90, 0)
  GListModel* toplevels = gtk_window_get_toplevels();
  guint n_items = g_list_model_get_n_items(toplevels);
  for (guint i = 0; i < n_items; ++i) {
    gpointer item = g_list_model_get_item(toplevels, i);
    GtkWidget* label = FindLabel(GTK_WIDGET(item));
    g_object_unref(item);
    if (label) {
      return label;
    }
  }
  return nullptr;
#else
  GList* toplevels = gtk_window_list_toplevels();
  GtkWidget* label = nullptr;
  for (GList* iter = toplevels; iter != nullptr; iter = g_list_next(iter)) {
    label = FindLabel(GTK_WIDGET(iter->data));
    if (label) {
      break;
    }
  }
  g_list_free(toplevels);
  return label;
#endif
}

}  // namespace

class DisconnectWindowLinuxTest : public testing::Test {
 public:
  DisconnectWindowLinuxTest() = default;
  ~DisconnectWindowLinuxTest() override = default;

  void TearDown() override {
    // Pump the event loop to ensure GTK widget destruction and cleanup is
    // processed.
    task_environment_.RunUntilIdle();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  TestResourceBundleDelegate resource_delegate_;
  ui::ResourceBundle resource_bundle_{&resource_delegate_};
  ui::ResourceBundle::SharedInstanceSwapperForTesting resource_swapper_{
      &resource_bundle_};
};

TEST_F(DisconnectWindowLinuxTest, NormalEmailDoesNotCrash) {
  if (!InitializeGtk()) {
    GTEST_SKIP() << "No display available for GTK.";
  }

  FakeClientSessionControl session_control(kTestUserJid);
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

  FakeClientSessionControl session_control(kLongUserJid);
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

  FakeClientSessionControl session_control(kUnicodeUserJid);
  std::unique_ptr<HostWindow> window = HostWindow::CreateDisconnectWindow();
  ASSERT_TRUE(window);
  window->Start(session_control.GetWeakPtr());
  EXPECT_TRUE(session_control.GetWeakPtr());

  window.reset();
  task_environment_.RunUntilIdle();
}

TEST_F(DisconnectWindowLinuxTest, LongEmailPreservesDomainSuffix) {
  if (!InitializeGtk()) {
    GTEST_SKIP() << "No display available for GTK.";
  }

  // An email exceeding kDefaultMaxEmailLength (36 characters) where the
  // authentic domain suffix would have been truncated under naive
  // end-truncation.
  FakeClientSessionControl session_control(kAttackUserJid);
  std::unique_ptr<HostWindow> window = HostWindow::CreateDisconnectWindow();
  ASSERT_TRUE(window);
  window->Start(session_control.GetWeakPtr());
  EXPECT_TRUE(session_control.GetWeakPtr());

  GtkWidget* label = FindDisconnectWindowLabel();
  ASSERT_NE(label, nullptr);
  const gchar* label_text = gtk_label_get_text(GTK_LABEL(label));
  ASSERT_NE(label_text, nullptr);
  EXPECT_THAT(label_text, testing::HasSubstr("fake.tld"));

  window.reset();
}

}  // namespace remoting
