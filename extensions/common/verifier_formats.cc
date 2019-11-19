// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/verifier_formats.h"

#include "components/crx_file/crx_verifier.h"

namespace extensions {

crx_file::VerifierFormat GetWebstoreVerifierFormat(
    bool test_publisher_enabled) {
  return test_publisher_enabled
             ? crx_file::VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF
             : crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF;
}

crx_file::VerifierFormat GetPolicyVerifierFormat() {
  return crx_file::VerifierFormat::CRX3;
}

crx_file::VerifierFormat GetExternalVerifierFormat() {
  return crx_file::VerifierFormat::CRX3;
}

crx_file::VerifierFormat GetTestVerifierFormat() {
  return crx_file::VerifierFormat::CRX3;
}

}  // namespace extensions
