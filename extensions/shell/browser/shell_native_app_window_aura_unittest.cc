// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_native_app_window_aura.h"

#include <memory>

#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/test_app_window_contents.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/shell/browser/shell_app_delegate.h"
#include "extensions/shell/browser/shell_app_window_client.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace extensions {

class ShellNativeAppWindowAuraTest : public ExtensionsTest {
 public:
  ShellNativeAppWindowAuraTest() { AppWindowClient::Set(&app_window_client_); }

  ~ShellNativeAppWindowAuraTest() override { AppWindowClient::Set(nullptr); }

 protected:
  ShellAppWindowClient app_window_client_;
};

TEST_F(ShellNativeAppWindowAuraTest, Bounds) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "test extension")
                           .Set("version", "1")
                           .Set("manifest_version", 2)
                           .Build())
          .Build();

  AppWindow* app_window =
      new AppWindow(browser_context(), new ShellAppDelegate, extension.get());

  std::unique_ptr<content::WebContents> web_contents(
      content::WebContents::Create(
          content::WebContents::CreateParams(browser_context())));
  app_window->SetAppWindowContentsForTesting(
      std::make_unique<TestAppWindowContents>(std::move(web_contents)));

  AppWindow::BoundsSpecification window_spec;
  window_spec.bounds = gfx::Rect(100, 200, 300, 400);
  AppWindow::CreateParams params;
  params.window_spec = window_spec;

  ShellNativeAppWindowAura window(app_window, params);

  gfx::Rect bounds = window.GetBounds();
  EXPECT_EQ(window_spec.bounds, bounds);

  // The window should not be resizable from the extension API.
  EXPECT_EQ(bounds.size(), window.GetContentMinimumSize());
  EXPECT_EQ(bounds.size(), window.GetContentMaximumSize());

  // Delete the AppWindow.
  app_window->OnNativeClose();
}

}  // namespace extensions
