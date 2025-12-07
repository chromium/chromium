// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/device_bound_sessions_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/device_bound_sessions.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

using SessionParams = net::device_bound_sessions::SessionParams;

TEST(DeviceBoundSessionsMojomTraitsTest, SerializeAndDeserializeSessionParams) {
  SessionParams::Scope scope;
  scope.include_site = true;
  scope.specifications.push_back(
      {SessionParams::Scope::Specification::Type::kExclude, "*.example.com",
       "/excluded_path"});
  scope.origin = "https://example.com";

  SessionParams input(
      "session_id", GURL("https://example.com/registration"), "/refresh",
      std::move(scope), {{"cookie_name", "Secure; SameSite=Lax"}},
      unexportable_keys::UnexportableKeyId(),
      {"*.allowed-refresh-initiator.com", "not-subdomains.com"});
  SessionParams output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              network::mojom::DeviceBoundSessionParams>(input, output));

  EXPECT_EQ(input.session_id, output.session_id);
  EXPECT_EQ(input.fetcher_url, output.fetcher_url);
  EXPECT_EQ(input.refresh_url, output.refresh_url);
  EXPECT_EQ(input.scope.include_site, output.scope.include_site);
  EXPECT_EQ(input.scope.specifications, output.scope.specifications);
  EXPECT_EQ(input.scope.origin, output.scope.origin);
  EXPECT_EQ(input.credentials, output.credentials);
  // `UnexportableKeyId`s are not currently serialized/deserialized.
  EXPECT_EQ(input.allowed_refresh_initiators,
            output.allowed_refresh_initiators);
}

}  // namespace
}  // namespace mojo
