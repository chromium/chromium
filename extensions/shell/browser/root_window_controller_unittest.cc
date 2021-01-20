// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/root_window_controller.h"

#include <algorithm>
#include <list>
#include <memory>

#include "content/public/browser/browser_context.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/extension_builder.h"
#include "extensions/shell/browser/root_window_controller.h"
#include "extensions/shell/browser/shell_app_window_client.h"
#include "extensions/shell/browser/shell_native_app_window_aura.h"
#include "extensions/shell/test/shell_test_base_aura.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace extensions {

namespace {

constexpr gfx::Rect kScreenBounds = gfx::Rect(0, 0, 800, 600);

// A fake that creates and exposes RootWindowControllers.
class FakeDesktopDelegate : public RootWindowController::DesktopDelegate {
 public:
  explicit FakeDesktopDelegate(content::BrowserContext* browser_context)
      : browser_context_(browser_context) {}
  ~FakeDesktopDelegate() override = default;

  RootWindowController* CreateRootWindowController() {
    root_window_controllers_.emplace_back(
        std::make_unique<RootWindowController>(this, kScreenBounds,
                                               browser_context_));
    return root_window_controllers_.back().get();
  }

  // RootWindowController::DesktopDelegate:
  void CloseRootWindowController(
      RootWindowController* root_window_controller) override {
    auto it = std::find_if(root_window_controllers_.begin(),
                           root_window_controllers_.end(),
                           [&](const auto& candidate) {
                             return candidate.get() == root_window_controller;
                           });
    DCHECK(it != root_window_controllers_.end());
    root_window_controllers_.erase(it);
  }

  auto root_window_controller_count() {
    return root_window_controllers_.size();
  }

 private:
  content::BrowserContext* browser_context_;
  std::list<std::unique_ptr<RootWindowController>> root_window_controllers_;

  DISALLOW_COPY_AND_ASSIGN(FakeDesktopDelegate);
};

// An AppWindowClient for use without a DesktopController.
class TestAppWindowClient : public ShellAppWindowClient {
 public:
  TestAppWindowClient() = default;
  ~TestAppWindowClient() override = default;

  NativeAppWindow* CreateNativeAppWindow(
      AppWindow* window,
      AppWindow::CreateParams* params) override {
    return new ShellNativeAppWindowAura(window, *params);
  }
};

}  // namespace

class RootWindowControllerTest : public ShellTestBaseAura {
 public:
  RootWindowControllerTest() = default;
  ~RootWindowControllerTest() override = default;

  void SetUp() override {
    ShellTestBaseAura::SetUp();

    AppWindowClient::Set(&app_window_client_);
    extension_ = ExtensionBuilder("Test").Build();

    desktop_delegate_ =
        std::make_unique<FakeDesktopDelegate>(browser_context());
  }

  void TearDown() override {
    desktop_delegate_.reset();
    AppWindowClient::Set(nullptr);
    ShellTestBaseAura::TearDown();
  }

 protected:
  // Creates and returns an AppWindow using the RootWindowController.
  AppWindow* CreateAppWindow(RootWindowController* root) {
    AppWindow* app_window =
        AppWindowClient::Get()->CreateAppWindow(browser_context(), extension());
    InitAppWindow(app_window);
    root->AddAppWindow(app_window, app_window->GetNativeWindow());
    return app_window;
  }

  // Checks that there are |num_expected| AppWindows registered.
  void ExpectNumAppWindows(size_t expected) {
    EXPECT_EQ(expected,
              AppWindowRegistry::Get(browser_context())->app_windows().size());
  }

  // Checks that |root|'s root window has |num_expected| child windows.
  void ExpectNumChildWindows(size_t expected, RootWindowController* root) {
    EXPECT_EQ(expected, root->host()->window()->children().size());
  }

  FakeDesktopDelegate* desktop_delegate() { return desktop_delegate_.get(); }
  const Extension* extension() { return extension_.get(); }

 private:
  TestAppWindowClient app_window_client_;

  scoped_refptr<const Extension> extension_;
  std::unique_ptr<FakeDesktopDelegate> desktop_delegate_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowControllerTest);
};

// Tests RootWindowController's basic setup and teardown.
TEST_F(RootWindowControllerTest, Basic) {
  RootWindowController* root_window_controller =
      desktop_delegate()->CreateRootWindowController();
  EXPECT_TRUE(root_window_controller->host());

  // The RootWindowController destroys itself when the root window closes.
  root_window_controller->OnHostCloseRequested(root_window_controller->host());
  EXPECT_EQ(0u, desktop_delegate()->root_window_controller_count());
}

// Tests the window layout.
TEST_F(RootWindowControllerTest, FillLayout) {
  RootWindowController* root_window_controller =
      desktop_delegate()->CreateRootWindowController();

  root_window_controller->host()->SetBoundsInPixels(gfx::Rect(0, 0, 500, 700));

  CreateAppWindow(root_window_controller);
  ExpectNumAppWindows(1u);
  ExpectNumChildWindows(1u, root_window_controller);

  // Test that reshaping the host window also resizes the child window, and
  // moving the host doesn't affect the child's position relative to the host.
  root_window_controller->host()->SetBoundsInPixels(
      gfx::Rect(100, 200, 300, 400));

  const aura::Window* root_window = root_window_controller->host()->window();
  EXPECT_EQ(gfx::Rect(0, 0, 300, 400), root_window->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 300, 400), root_window->children()[0]->bounds());

  // The AppWindow will close on shutdown.
}

// Tests creating and removing AppWindows.
TEST_F(RootWindowControllerTest, AppWindows) {
  RootWindowController* root_window_controller =
      desktop_delegate()->CreateRootWindowController();

  {
    // Create some AppWindows.
    CreateAppWindow(root_window_controller);
    AppWindow* middle_window = CreateAppWindow(root_window_controller);
    CreateAppWindow(root_window_controller);

    ExpectNumAppWindows(3u);
    ExpectNumChildWindows(3u, root_window_controller);

    // Close one window, which deletes |middle_window|.
    middle_window->GetBaseWindow()->Close();
  }

  ExpectNumAppWindows(2u);
  ExpectNumChildWindows(2u, root_window_controller);

  // Close all remaining windows.
  root_window_controller->CloseAppWindows();
  ExpectNumAppWindows(0u);
}

// Tests that a second RootWindowController can be used independently of the
// first.
TEST_F(RootWindowControllerTest, Multiple) {
  // Create the longer-lived RootWindowController before the shorter-lived one
  // is deleted. Otherwise it may be created at the same address, preventing
  // the test from failing on use-after-free.
  RootWindowController* longer_lived =
      desktop_delegate()->CreateRootWindowController();
  AppWindow* longer_lived_app_window = CreateAppWindow(longer_lived);
  ExpectNumAppWindows(1u);

  {
    RootWindowController* shorter_lived =
        desktop_delegate()->CreateRootWindowController();
    AppWindow* app_window = CreateAppWindow(shorter_lived);
    ExpectNumAppWindows(2u);
    app_window->GetBaseWindow()->Close();  // Deletes the AppWindow.
  }
  ExpectNumAppWindows(1u);

  // The still-living RootWindowController can still be used.
  AppWindow* longer_lived_app_window2 = CreateAppWindow(longer_lived);
  ExpectNumAppWindows(2u);
  longer_lived_app_window->GetBaseWindow()->Close();
  ExpectNumAppWindows(1u);
  longer_lived_app_window2->GetBaseWindow()->Close();
  ExpectNumAppWindows(0u);
}

}  // namespace extensions
