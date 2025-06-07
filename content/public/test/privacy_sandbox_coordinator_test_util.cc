// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/privacy_sandbox_coordinator_test_util.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/json/json_writer.h"
#include "base/strings/string_view_util.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "content/browser/interest_group/bidding_and_auction_server_key_fetcher.h"
#include "content/browser/interest_group/interest_group_auction.h"
#include "content/public/browser/interest_group_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace content {

std::string CreatePrivacySandboxCoordinatorSerializedPublicKeys(
    const url::Origin& coordinator,
    base::span<const url::Origin> origins) {
  base::Value::Dict origin_scoped_keys;
  for (const auto& origin : origins) {
    base::Value::Dict key;
    key.Set("key", base::Base64Encode(kTestPrivacySandboxCoordinatorPublicKey));
    key.Set("id", kTestPrivacySandboxCoordinatorIdString);
    base::Value::List keys;
    keys.Append(std::move(key));

    base::Value::Dict origin_dict;
    origin_dict.Set("keys", std::move(keys));

    origin_scoped_keys.Set(origin.Serialize(), std::move(origin_dict));
  }

  base::Value::Dict coordinator_response;
  coordinator_response.Set("originScopedKeys", std::move(origin_scoped_keys));

  std::optional<std::string> serialized_response =
      base::WriteJson(coordinator_response);
  CHECK(serialized_response);
  return std::move(serialized_response).value();
}

void ConfigureTestPrivacySandboxCoordinatorKeys(
    InterestGroupManager* interest_group_manager,
    InterestGroupManager::TrustedServerAPIType api_type,
    const url::Origin& coordinator,
    base::span<const url::Origin> origins) {
  base::test::TestFuture<std::optional<std::string>> future;
  interest_group_manager->AddTrustedServerKeysDebugOverride(
      api_type, coordinator,
      CreatePrivacySandboxCoordinatorSerializedPublicKeys(coordinator,
                                                          {origins}),
      future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get()) << *future.Get();
}

std::string GetTestPrivacySandboxCoordinatorPrivateKey() {
  return std::string(
      base::as_string_view(kTestPrivacySandboxCoordinatorPrivateKey));
}

}  // namespace content
