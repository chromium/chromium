// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/app/shell_main_delegate.h"

#include "content/shell/browser/shell_application_mac.h"

namespace extensions {

std::optional<int> ShellMainDelegate::PreBrowserMain() {
  // Force the NSApplication subclass to be used.
  [ShellCrApplication sharedApplication];

  // If there was an invocation to NSApp prior to this method, then the NSApp
  // will not be a ShellCrApplication, but will instead be an NSApplication.
  // This is undesirable and we must enforce that this doesn't happen.
  CHECK([NSApp isKindOfClass:[ShellCrApplication class]]);

  return std::nullopt;
}

}  // namespace extensions
