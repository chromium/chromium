// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/resource_sync_token_client.h"

#include <vector>

#include "components/viz/test/test_gles2_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

gpu::SyncToken CreateSyncToken(int value) {
  return gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO,
                        gpu::CommandBufferId::FromUnsafeValue(value), 0);
}

class SyncTokenTestInterface : public viz::TestGLES2Interface {
 public:
  void GenSyncTokenCHROMIUM(GLbyte* sync_token) override {
    viz::TestGLES2Interface::GenSyncTokenCHROMIUM(sync_token);
    gpu::SyncToken sync_token_data;
    memcpy(sync_token_data.GetData(), sync_token, sizeof(sync_token_data));
    generated_tokens_.push_back(sync_token_data);
  }

  void WaitSyncTokenCHROMIUM(const GLbyte* sync_token) override {
    gpu::SyncToken sync_token_data;
    memcpy(sync_token_data.GetData(), sync_token, sizeof(sync_token_data));
    viz::TestGLES2Interface::WaitSyncTokenCHROMIUM(sync_token);
    waited_tokens_.push_back(sync_token_data);
  }

  const std::vector<gpu::SyncToken>& generated_tokens() const {
    return generated_tokens_;
  }
  const std::vector<gpu::SyncToken>& waited_tokens() const {
    return waited_tokens_;
  }

 private:
  std::vector<gpu::SyncToken> generated_tokens_;
  std::vector<gpu::SyncToken> waited_tokens_;
};

}  // namespace

namespace media {

// Test that no additional work is triggered when receiving a duplicated
// SyncToken.
TEST(ResourceSyncTokenClientTest, DuplicateToken) {
  SyncTokenTestInterface gl;

  auto token = CreateSyncToken(0x1234);
  ResourceSyncTokenClient client(&gl, token, token);
  client.WaitSyncToken(token);
  EXPECT_TRUE(gl.generated_tokens().empty());
  EXPECT_TRUE(gl.waited_tokens().empty());

  gpu::SyncToken generated_token;
  client.GenerateSyncToken(&generated_token);
  EXPECT_EQ(generated_token, token);
  EXPECT_TRUE(gl.generated_tokens().empty());
  EXPECT_TRUE(gl.waited_tokens().empty());
}

// Test that no additional work is triggered when the token waited on matches
// the original token.
TEST(ResourceSyncTokenClientTest, MatchesOriginalToken) {
  SyncTokenTestInterface gl;

  auto original_token = CreateSyncToken(0xDEED);
  auto token = CreateSyncToken(0x1234);
  ResourceSyncTokenClient client(&gl, original_token, token);
  client.WaitSyncToken(original_token);
  EXPECT_TRUE(gl.generated_tokens().empty());
  EXPECT_TRUE(gl.waited_tokens().empty());

  gpu::SyncToken generated_token;
  client.GenerateSyncToken(&generated_token);
  EXPECT_EQ(generated_token, token);
  EXPECT_TRUE(gl.generated_tokens().empty());
  EXPECT_TRUE(gl.waited_tokens().empty());
}

// Test that the appropriate waits and token generation happen when the
// token waited upon is different than the original token.
TEST(ResourceSyncTokenClientTest, NewFrameToken) {
  SyncTokenTestInterface gl;

  auto original_token = CreateSyncToken(0xDEED);
  auto token = CreateSyncToken(0x1234);
  ResourceSyncTokenClient client(&gl, original_token, token);

  auto new_token = CreateSyncToken(0xBEED);
  client.WaitSyncToken(new_token);
  ASSERT_EQ(gl.waited_tokens().size(), 2u);
  EXPECT_EQ(gl.waited_tokens()[0], new_token);
  EXPECT_EQ(gl.waited_tokens()[1], token);

  gpu::SyncToken generated_token;
  client.GenerateSyncToken(&generated_token);
  EXPECT_NE(generated_token, original_token);
  EXPECT_NE(generated_token, token);
  EXPECT_NE(generated_token, new_token);
  ASSERT_EQ(gl.generated_tokens().size(), 1u);
  EXPECT_EQ(gl.generated_tokens()[0], generated_token);
}

}  // namespace media
