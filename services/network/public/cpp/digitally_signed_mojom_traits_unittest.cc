// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/digitally_signed_mojom_traits.h"

#include "base/test/gtest_util.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "services/network/public/mojom/digitally_signed.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

TEST(DigitallySignedTraitsTest, Roundtrips) {
  for (auto sig_alg : {net::ct::DigitallySigned::SIG_ALGO_ANONYMOUS,
                       net::ct::DigitallySigned::SIG_ALGO_RSA,
                       net::ct::DigitallySigned::SIG_ALGO_DSA,
                       net::ct::DigitallySigned::SIG_ALGO_ECDSA}) {
    for (auto hash_alg : {net::ct::DigitallySigned::HASH_ALGO_NONE,
                          net::ct::DigitallySigned::HASH_ALGO_MD5,
                          net::ct::DigitallySigned::HASH_ALGO_SHA1,
                          net::ct::DigitallySigned::HASH_ALGO_SHA224,
                          net::ct::DigitallySigned::HASH_ALGO_SHA256,
                          net::ct::DigitallySigned::HASH_ALGO_SHA384,
                          net::ct::DigitallySigned::HASH_ALGO_SHA512}) {
      net::ct::DigitallySigned original;
      original.hash_algorithm = hash_alg;
      original.signature_algorithm = sig_alg;
      original.signature_data.assign(5, static_cast<char>(hash_alg));

      net::ct::DigitallySigned copied;
      EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::DigitallySigned>(
          original, copied))
          << "with hash " << hash_alg << " and sig " << sig_alg;
      EXPECT_EQ(original.hash_algorithm, copied.hash_algorithm);
      EXPECT_EQ(original.signature_algorithm, copied.signature_algorithm);
      EXPECT_EQ(original.signature_data, copied.signature_data);
    }
  }
}

TEST(DigitallySignedTraitsTest, EmptySignatureRejected) {
  net::ct::DigitallySigned original;
  original.hash_algorithm = net::ct::DigitallySigned::HASH_ALGO_SHA256;
  original.signature_algorithm = net::ct::DigitallySigned::SIG_ALGO_ECDSA;
  original.signature_data.clear();

  net::ct::DigitallySigned copied;
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::DigitallySigned>(
      original, copied));
}

TEST(DigitallySignedTraitsTest, OutOfBoundsEnumsRejected) {
  net::ct::DigitallySigned original;
  original.hash_algorithm =
      static_cast<net::ct::DigitallySigned::HashAlgorithm>(-1);
  original.signature_algorithm = net::ct::DigitallySigned::SIG_ALGO_ECDSA;
  original.signature_data.assign(32, '\x01');

  net::ct::DigitallySigned copied;
  EXPECT_DCHECK_DEATH(
      mojo::test::SerializeAndDeserialize<mojom::DigitallySigned>(original,
                                                                  copied));
}

}  // namespace
}  // namespace network
