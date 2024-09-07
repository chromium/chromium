// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/do_nothing_ct_verifier.h"

#include <string_view>

#include "net/base/net_errors.h"

namespace net {

DoNothingCTVerifier::DoNothingCTVerifier() = default;
DoNothingCTVerifier::~DoNothingCTVerifier() = default;

void DoNothingCTVerifier::Verify(
    X509Certificate* cert,
    std::string_view stapled_ocsp_response,
    std::string_view sct_list_from_tls_extension,
    base::Time current_time,
    SignedCertificateTimestampAndStatusList* output_scts,
    const NetLogWithSource& net_log) const {
  output_scts->clear();
}

}  // namespace net
