// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/app_window/test_app_window_contents.h"

#include "content/public/browser/web_contents.h"

namespace extensions {

TestAppWindowContents::TestAppWindowContents(
    std::unique_ptr<content::WebContents> web_contents)
    : web_contents_(std::move(web_contents)) {}

TestAppWindowContents::~TestAppWindowContents() {
}

void TestAppWindowContents::Initialize(content::BrowserContext* context,
                                       content::RenderFrameHost* creator_frame,
                                       const GURL& url) {}

void TestAppWindowContents::LoadContents(int32_t creator_process_id) {}

void TestAppWindowContents::NativeWindowChanged(
    NativeAppWindow* native_app_window) {
}

void TestAppWindowContents::NativeWindowClosed(bool send_onclosed) {}

content::WebContents* TestAppWindowContents::GetWebContents() const {
  return web_contents_.get();
}

WindowController* TestAppWindowContents::GetWindowController() const {
  return nullptr;
}

}  // namespace extensions
