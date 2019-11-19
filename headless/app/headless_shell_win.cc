// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/app/headless_shell.h"
#include "headless/app/headless_shell_switches.h"
#include "headless/public/headless_browser.h"

namespace headless {

#if defined(CHROME_MULTIPLE_DLL_CHILD)
void HeadlessShell::OnStart(HeadlessBrowser* browser) {}

void HeadlessShell::Shutdown() {}

void HeadlessShell::DevToolsTargetReady() {}

void HeadlessShell::HeadlessWebContentsDestroyed() {}
#endif  // defined(CHROME_MULTIPLE_DLL_CHILD)

}  // namespace headless
