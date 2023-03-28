// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_APP_WINDOW_APP_WINDOW_API_H_
#define EXTENSIONS_BROWSER_API_APP_WINDOW_APP_WINDOW_API_H_

#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

namespace api {
namespace app_window {
struct CreateWindowOptions;
}
}

class AppWindowCreateFunction : public ExtensionFunction {
 public:
  AppWindowCreateFunction();
  DECLARE_EXTENSION_FUNCTION("app.window.create", APP_WINDOW_CREATE)

 protected:
  ~AppWindowCreateFunction() override {}
  ResponseAction Run() override;

 private:
  void OnAppWindowFinishedFirstNavigationOrClosed(AppWindow* app_window,
                                                  bool is_existing_window,
                                                  bool did_finish);

  bool GetBoundsSpec(
      const extensions::api::app_window::CreateWindowOptions& options,
      AppWindow::CreateParams* params,
      std::string* error);

  AppWindow::Frame GetFrameFromString(const std::string& frame_string);
  bool GetFrameOptions(
      const extensions::api::app_window::CreateWindowOptions& options,
      AppWindow::CreateParams* create_params,
      std::string* error);
  void UpdateFrameOptionsForChannel(AppWindow::CreateParams* create_params);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_APP_WINDOW_APP_WINDOW_API_H_
