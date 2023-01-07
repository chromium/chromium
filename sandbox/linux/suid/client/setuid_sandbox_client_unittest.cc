// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/suid/client/setuid_sandbox_client.h"

#include <memory>

#include "base/environment.h"
#include "base/strings/string_number_conversions.h"
#include "sandbox/linux/suid/common/sandbox.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

TEST(SetuidSandboxClient, SandboxedClientAPI) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  EXPECT_TRUE(env != NULL);

  std::unique_ptr<SetuidSandboxClient> sandbox_client(
      SetuidSandboxClient::Create());
  EXPECT_TRUE(sandbox_client != NULL);

  // Set-up a fake environment as if we went through the setuid sandbox.
  EXPECT_TRUE(env->SetVar(kSandboxEnvironmentApiProvides,
                          base::NumberToString(kSUIDSandboxApiNumber)));
  EXPECT_TRUE(env->SetVar(kSandboxDescriptorEnvironmentVarName, "1"));
  EXPECT_TRUE(env->SetVar(kSandboxPIDNSEnvironmentVarName, "1"));
  EXPECT_TRUE(env->UnSetVar(kSandboxNETNSEnvironmentVarName));

  // Check the API.
  EXPECT_TRUE(sandbox_client->IsSuidSandboxUpToDate());
  EXPECT_TRUE(sandbox_client->IsSuidSandboxChild());
  EXPECT_TRUE(sandbox_client->IsInNewPIDNamespace());
  EXPECT_FALSE(sandbox_client->IsInNewNETNamespace());

  // Forge an incorrect API version and check.
  EXPECT_TRUE(env->SetVar(kSandboxEnvironmentApiProvides,
                          base::NumberToString(kSUIDSandboxApiNumber + 1)));
  EXPECT_FALSE(sandbox_client->IsSuidSandboxUpToDate());
  // We didn't go through the actual sandboxing mechanism as it is
  // very hard in a unit test.
  EXPECT_FALSE(sandbox_client->IsSandboxed());
}

}  // namespace sandbox

