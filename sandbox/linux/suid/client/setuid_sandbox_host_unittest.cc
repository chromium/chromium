// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/suid/client/setuid_sandbox_host.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include "base/environment.h"
#include "base/strings/string_number_conversions.h"
#include "sandbox/linux/suid/common/sandbox.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

TEST(SetuidSandboxHost, SetupLaunchEnvironment) {
  const char kTestValue[] = "This is a test";
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  EXPECT_TRUE(env != NULL);

  // Setup environment variables to save or not save.
  std::optional<std::string> saved_ld_preload = env->GetVar("LD_PRELOAD");
  EXPECT_TRUE(env->SetVar("LD_PRELOAD", kTestValue));
  EXPECT_TRUE(env->UnSetVar("LD_ORIGIN_PATH"));

  std::unique_ptr<SetuidSandboxHost> sandbox_host(SetuidSandboxHost::Create());
  EXPECT_TRUE(sandbox_host != NULL);

  // Make sure the environment is clean.
  EXPECT_TRUE(env->UnSetVar(kSandboxEnvironmentApiRequest));
  EXPECT_TRUE(env->UnSetVar(kSandboxEnvironmentApiProvides));

  sandbox_host->SetupLaunchEnvironment();

  // Check if the requested API environment was set.
  std::optional<std::string> api_request =
      env->GetVar(kSandboxEnvironmentApiRequest);
  EXPECT_TRUE(api_request.has_value());
  int api_request_num;
  EXPECT_TRUE(base::StringToInt(*api_request, &api_request_num));
  EXPECT_EQ(api_request_num, kSUIDSandboxApiNumber);

  // Now check if LD_PRELOAD was saved to SANDBOX_LD_PRELOAD.
  std::optional<std::string> sandbox_ld_preload =
      env->GetVar("SANDBOX_LD_PRELOAD");
  EXPECT_TRUE(sandbox_ld_preload.has_value());
  EXPECT_EQ(*sandbox_ld_preload, kTestValue);

  // Check that LD_ORIGIN_PATH was not saved.
  EXPECT_FALSE(env->HasVar("SANDBOX_LD_ORIGIN_PATH"));

  // We should not forget to restore LD_PRELOAD at the end, or this environment
  // variable will affect the next running tests!
  if (saved_ld_preload.has_value()) {
    EXPECT_TRUE(env->SetVar("LD_PRELOAD", *saved_ld_preload));
  } else {
    EXPECT_TRUE(env->UnSetVar("LD_PRELOAD"));
  }
}

// This test doesn't accomplish much, but will make sure that analysis tools
// will run this codepath.
TEST(SetuidSandboxHost, GetSandboxBinaryPath) {
  std::unique_ptr<SetuidSandboxHost> setuid_sandbox_host(
      SetuidSandboxHost::Create());
  std::ignore = setuid_sandbox_host->GetSandboxBinaryPath();
}

}  // namespace sandbox
