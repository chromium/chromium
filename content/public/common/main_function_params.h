// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_MAIN_FUNCTION_PARAMS_H_
#define CONTENT_PUBLIC_COMMON_MAIN_FUNCTION_PARAMS_H_

#include <memory>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

#if BUILDFLAG(IS_WIN)
namespace sandbox {
struct SandboxInterfaceInfo;
}
#elif BUILDFLAG(IS_MAC)
namespace base {
namespace mac {
class ScopedNSAutoreleasePool;
}
}
#endif

namespace content {

class BrowserMainParts;
struct StartupData;

using CreatedMainPartsClosure = base::OnceCallback<void(BrowserMainParts*)>;

// Wrapper to the parameter list for the "main" entry points (browser, renderer,
// plugin) to shield the call sites from the differences between platforms
// (e.g., POSIX doesn't need to pass any sandbox information).
struct CONTENT_EXPORT MainFunctionParams {
  explicit MainFunctionParams(const base::CommandLine* cl);
  ~MainFunctionParams();

  // Do not reuse the moved-from MainFunctionParams after this call.
  MainFunctionParams(MainFunctionParams&&);
  MainFunctionParams& operator=(MainFunctionParams&&);

  const base::CommandLine* command_line;

#if BUILDFLAG(IS_WIN)
  sandbox::SandboxInterfaceInfo* sandbox_info = nullptr;
#elif BUILDFLAG(IS_MAC)
  base::mac::ScopedNSAutoreleasePool* autorelease_pool = nullptr;
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
  bool zygote_child = false;
#endif

  // Used by BrowserTestBase. If set, BrowserMainLoop runs this task instead of
  // the main message loop.
  base::OnceClosure ui_task;

  // Used by BrowserTestBase. If set, this is invoked after BrowserMainParts has
  // been created and before PreEarlyInitialization().
  CreatedMainPartsClosure created_main_parts_closure;

  // Used by //content, when the embedder yields control back to it, to extract
  // startup data passed from ContentMainRunner.
  std::unique_ptr<StartupData> startup_data;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_MAIN_FUNCTION_PARAMS_H_
