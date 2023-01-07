// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/public/app/content_main.h"
#include "extensions/shell/app/shell_main_delegate.h"

#if BUILDFLAG(IS_WIN)
#include "content/public/app/sandbox_helper_win.h"
#include "sandbox/win/src/sandbox_types.h"
#endif

#if BUILDFLAG(IS_WIN)
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  extensions::ShellMainDelegate delegate;
  content::ContentMainParams params(&delegate);

  sandbox::SandboxInterfaceInfo sandbox_info = {nullptr};
  content::InitializeSandboxInfo(&sandbox_info);
  params.instance = instance;
  params.sandbox_info = &sandbox_info;

  return content::ContentMain(std::move(params));
}
#else
int main(int argc, const char** argv) {
  extensions::ShellMainDelegate delegate;
  content::ContentMainParams params(&delegate);

  params.argc = argc;
  params.argv = argv;

  return content::ContentMain(std::move(params));
}
#endif
