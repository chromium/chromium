// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/test/test_shell_main_delegate.h"

#include "base/command_line.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/network_service_test_helper.h"
#include "content/public/utility/content_utility_client.h"
#include "content/shell/common/shell_switches.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace {

class TestShellContentUtilityClient : public content::ContentUtilityClient {
 public:
  TestShellContentUtilityClient() {
    if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kProcessType) == switches::kUtilityProcess) {
      network_service_test_helper_ =
          std::make_unique<content::NetworkServiceTestHelper>();
    }
  }

  TestShellContentUtilityClient(const TestShellContentUtilityClient&) = delete;
  TestShellContentUtilityClient& operator=(
      const TestShellContentUtilityClient&) = delete;

  ~TestShellContentUtilityClient() override {}

  // content::ContentUtilityClient implementation.
  void RegisterNetworkBinders(
      service_manager::BinderRegistry* registry) override {
    network_service_test_helper_->RegisterNetworkBinders(registry);
  }

 private:
  std::unique_ptr<content::NetworkServiceTestHelper>
      network_service_test_helper_;
};

}  // namespace

namespace extensions {

TestShellMainDelegate::TestShellMainDelegate() {}

TestShellMainDelegate::~TestShellMainDelegate() {}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
absl::optional<int> TestShellMainDelegate::PostEarlyInitialization(
    InvokedIn invoked_in) {
  if (absl::holds_alternative<InvokedInBrowserProcess>(invoked_in)) {
    // Browser tests on Lacros requires a non-null LacrosService.
    lacros_service_ = std::make_unique<chromeos::LacrosService>();
  }
  extensions::ShellMainDelegate::PostEarlyInitialization(invoked_in);

  return absl::nullopt;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

content::ContentUtilityClient*
TestShellMainDelegate::CreateContentUtilityClient() {
  utility_client_ = std::make_unique<TestShellContentUtilityClient>();
  return utility_client_.get();
}

}  // namespace extensions
