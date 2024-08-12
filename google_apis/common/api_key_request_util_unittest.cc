// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/common/api_key_request_util.h"

#include "base/test/gtest_util.h"
#include "base/version_info/channel.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {
namespace {

constexpr char kApiKeyHeaderName[] = "X-Goog-Api-Key";

class APIKeyRequestUtilTest : public testing::Test {
 public:
  APIKeyRequestUtilTest() = default;
  ~APIKeyRequestUtilTest() override = default;
};

TEST_F(APIKeyRequestUtilTest, AddDefaultAPIKeyToRequest) {
  network::ResourceRequest request;

  AddDefaultAPIKeyToRequest(request, version_info::Channel::STABLE);

  ASSERT_TRUE(request.headers.HasHeader(kApiKeyHeaderName));
  ASSERT_EQ(request.headers.GetHeader(kApiKeyHeaderName),
            GetAPIKey(version_info::Channel::STABLE));
}

TEST_F(APIKeyRequestUtilTest, AddAPIKeyToRequest) {
  network::ResourceRequest request;

  AddAPIKeyToRequest(request, "test_api_key");

  ASSERT_TRUE(request.headers.HasHeader(kApiKeyHeaderName));
  ASSERT_EQ(request.headers.GetHeader(kApiKeyHeaderName), "test_api_key");
}

#if DCHECK_IS_ON()
TEST_F(APIKeyRequestUtilTest, AddAPIKeyToRequest_EmptyKey) {
  network::ResourceRequest request;

  // This is a misuse of the API, enforced by a DLOG(FATAL) in tests.
  EXPECT_DEATH_IF_SUPPORTED(AddAPIKeyToRequest(request, ""),
                            "API key cannot be empty.");
}

TEST_F(APIKeyRequestUtilTest, AddAPIKeyToRequest_ExistingHeader) {
  network::ResourceRequest request;
  request.headers.SetHeader(kApiKeyHeaderName, "existing_api_key");

  // This is a misuse of the API, enforced by a DLOG(FATAL) in tests.
  EXPECT_DEATH_IF_SUPPORTED(AddAPIKeyToRequest(request, "test_api_key"),
                            "API key already present in request header.");
}
#endif  // DCHECK_IS_ON()

}  // namespace
}  // namespace google_apis
