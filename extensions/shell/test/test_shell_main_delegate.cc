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
#include "services/service_manager/embedder/switches.h"

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
  ~TestShellContentUtilityClient() override {}

  // content::ContentUtilityClient implementation.
  void RegisterNetworkBinders(
      service_manager::BinderRegistry* registry) override {
    network_service_test_helper_->RegisterNetworkBinders(registry);
  }

 private:
  std::unique_ptr<content::NetworkServiceTestHelper>
      network_service_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(TestShellContentUtilityClient);
};

}  // namespace

namespace extensions {

TestShellMainDelegate::TestShellMainDelegate() {}

TestShellMainDelegate::~TestShellMainDelegate() {}

content::ContentUtilityClient*
TestShellMainDelegate::CreateContentUtilityClient() {
  utility_client_ = std::make_unique<TestShellContentUtilityClient>();
  return utility_client_.get();
}

}  // namespace extensions
