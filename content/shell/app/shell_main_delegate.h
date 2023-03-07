// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_APP_SHELL_MAIN_DELEGATE_H_
#define CONTENT_SHELL_APP_SHELL_MAIN_DELEGATE_H_

#include <memory>

#include "build/build_config.h"
#include "components/memory_system/memory_system.h"
#include "content/public/app/content_main_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class ShellContentClient;
class ShellContentBrowserClient;
class ShellContentGpuClient;
class ShellContentRendererClient;
class ShellContentUtilityClient;

#if !BUILDFLAG(IS_ANDROID)
class WebTestBrowserMainRunner;
#endif

class ShellMainDelegate : public ContentMainDelegate {
 public:
  explicit ShellMainDelegate(bool is_content_browsertests = false);

  ShellMainDelegate(const ShellMainDelegate&) = delete;
  ShellMainDelegate& operator=(const ShellMainDelegate&) = delete;

  ~ShellMainDelegate() override;

  // ContentMainDelegate implementation:
  absl::optional<int> BasicStartupComplete() override;
  bool ShouldCreateFeatureList(InvokedIn invoked_in) override;
  bool ShouldInitializeMojo(InvokedIn invoked_in) override;
  void PreSandboxStartup() override;
  absl::variant<int, MainFunctionParams> RunProcess(
      const std::string& process_type,
      MainFunctionParams main_function_params) override;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  void ZygoteForked() override;
#endif
  absl::optional<int> PreBrowserMain() override;
  absl::optional<int> PostEarlyInitialization(InvokedIn invoked_in) override;
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
#if !BUILDFLAG(IS_ANDROID)
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

  memory_system::MemorySystem memory_system_;
};

}  // namespace content

#endif  // CONTENT_SHELL_APP_SHELL_MAIN_DELEGATE_H_
