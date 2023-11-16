// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/test/test_shell_main_delegate.h"

#include "base/command_line.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/network_service_test_helper.h"
#include "content/public/utility/content_utility_client.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "ui/gfx/linux/gbm_util.h"  // nogncheck
#endif

namespace {

class TestShellContentUtilityClient : public content::ContentUtilityClient {
 public:
  TestShellContentUtilityClient() {
    network_service_test_helper_ = content::NetworkServiceTestHelper::Create();
  }

  TestShellContentUtilityClient(const TestShellContentUtilityClient&) = delete;
  TestShellContentUtilityClient& operator=(
      const TestShellContentUtilityClient&) = delete;

  ~TestShellContentUtilityClient() override {}

 private:
  std::unique_ptr<content::NetworkServiceTestHelper>
      network_service_test_helper_;
};

}  // namespace

namespace extensions {

TestShellMainDelegate::TestShellMainDelegate() = default;

TestShellMainDelegate::~TestShellMainDelegate() = default;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
std::optional<int> TestShellMainDelegate::PostEarlyInitialization(
    InvokedIn invoked_in) {
  // At this point, the base::FeatureList has been initialized and the process
  // should still be single threaded. Additionally, minigbm shouldn't have been
  // used yet by this process. Therefore, it's a good time to ensure the Intel
  // media compression environment flag for minigbm is correctly set.
  ui::EnsureIntelMediaCompressionEnvVarIsSet();
  if (absl::holds_alternative<InvokedInBrowserProcess>(invoked_in)) {
    // Browser tests on Lacros requires a non-null LacrosService.
    lacros_service_ = std::make_unique<chromeos::LacrosService>();
  }
  extensions::ShellMainDelegate::PostEarlyInitialization(invoked_in);

  return std::nullopt;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

content::ContentUtilityClient*
TestShellMainDelegate::CreateContentUtilityClient() {
  utility_client_ = std::make_unique<TestShellContentUtilityClient>();
  return utility_client_.get();
}

}  // namespace extensions
