// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/shell/app/shell_content_main.h"

#include "build/build_config.h"
#include "content/public/app/content_main.h"
#include "content/public/common/content_switches.h"
#include "content/shell/app/paths_mac.h"
#include "content/shell/app/shell_main_delegate.h"

#if BUILDFLAG(IS_MAC)
int ContentMain(int argc,
                const char** argv) {
  bool is_browsertest = false;
  std::string browser_test_flag(std::string("--") + switches::kBrowserTest);
  for (int i = 0; i < argc; ++i) {
    if (browser_test_flag == argv[i]) {
      is_browsertest = true;
      break;
    }
  }
  content::ShellMainDelegate delegate(is_browsertest);
  content::ContentMainParams params(&delegate);
  params.argc = argc;
  params.argv = argv;

  // Ensure that Bundle Id is set before ContentMain.
  OverrideFrameworkBundlePath();
  OverrideOuterBundlePath();
  OverrideChildProcessPath();
  OverrideSourceRootPath();
  OverrideBundleID();

  return content::ContentMain(std::move(params));
}
#endif  // BUILDFLAG(IS_MAC)
