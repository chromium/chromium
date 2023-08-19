// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/shell_web_main_parts.h"

#import "ios/web/shell/shell_browser_state.h"

#if DCHECK_IS_ON()
#import "ui/display/screen_base.h"
#endif

namespace web {

ShellWebMainParts::ShellWebMainParts() {
}

ShellWebMainParts::~ShellWebMainParts() {
#if DCHECK_IS_ON()
  // The screen object is never deleted on IOS. Make sure that all display
  // observers are removed at the end.
  display::ScreenBase* screen =
      static_cast<display::ScreenBase*>(display::Screen::GetScreen());
  DCHECK(!screen->HasDisplayObservers());
#endif
}

void ShellWebMainParts::PreMainMessageLoopRun() {
  browser_state_.reset(new ShellBrowserState);
}

}  // namespace web
