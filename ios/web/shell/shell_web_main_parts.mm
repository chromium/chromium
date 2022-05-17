// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/shell/shell_web_main_parts.h"

#include "ios/web/shell/shell_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

ShellWebMainParts::ShellWebMainParts() {
}

ShellWebMainParts::~ShellWebMainParts() {
}

void ShellWebMainParts::PreMainMessageLoopRun() {
  browser_state_.reset(new ShellBrowserState);
}

}  // namespace web
