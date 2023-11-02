// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_SHELL_H_
#define HEADLESS_PUBLIC_HEADLESS_SHELL_H_

#include "build/build_config.h"
#include "content/public/app/content_main.h"

#if BUILDFLAG(IS_WIN)
#include "sandbox/win/src/sandbox_types.h"
#endif

namespace headless {

// Start the headless shell applications from a |ContentMainParams| object.
// Note that the |ContentMainDelegate| is ignored and
// |HeadlessContentMainDelegate| is used instead.
int HeadlessShellMain(const content::ContentMainParams& params);

// Start the Headless Shell application. Intended to be called early in main().
// Returns the exit code for the process.
#if BUILDFLAG(IS_WIN)
int HeadlessShellMain(HINSTANCE instance,
                      sandbox::SandboxInterfaceInfo* sandbox_info);
#else
int HeadlessShellMain(int argc, const char** argv);
#endif  // BUILDFLAG(IS_WIN)
}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_SHELL_H_
