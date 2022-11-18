// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_APP_HEADLESS_SHELL_COMMAND_LINE_H_
#define HEADLESS_APP_HEADLESS_SHELL_COMMAND_LINE_H_

#include "base/command_line.h"
#include "headless/public/headless_browser.h"

namespace headless {

bool HandleCommandLineSwitches(base::CommandLine& command_line,
                               HeadlessBrowser::Options::Builder& builder);

bool IsRemoteDebuggingEnabled();

}  // namespace headless

#endif  // HEADLESS_APP_HEADLESS_SHELL_COMMAND_LINE_H_
