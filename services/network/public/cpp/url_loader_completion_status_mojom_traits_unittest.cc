// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/mojom/url_loader_completion_status.mojom.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/host_port_pair.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/cors.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

TEST(URLLoaderCompletionStatusMojomTraitsTest, MojoRoundTrip) {
  URLLoaderCompletionStatus original;
  original.error_code = 1;
  original.extended_error_code = 2;
  original.exists_in_cache = true;
  original.completion_time += base::Minutes(3);
  original.encoded_data_length = 4;
  original.encoded_body_length = 5;
  original.decoded_body_length = 6;
  original.cors_error_status =
      CorsErrorStatus(mojom::CorsError::kInsecurePrivateNetwork);
  original.trust_token_operation_status =
      mojom::TrustTokenOperationStatus::kInvalidArgument;
  original.blocked_by_response_reason =
      mojom::BlockedByResponseReason::kCorpNotSameOrigin;
  original.should_report_orb_blocking = true;
  original.resolve_error_info = net::ResolveErrorInfo(7, true);
  original.should_collapse_initiator = true;

  original.ssl_info = net::SSLInfo();
  original.ssl_info->is_issued_by_known_root = true;

  URLLoaderCompletionStatus copy;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::URLLoaderCompletionStatus>(
          original, copy));
  EXPECT_EQ(original, copy);
}

}  // namespace
}  // namespace network
