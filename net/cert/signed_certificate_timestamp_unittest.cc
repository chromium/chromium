// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/signed_certificate_timestamp.h"

#include <string>

#include "base/pickle.h"
#include "net/test/ct_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::ct {

namespace {

const char kLogDescription[] = "somelog";

class SignedCertificateTimestampTest : public ::testing::Test {
 public:
  void SetUp() override {
    GetX509CertSCT(&sample_sct_);
    sample_sct_->origin = SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE;
    sample_sct_->log_description = kLogDescription;
  }

 protected:
  scoped_refptr<SignedCertificateTimestamp> sample_sct_;
};

TEST_F(SignedCertificateTimestampTest, PicklesAndUnpickles) {
  base::Pickle pickle;

  sample_sct_->Persist(&pickle);
  base::PickleIterator iter(pickle);

  scoped_refptr<SignedCertificateTimestamp> unpickled_sct(
      SignedCertificateTimestamp::CreateFromPickle(&iter));

  SignedCertificateTimestamp::LessThan less_than;

  ASSERT_FALSE(less_than(sample_sct_, unpickled_sct));
  ASSERT_FALSE(less_than(unpickled_sct, sample_sct_));
  ASSERT_EQ(sample_sct_->origin, unpickled_sct->origin);
  ASSERT_EQ(sample_sct_->log_description, unpickled_sct->log_description);
}

TEST_F(SignedCertificateTimestampTest, SCTsWithDifferentOriginsNotEqual) {
  scoped_refptr<SignedCertificateTimestamp> another_sct;
  GetX509CertSCT(&another_sct);
  another_sct->origin = SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION;

  SignedCertificateTimestamp::LessThan less_than;

  ASSERT_TRUE(less_than(sample_sct_, another_sct) ||
              less_than(another_sct, sample_sct_));
}

}  // namespace

}  // namespace net::ct
