// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_CERT_UTILS_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_CERT_UTILS_IMPL_H_

#include "base/strings/abseil_string_conversions.h"
#include "net/cert/asn1_util.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_string_piece.h"

namespace quic {

class QuicCertUtilsImpl {
 public:
  static bool ExtractSubjectNameFromDERCert(
      quiche::QuicheStringPiece cert,
      quiche::QuicheStringPiece* subject_out) {
    base::StringPiece out;
    bool result = net::asn1::ExtractSubjectFromDERCert(
        base::StringViewToStringPiece(cert), &out);
    *subject_out = base::StringPieceToStringView(out);
    return result;
  }
};

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_CERT_UTILS_IMPL_H_
