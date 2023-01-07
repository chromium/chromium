// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_APP_WINDOW_TEST_APP_WINDOW_CONTENTS_H_
#define EXTENSIONS_BROWSER_APP_WINDOW_TEST_APP_WINDOW_CONTENTS_H_

#include <stdint.h>
#include <memory>

#include "extensions/browser/app_window/app_window.h"

namespace content {
class WebContents;
}

namespace extensions {

// A dummy version of AppWindowContents for unit tests.
// Best used with AppWindow::SetAppWindowContentsForTesting().
class TestAppWindowContents : public AppWindowContents {
 public:
  explicit TestAppWindowContents(
      std::unique_ptr<content::WebContents> web_contents);

  TestAppWindowContents(const TestAppWindowContents&) = delete;
  TestAppWindowContents& operator=(const TestAppWindowContents&) = delete;

  ~TestAppWindowContents() override;

  // apps:AppWindowContents:
  void Initialize(content::BrowserContext* context,
                  content::RenderFrameHost* creator_frame,
                  const GURL& url) override;
  void LoadContents(int32_t creator_process_id) override;
  void NativeWindowChanged(NativeAppWindow* native_app_window) override;
  void NativeWindowClosed(bool send_onclosed) override;
  content::WebContents* GetWebContents() const override;
  WindowController* GetWindowController() const override;

 private:
  std::unique_ptr<content::WebContents> web_contents_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_APP_WINDOW_TEST_APP_WINDOW_CONTENTS_H_
