// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CT_SCT_TO_STRING_H_
#define NET_CERT_CT_SCT_TO_STRING_H_

#include <string>

#include "net/base/net_export.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/signed_certificate_timestamp.h"

// Functions for converting non-string attributes of
// net::ct::SignedCertificateTimestamp and net::ct::SCTVerifyStatus values to
// strings.
namespace net::ct {

// Returns a textual representation of |hash_algorithm|.
NET_EXPORT const std::string HashAlgorithmToString(
    DigitallySigned::HashAlgorithm hashAlgorithm);

// Returns a textual representation of |origin|.
NET_EXPORT const std::string OriginToString(
    SignedCertificateTimestamp::Origin origin);

// Returns a textual representation of |signatureAlgorithm|.
NET_EXPORT const std::string SignatureAlgorithmToString(
    DigitallySigned::SignatureAlgorithm signatureAlgorithm);

// Returns a textual representation of |status|.
NET_EXPORT const std::string StatusToString(SCTVerifyStatus status);

}  // namespace net::ct

#endif  // NET_CERT_CT_SCT_TO_STRING_H_
