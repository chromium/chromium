// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_SHELL_WEB_MAIN_PARTS_H_
#define IOS_WEB_SHELL_SHELL_WEB_MAIN_PARTS_H_

#include <memory>

#include "ios/web/public/init/web_main_parts.h"

namespace web {
class ShellBrowserState;

// Shell-specific implementation of WebMainParts.
class ShellWebMainParts : public WebMainParts {
 public:
  ShellWebMainParts();
  ~ShellWebMainParts() override;

  ShellBrowserState* browser_state() const { return browser_state_.get(); }

  // WebMainParts implementation.
  void PreMainMessageLoopRun() override;

 private:
  std::unique_ptr<ShellBrowserState> browser_state_;
};

}  // namespace web

#endif  // IOS_WEB_SHELL_SHELL_WEB_MAIN_PARTS_H_
