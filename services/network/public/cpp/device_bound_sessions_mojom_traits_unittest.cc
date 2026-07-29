// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/device_bound_sessions_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/device_bound_sessions/refresh_result.h"
#include "services/network/public/mojom/device_bound_sessions.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

using SessionParams = net::device_bound_sessions::SessionParams;

TEST(DeviceBoundSessionsMojomTraitsTest, SerializeAndDeserializeSessionParams) {
  SessionParams input{
      .session_id = "session_id",
      .fetcher_url = GURL("https://example.com/registration"),
      .refresh_url = "/refresh",
      .scope =
          {
              .include_site = true,
              .specifications = {{
                  .type = SessionParams::Scope::Specification::Type::kExclude,
                  .domain = "*.example.com",
                  .path = "/excluded_path",
              }},
              .origin = "https://example.com",
          },
      .credentials = {{.name = "test_cookie", .attributes = "secure"}},
      .allowed_refresh_initiators = {"*.allowed-refresh-initiator.com",
                                     "not-subdomains.com"},
  };
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
  // `UnexportableSigningKeyId`s are not currently serialized/deserialized.
  EXPECT_EQ(input.allowed_refresh_initiators,
            output.allowed_refresh_initiators);
}

TEST(DeviceBoundSessionsMojomTraitsTest, SerializeAndDeserializeRefreshResult) {
  net::device_bound_sessions::RefreshResult input =
      net::device_bound_sessions::RefreshResult::kInScopeRefreshNotYetNeeded;
  net::device_bound_sessions::RefreshResult output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              network::mojom::DeviceBoundSessionRefreshResult>(input, output));
  EXPECT_EQ(input, output);
}

}  // namespace
}  // namespace mojo
