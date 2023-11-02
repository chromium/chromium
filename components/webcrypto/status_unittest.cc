// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/status.h"
#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/algorithms/test_helpers.h"
#include "third_party/blink/public/platform/web_crypto_algorithm.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"

namespace webcrypto {

namespace {

// Tests several Status objects against their expected hard coded values, as
// well as ensuring that comparison of Status objects works.
// Comparison should take into account both the error details, as well as the
// error type.
TEST(WebCryptoStatusTest, Basic) {
  // Even though the error message is the same, these should not be considered
  // the same by the tests because the error type is different.
  EXPECT_NE(Status::DataError(), Status::OperationError());
  EXPECT_NE(Status::Success(), Status::OperationError());

  EXPECT_EQ(Status::Success(), Status::Success());
  EXPECT_EQ(Status::ErrorJwkMemberWrongType("kty", "string"),
            Status::ErrorJwkMemberWrongType("kty", "string"));

  Status status = Status::Success();

  EXPECT_FALSE(status.IsError());
  EXPECT_EQ("", status.error_details());

  status = Status::OperationError();
  EXPECT_TRUE(status.IsError());
  EXPECT_EQ("", status.error_details());
  EXPECT_EQ(blink::kWebCryptoErrorTypeOperation, status.error_type());

  status = Status::DataError();
  EXPECT_TRUE(status.IsError());
  EXPECT_EQ("", status.error_details());
  EXPECT_EQ(blink::kWebCryptoErrorTypeData, status.error_type());

  status = Status::ErrorUnsupported();
  EXPECT_TRUE(status.IsError());
  EXPECT_EQ("The requested operation is unsupported", status.error_details());
  EXPECT_EQ(blink::kWebCryptoErrorTypeNotSupported, status.error_type());

  status = Status::ErrorJwkMemberMissing("kty");
  EXPECT_TRUE(status.IsError());
  EXPECT_EQ("The required JWK member \"kty\" was missing",
            status.error_details());
  EXPECT_EQ(blink::kWebCryptoErrorTypeData, status.error_type());

  status = Status::ErrorJwkMemberWrongType("kty", "string");
  EXPECT_TRUE(status.IsError());
  EXPECT_EQ("The JWK member \"kty\" must be a string", status.error_details());
  EXPECT_EQ(blink::kWebCryptoErrorTypeData, status.error_type());

  status = Status::ErrorJwkBase64Decode("n");
  EXPECT_TRUE(status.IsError());
  EXPECT_EQ(
      "The JWK member \"n\" could not be base64url decoded or contained "
      "padding",
      status.error_details());
  EXPECT_EQ(blink::kWebCryptoErrorTypeData, status.error_type());
}

}  // namespace

}  // namespace webcrypto
