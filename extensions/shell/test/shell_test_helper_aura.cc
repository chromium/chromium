// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/test/shell_test_helper_aura.h"

#include <memory>

#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/test_app_window_contents.h"
#include "extensions/shell/browser/shell_app_delegate.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/compositor/compositor.h"
#include "url/gurl.h"

namespace extensions {

ShellTestHelperAura::ShellTestHelperAura() {}

ShellTestHelperAura::~ShellTestHelperAura() {}

void ShellTestHelperAura::SetUp() {
  // AuraTestHelper sets up the rest of the Aura initialization.
  helper_ = std::make_unique<aura::test::AuraTestHelper>();
  helper_->SetUp();
}

void ShellTestHelperAura::TearDown() {
  helper_->RunAllPendingInMessageLoop();
  helper_->TearDown();
}

void ShellTestHelperAura::InitAppWindow(AppWindow* app_window,
                                        const gfx::Rect& bounds) {
  // Create a TestAppWindowContents for the ShellAppDelegate to initialize the
  // ShellExtensionWebContentsObserver with.
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContents::Create(
          content::WebContents::CreateParams(app_window->browser_context())));
  auto app_window_contents =
      std::make_unique<TestAppWindowContents>(std::move(web_contents));

  // Initialize the web contents and AppWindow.
  app_window->app_delegate()->InitWebContents(
      app_window_contents->GetWebContents());

  content::RenderFrameHost* main_frame =
      app_window_contents->GetWebContents()->GetMainFrame();
  DCHECK(main_frame);

  AppWindow::CreateParams params;
  params.content_spec.bounds = bounds;
  app_window->Init(GURL(), app_window_contents.release(), main_frame, params);
}

}  // namespace extensions
