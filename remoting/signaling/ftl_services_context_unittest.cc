// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_services_context.h"

#include "remoting/proto/ftl/v1/ftl_messages.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(FtlServicesContextTest, GetBackoffPolicy) {
  const net::BackoffEntry::Policy& policy =
      FtlServicesContext::GetBackoffPolicy();
  EXPECT_EQ(policy.num_errors_to_ignore, 0);
  EXPECT_EQ(policy.initial_delay_ms,
            FtlServicesContext::kBackoffInitialDelay.InMilliseconds());
  EXPECT_EQ(policy.multiply_factor, 2);
  EXPECT_EQ(policy.jitter_factor, 0.5);
  EXPECT_EQ(policy.maximum_backoff_ms,
            FtlServicesContext::kBackoffMaxDelay.InMilliseconds());
  EXPECT_EQ(policy.entry_lifetime_ms, -1);
  EXPECT_FALSE(policy.always_use_initial_delay);
}

TEST(FtlServicesContextTest, GetChromotingAppIdentifier) {
  EXPECT_EQ(FtlServicesContext::GetChromotingAppIdentifier(), "CRD");
}

TEST(FtlServicesContextTest, CreateIdFromString) {
  ftl::Id id = FtlServicesContext::CreateIdFromString("test_id");
  EXPECT_EQ(id.id(), "test_id");
  EXPECT_EQ(id.app(), "CRD");
  EXPECT_EQ(id.type(), ftl::IdType_Type_EMAIL);
}

TEST(FtlServicesContextTest, CreateRequestHeader) {
  ftl::RequestHeader header =
      FtlServicesContext::CreateRequestHeader("test_token");
  EXPECT_FALSE(header.request_id().empty());
  EXPECT_EQ(header.app(), "CRD");
  EXPECT_EQ(header.auth_token_payload(), "test_token");
  EXPECT_TRUE(header.has_client_info());
  EXPECT_EQ(header.client_info().api_version(), ftl::ApiVersion_Value_V4);
}

TEST(FtlServicesContextTest, CreateRequestHeader_EmptyToken) {
  ftl::RequestHeader header = FtlServicesContext::CreateRequestHeader("");
  EXPECT_TRUE(header.auth_token_payload().empty());
}

}  // namespace remoting
