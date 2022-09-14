// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/token_manager.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class TokenManagerTest : public ::testing::Test {
 public:
  TokenManagerTest() {
    // Generate server token.
    server_token_ = base::UnguessableToken::Create();
    manager_.AssignServerTokenForTesting(server_token_);

    // Generate tokens from trusted clients.
    for (const auto& type : TokenManager::kTrustedClientTypes) {
      auto& token = client_token_map_[type];
      token = base::UnguessableToken::Create();
      manager_.AssignClientTokenForTesting(type, token);
    }
  }

  ~TokenManagerTest() override = default;

 protected:
  TokenManager manager_;
  base::UnguessableToken server_token_;
  base::flat_map<cros::mojom::CameraClientType, base::UnguessableToken>
      client_token_map_;
};

// Test that TokenManager authenticates token for CameraHalServer.
TEST_F(TokenManagerTest, AuthenticateServer) {
  EXPECT_TRUE(manager_.AuthenticateServer(server_token_));
}

// Test that TokenManager authenticates token for CameraHalClient.
TEST_F(TokenManagerTest, AuthenticateClient) {
  for (auto type : TokenManager::kTrustedClientTypes) {
    auto authenticated_type =
        manager_.AuthenticateClient(type, client_token_map_[type]);
    ASSERT_TRUE(authenticated_type.has_value());
    EXPECT_EQ(authenticated_type.value(), type);

    // Verify that an empty token fails authentication.
    authenticated_type =
        manager_.AuthenticateClient(type, base::UnguessableToken());
    EXPECT_FALSE(authenticated_type.has_value());
  }
}

// Test that TokanManager authenticates token for pluginvm and the
// authentication fails when the token is unregistered.
TEST_F(TokenManagerTest, AuthenticatePluginvm) {
  // Create a fake token for pluginvm.
  auto token = base::UnguessableToken::Create();

  manager_.RegisterPluginVmToken(token);
  auto authenticated_type = manager_.AuthenticateClient(
      cros::mojom::CameraClientType::UNKNOWN, token);
  ASSERT_TRUE(authenticated_type.has_value());
  EXPECT_EQ(authenticated_type.value(),
            cros::mojom::CameraClientType::PLUGINVM);

  manager_.UnregisterPluginVmToken(token);
  authenticated_type = manager_.AuthenticateClient(
      cros::mojom::CameraClientType::UNKNOWN, token);
  EXPECT_FALSE(authenticated_type.has_value());
}

// Test that CameraClientType::UNKNOWN with an empty token is rejected.
TEST_F(TokenManagerTest, AuthenticateUnknown) {
  auto authenticated_type = manager_.AuthenticateClient(
      cros::mojom::CameraClientType::UNKNOWN, base::UnguessableToken::Create());
  EXPECT_FALSE(authenticated_type.has_value());
}

// Test that TokenManager::GetTokenForTrustedClient returns an empty token for
// untrusted clients.
TEST_F(TokenManagerTest, GetTokenForTrustedClientFailForUntrustedClients) {
  EXPECT_TRUE(
      manager_.GetTokenForTrustedClient(cros::mojom::CameraClientType::UNKNOWN)
          .is_empty());
  EXPECT_TRUE(
      manager_.GetTokenForTrustedClient(cros::mojom::CameraClientType::PLUGINVM)
          .is_empty());
}

}  // namespace media
