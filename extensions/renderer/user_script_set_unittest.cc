// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/user_script_set.h"

#include "base/containers/span.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/pickle.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/user_script.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/test_extensions_renderer_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class UserScriptSetTest : public testing::Test {
 public:
  UserScriptSetTest() {
    ExtensionsRendererClient::Set(&extensions_renderer_client_);
  }
  ~UserScriptSetTest() override { ExtensionsRendererClient::Set(nullptr); }

 protected:
  TestExtensionsRendererClient extensions_renderer_client_;
};

// Regression test for crbug.com/493225428.
TEST_F(UserScriptSetTest, UpdateUserScripts_FailureClearsScripts) {
  mojom::HostID host_id(mojom::HostID::HostType::kExtensions, "extension_id");
  UserScriptSet user_script_set(host_id);

  // 1. Create a valid shared memory region with one script.
  base::Pickle pickle;
  pickle.WriteUInt32(1);  // num_scripts

  UserScript script;
  script.set_id("script_id");
  script.Pickle(&pickle);

  // No JS or CSS scripts for simplicity.

  base::MappedReadOnlyRegion mapped_region =
      base::ReadOnlySharedMemoryRegion::Create(pickle.size());
  ASSERT_TRUE(mapped_region.IsValid());

  mapped_region.mapping.GetMemoryAsSpan<uint8_t>()
      .first(pickle.size())
      .copy_from(pickle.AsBytes());

  // 2. Update with valid scripts.

  EXPECT_TRUE(
      user_script_set.UpdateUserScripts(std::move(mapped_region.region)));
  EXPECT_TRUE(user_script_set.HasScripts());

  // 3. Attempt to update with an INVALID shared memory region.
  // This should return false and clear the scripts.
  EXPECT_FALSE(
      user_script_set.UpdateUserScripts(base::ReadOnlySharedMemoryRegion()));

  // 4. Verify that HasScripts() is false if update failed.
  EXPECT_FALSE(user_script_set.HasScripts())
      << "Scripts should be cleared after a failed update!";
}

}  // namespace extensions
