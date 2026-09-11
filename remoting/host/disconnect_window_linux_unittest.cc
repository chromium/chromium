// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include <memory>
#include <string>
#include <string_view>
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

constexpr char kWindowTitle[] = "Chrome Remote Desktop";
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

constexpr int kDisplacementOffset = 300;
constexpr int kDisplacementAttemptsOverLimit = 5;
constexpr int kMaxRepositionAttempts = 3;

constexpr char kUpArrow[] = "▲";
constexpr char kDownArrow[] = "▼";

constexpr char kCurrentWidthKey[] = "current_width";
constexpr char kCurrentHeightKey[] = "current_height";
constexpr char kExpectedXKey[] = "expected_x";
constexpr char kExpectedYKey[] = "expected_y";
constexpr char kRepositionAttemptsKey[] = "reposition_attempts";

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

GtkWidget* FindButtonByLabel(GtkWidget* widget, std::string_view label_text) {
  if (GTK_IS_BUTTON(widget)) {
    const gchar* label = gtk_button_get_label(GTK_BUTTON(widget));
    if (label && label == label_text) {
      return widget;
    }
  }
#if GTK_CHECK_VERSION(3, 90, 0)
  for (GtkWidget* child = gtk_widget_get_first_child(widget); child != nullptr;
       child = gtk_widget_get_next_sibling(child)) {
    GtkWidget* found = FindButtonByLabel(child, label_text);
    if (found) {
      return found;
    }
  }
#else
  if (GTK_IS_CONTAINER(widget)) {
    GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
    GtkWidget* found = nullptr;
    for (GList* iter = children; iter != nullptr; iter = g_list_next(iter)) {
      found = FindButtonByLabel(GTK_WIDGET(iter->data), label_text);
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

GtkWindow* FindDisconnectWindow() {
#if GTK_CHECK_VERSION(3, 90, 0)
  GListModel* toplevels = gtk_window_get_toplevels();
  guint n_items = g_list_model_get_n_items(toplevels);
  for (guint i = 0; i < n_items; ++i) {
    gpointer item = g_list_model_get_item(toplevels, i);
    if (GTK_IS_WINDOW(item)) {
      const gchar* title = gtk_window_get_title(GTK_WINDOW(item));
      if (title && std::string_view(title) == kWindowTitle) {
        GtkWindow* window = GTK_WINDOW(item);
        g_object_unref(item);
        return window;
      }
    }
    g_object_unref(item);
  }
  return nullptr;
#else
  GList* toplevels = gtk_window_list_toplevels();
  GtkWindow* window = nullptr;
  for (GList* iter = toplevels; iter != nullptr; iter = g_list_next(iter)) {
    if (GTK_IS_WINDOW(iter->data)) {
      const gchar* title = gtk_window_get_title(GTK_WINDOW(iter->data));
      if (title && std::string_view(title) == kWindowTitle) {
        window = GTK_WINDOW(iter->data);
        break;
      }
    }
  }
  g_list_free(toplevels);
  return window;
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

#if !GTK_CHECK_VERSION(3, 90, 0)
TEST_F(DisconnectWindowLinuxTest, ToggleAlignmentMovesWindowBetweenAnchors) {
  if (!InitializeGtk()) {
    GTEST_SKIP() << "No display available for GTK.";
  }

  FakeClientSessionControl session_control(kTestUserJid);
  std::unique_ptr<HostWindow> window = HostWindow::CreateDisconnectWindow();
  ASSERT_TRUE(window);
  window->Start(session_control.GetWeakPtr());

  GtkWindow* gtk_window = FindDisconnectWindow();
  ASSERT_NE(gtk_window, nullptr);

  // Initial anchor is bottom.
  int initial_expected_y =
      GPOINTER_TO_INT(g_object_get_data(G_OBJECT(gtk_window), kExpectedYKey));
  int expected_x =
      GPOINTER_TO_INT(g_object_get_data(G_OBJECT(gtk_window), kExpectedXKey));
  EXPECT_GT(initial_expected_y, 0);

  // Find the toggle button (initially showing up-arrow "▲").
  GtkWidget* toggle_button =
      FindButtonByLabel(GTK_WIDGET(gtk_window), kUpArrow);
  ASSERT_NE(toggle_button, nullptr);

  // Click the toggle button to move the window to the top anchor.
  g_signal_emit_by_name(toggle_button, "clicked");

  int toggled_expected_y =
      GPOINTER_TO_INT(g_object_get_data(G_OBJECT(gtk_window), kExpectedYKey));
  // Top anchor coordinate should be higher up (smaller y) than bottom anchor.
  EXPECT_LT(toggled_expected_y, initial_expected_y);
  EXPECT_EQ(
      GPOINTER_TO_INT(g_object_get_data(G_OBJECT(gtk_window), kExpectedXKey)),
      expected_x);

  // Toggle button label updates to down-arrow "▼" and disables during cooldown.
  EXPECT_STREQ(gtk_button_get_label(GTK_BUTTON(toggle_button)), kDownArrow);
  EXPECT_FALSE(gtk_widget_get_sensitive(toggle_button));

  // Legitimate configure event at the new anchor coordinates should NOT trigger
  // displacement snap-back (reposition attempts remain 0).
  int width = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(gtk_window), kCurrentWidthKey));
  int height = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(gtk_window), kCurrentHeightKey));

  GdkEventConfigure configure_event;
  memset(&configure_event, 0, sizeof(configure_event));
  configure_event.type = GDK_CONFIGURE;
  configure_event.window = gtk_widget_get_window(GTK_WIDGET(gtk_window));
  configure_event.send_event = TRUE;
  configure_event.x = expected_x;
  configure_event.y = toggled_expected_y;
  configure_event.width = width;
  configure_event.height = height;

  gboolean handled = FALSE;
  g_signal_emit_by_name(gtk_window, "configure-event", &configure_event,
                        &handled);
  int reposition_attempts = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(gtk_window), kRepositionAttemptsKey));
  EXPECT_EQ(reposition_attempts, 0);

  // External displacement (e.g. shortcut move) away from new anchor triggers
  // snap-back.
  configure_event.x = expected_x + kDisplacementOffset;
  configure_event.y = toggled_expected_y + kDisplacementOffset;
  g_signal_emit_by_name(gtk_window, "configure-event", &configure_event,
                        &handled);
  reposition_attempts = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(gtk_window), kRepositionAttemptsKey));
  EXPECT_EQ(reposition_attempts, 1);

  // Click again to toggle back to bottom anchor so global anchor state is
  // restored.
  g_signal_emit_by_name(toggle_button, "clicked");
  EXPECT_EQ(
      GPOINTER_TO_INT(g_object_get_data(G_OBJECT(gtk_window), kExpectedYKey)),
      initial_expected_y);

  window.reset();
}

