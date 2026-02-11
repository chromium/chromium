// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sandboxed_opaque_origin_creator.h"

#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"

namespace content {

class SandboxedOpaqueOriginCreatorTest : public ::testing::Test {
 protected:
  static base::PassKey<SandboxedOpaqueOriginCreatorTest> GetPassKey() {
    return base::PassKey<SandboxedOpaqueOriginCreatorTest>();
  }
};

TEST_F(SandboxedOpaqueOriginCreatorTest, SameNonceAndTupleProduceSameOrigin) {
  base::UnguessableToken nonce = base::UnguessableToken::Create();
  url::SchemeHostPort tuple("https", "a.com", 443);

  url::Origin origin1 =
      SandboxedOpaqueOriginCreator::CreateOriginForSandboxedFrameForTesting(
          GetPassKey(), nonce, tuple);
  url::Origin origin2 =
      SandboxedOpaqueOriginCreator::CreateOriginForSandboxedFrameForTesting(
          GetPassKey(), nonce, tuple);

  EXPECT_TRUE(origin1.opaque());
  EXPECT_TRUE(origin2.opaque());
  EXPECT_EQ(origin1, origin2);
}

TEST_F(SandboxedOpaqueOriginCreatorTest,
       DifferentNoncesProduceDifferentOrigins) {
  base::UnguessableToken nonce1 = base::UnguessableToken::Create();
  base::UnguessableToken nonce2 = base::UnguessableToken::Create();
  url::SchemeHostPort tuple("https", "a.com", 443);

  url::Origin origin1 =
      SandboxedOpaqueOriginCreator::CreateOriginForSandboxedFrameForTesting(
          GetPassKey(), nonce1, tuple);
  url::Origin origin2 =
      SandboxedOpaqueOriginCreator::CreateOriginForSandboxedFrameForTesting(
          GetPassKey(), nonce2, tuple);

  EXPECT_TRUE(origin1.opaque());
  EXPECT_TRUE(origin2.opaque());
  EXPECT_NE(origin1, origin2);
}

TEST_F(SandboxedOpaqueOriginCreatorTest,
       DifferentTuplesProduceDifferentOrigins) {
  base::UnguessableToken nonce = base::UnguessableToken::Create();
  url::SchemeHostPort tuple1("https", "a.com", 443);
  url::SchemeHostPort tuple2("https", "b.com", 443);

  url::Origin origin1 =
      SandboxedOpaqueOriginCreator::CreateOriginForSandboxedFrameForTesting(
          GetPassKey(), nonce, tuple1);
  url::Origin origin2 =
      SandboxedOpaqueOriginCreator::CreateOriginForSandboxedFrameForTesting(
          GetPassKey(), nonce, tuple2);

  EXPECT_TRUE(origin1.opaque());
  EXPECT_TRUE(origin2.opaque());
  EXPECT_NE(origin1, origin2);
}

TEST_F(SandboxedOpaqueOriginCreatorTest, CreateOpaqueOriginsForVariousUrls) {
  struct TestCase {
    const char* url;
    bool is_opaque;
  };

  const TestCase cases[] = {
      {"http://a.com", false},
      {"https://a.com", false},
      {"data:text/html,foo", true},
      {"about:blank", true},
      {"about:srcdoc", true},
      {"blob:https://a.com/foo", false},
      {"", true},
  };

  for (const auto& test_case : cases) {
    base::UnguessableToken nonce = base::UnguessableToken::Create();
    url::Origin creator_origin = url::Origin::Create(GURL(test_case.url));

    url::Origin origin =
        SandboxedOpaqueOriginCreator::CreateOriginForSandboxedFrameForTesting(
            GetPassKey(), nonce,
            creator_origin.GetTupleOrPrecursorTupleIfOpaque());

    EXPECT_TRUE(origin.opaque());

    // Verify precursor tuple is preserved from the input origin.
    EXPECT_EQ(origin.GetTupleOrPrecursorTupleIfOpaque().IsValid(),
              !test_case.is_opaque);
  }
}

}  // namespace content
