// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_APP_SHELL_MAIN_DELEGATE_H_
#define CONTENT_SHELL_APP_SHELL_MAIN_DELEGATE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/app/content_main_delegate.h"

namespace content {
class ShellContentClient;
class ShellContentBrowserClient;
class ShellContentGpuClient;
class ShellContentRendererClient;
class ShellContentUtilityClient;

#if !defined(OS_ANDROID)
class WebTestBrowserMainRunner;
#endif

class ShellMainDelegate : public ContentMainDelegate {
 public:
  explicit ShellMainDelegate(bool is_content_browsertests = false);
  ~ShellMainDelegate() override;

  // ContentMainDelegate implementation:
  bool BasicStartupComplete(int* exit_code) override;
  void PreSandboxStartup() override;
  int RunProcess(const std::string& process_type,
                 const MainFunctionParams& main_function_params) override;
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  void ZygoteForked() override;
#endif
  void PreBrowserMain() override;
  ContentClient* CreateContentClient() override;
  ContentBrowserClient* CreateContentBrowserClient() override;
  ContentGpuClient* CreateContentGpuClient() override;
  ContentRendererClient* CreateContentRendererClient() override;
  ContentUtilityClient* CreateContentUtilityClient() override;

  static void InitializeResourceBundle();

 protected:
  // Only present when running content_browsertests, which run inside Content
  // Shell.
  //
  // content_browsertests should not set the kRunWebTests command line flag, so
  // |is_content_browsertests_| and |web_test_runner_| are mututally exclusive.
  bool is_content_browsertests_;
#if !defined(OS_ANDROID)
  // Only present when running web tests, which run inside Content Shell.
  //
  // Web tests are not browser tests, so |is_content_browsertests_| and
  // |web_test_runner_| are mututally exclusive.
  std::unique_ptr<WebTestBrowserMainRunner> web_test_runner_;
#endif

  std::unique_ptr<ShellContentBrowserClient> browser_client_;
  std::unique_ptr<ShellContentGpuClient> gpu_client_;
  std::unique_ptr<ShellContentRendererClient> renderer_client_;
  std::unique_ptr<ShellContentUtilityClient> utility_client_;
  std::unique_ptr<ShellContentClient> content_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellMainDelegate);
};

}  // namespace content

#endif  // CONTENT_SHELL_APP_SHELL_MAIN_DELEGATE_H_