TEST_F(DisconnectWindowLinuxTest, ConfigureEventDisplacementRestoresPosition) {
  if (!InitializeGtk()) {
    GTEST_SKIP() << "No display available for GTK.";
  }

  FakeClientSessionControl session_control(kTestUserJid);
  std::unique_ptr<HostWindow> window = HostWindow::CreateDisconnectWindow();
  ASSERT_TRUE(window);
  window->Start(session_control.GetWeakPtr());

  GtkWindow* gtk_window = FindDisconnectWindow();
  ASSERT_NE(gtk_window, nullptr);

  int expected_x =
      GPOINTER_TO_INT(g_object_get_data(G_OBJECT(gtk_window), kExpectedXKey));
  int expected_y =
      GPOINTER_TO_INT(g_object_get_data(G_OBJECT(gtk_window), kExpectedYKey));
  int width = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(gtk_window), kCurrentWidthKey));
  int height = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(gtk_window), kCurrentHeightKey));
  EXPECT_GT(expected_x, 0);
  EXPECT_GT(expected_y, 0);
  EXPECT_GT(width, 0);
  EXPECT_GT(height, 0);

  // Simulate an external displacement (e.g. window manager shortcut move).
  GdkEventConfigure configure_event;
  memset(&configure_event, 0, sizeof(configure_event));
  configure_event.type = GDK_CONFIGURE;
  configure_event.window = gtk_widget_get_window(GTK_WIDGET(gtk_window));
  configure_event.send_event = TRUE;
  configure_event.x = expected_x + kDisplacementOffset;
  configure_event.y = expected_y + kDisplacementOffset;
  configure_event.width = width;
  configure_event.height = height;

  gboolean handled = FALSE;
  g_signal_emit_by_name(gtk_window, "configure-event", &configure_event,
                        &handled);

  // Reposition attempt counter must be incremented upon displacement.
  int reposition_attempts = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(gtk_window), kRepositionAttemptsKey));
  EXPECT_EQ(reposition_attempts, 1);

  // When a configure event matching expected anchor coordinates arrives,
  // the reposition counter must reset to 0.
  configure_event.x = expected_x;
  configure_event.y = expected_y;
  g_signal_emit_by_name(gtk_window, "configure-event", &configure_event,
                        &handled);
  reposition_attempts = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(gtk_window), kRepositionAttemptsKey));
  EXPECT_EQ(reposition_attempts, 0);

  window.reset();
}

