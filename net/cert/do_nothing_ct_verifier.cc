// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/do_nothing_ct_verifier.h"

#include "net/base/net_errors.h"

namespace net {

DoNothingCTVerifier::DoNothingCTVerifier() = default;
DoNothingCTVerifier::~DoNothingCTVerifier() = default;

void DoNothingCTVerifier::Verify(
    base::StringPiece hostname,
    X509Certificate* cert,
    base::StringPiece stapled_ocsp_response,
    base::StringPiece sct_list_from_tls_extension,
    SignedCertificateTimestampAndStatusList* output_scts,
    const NetLogWithSource& net_log) {
  output_scts->clear();
}

}  // namespace net