TEST_F(DisconnectWindowLinuxTest, ConfigureEventDisplacementBoundedAttempts) {
  if (!InitializeGtk()) {
    GTEST_SKIP() << "No display available for GTK.";
  }

  FakeClientSessionControl session_control(kTestUserJid);
  std::unique_ptr<HostWindow> window = HostWindow::CreateDisconnectWindow();
  ASSERT_TRUE(window);
  window->Start(session_control.GetWeakPtr());

  GtkWindow* gtk_window = FindDisconnectWindow();
  ASSERT_NE(gtk_window, nullptr);

  int expected_x =
      GPOINTER_TO_INT(g_object_get_data(G_OBJECT(gtk_window), kExpectedXKey));
  int expected_y =
      GPOINTER_TO_INT(g_object_get_data(G_OBJECT(gtk_window), kExpectedYKey));
  int width = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(gtk_window), kCurrentWidthKey));
  int height = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(gtk_window), kCurrentHeightKey));
  EXPECT_GT(width, 0);
  EXPECT_GT(height, 0);

  GdkEventConfigure configure_event;
  memset(&configure_event, 0, sizeof(configure_event));
  configure_event.type = GDK_CONFIGURE;
  configure_event.window = gtk_widget_get_window(GTK_WIDGET(gtk_window));
  configure_event.send_event = TRUE;
  configure_event.x = expected_x + kDisplacementOffset;
  configure_event.y = expected_y + kDisplacementOffset;
  configure_event.width = width;
  configure_event.height = height;

  gboolean handled = FALSE;
  // Send more displacement configure events than kMaxRepositionAttempts (3).
  for (int i = 0; i < kDisplacementAttemptsOverLimit; ++i) {
    g_signal_emit_by_name(gtk_window, "configure-event", &configure_event,
                          &handled);
  }

  // Counter must be capped at 3 to prevent infinite loops.
  int reposition_attempts = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(gtk_window), kRepositionAttemptsKey));
  EXPECT_EQ(reposition_attempts, kMaxRepositionAttempts);

  window.reset();
}

TEST_F(DisconnectWindowLinuxTest, WindowStateEventRestoresMinimizedWindow) {
  if (!InitializeGtk()) {
    GTEST_SKIP() << "No display available for GTK.";
  }

  FakeClientSessionControl session_control(kTestUserJid);
  std::unique_ptr<HostWindow> window = HostWindow::CreateDisconnectWindow();
  ASSERT_TRUE(window);
  window->Start(session_control.GetWeakPtr());

  GtkWindow* gtk_window = FindDisconnectWindow();
  ASSERT_NE(gtk_window, nullptr);

  // Simulate window minimization/iconification state event.
  GdkEventWindowState state_event;
  memset(&state_event, 0, sizeof(state_event));
  state_event.type = GDK_WINDOW_STATE;
  state_event.window = gtk_widget_get_window(GTK_WIDGET(gtk_window));
  state_event.send_event = TRUE;
  state_event.changed_mask = GDK_WINDOW_STATE_ICONIFIED;
  state_event.new_window_state = GDK_WINDOW_STATE_ICONIFIED;

  gboolean handled = FALSE;
  g_signal_emit_by_name(gtk_window, "window-state-event", &state_event,
                        &handled);

  // Window position should be refreshed and reposition attempts reset.
  int reposition_attempts = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(gtk_window), kRepositionAttemptsKey));
  EXPECT_EQ(reposition_attempts, 0);
  int expected_x =
      GPOINTER_TO_INT(g_object_get_data(G_OBJECT(gtk_window), kExpectedXKey));
  EXPECT_GT(expected_x, 0);

  window.reset();
}

TEST_F(DisconnectWindowLinuxTest, WindowStateEventRestoresAboveAndSticky) {
  if (!InitializeGtk()) {
    GTEST_SKIP() << "No display available for GTK.";
  }

  FakeClientSessionControl session_control(kTestUserJid);
  std::unique_ptr<HostWindow> window = HostWindow::CreateDisconnectWindow();
  ASSERT_TRUE(window);
  window->Start(session_control.GetWeakPtr());

  GtkWindow* gtk_window = FindDisconnectWindow();
  ASSERT_NE(gtk_window, nullptr);

  // Simulate loss of ABOVE and STICKY state.
  GdkEventWindowState state_event;
  memset(&state_event, 0, sizeof(state_event));
  state_event.type = GDK_WINDOW_STATE;
  state_event.window = gtk_widget_get_window(GTK_WIDGET(gtk_window));
  state_event.send_event = TRUE;
  state_event.changed_mask = static_cast<GdkWindowState>(
      GDK_WINDOW_STATE_ABOVE | GDK_WINDOW_STATE_STICKY);
  state_event.new_window_state = static_cast<GdkWindowState>(0);

  gboolean handled = FALSE;
  g_signal_emit_by_name(gtk_window, "window-state-event", &state_event,
                        &handled);

  // Window should not crash and should remain active.
  EXPECT_TRUE(session_control.GetWeakPtr());

  window.reset();
}
#endif

}  // namespace remoting
