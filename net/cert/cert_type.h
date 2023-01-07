// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CERT_TYPE_H_
#define NET_CERT_CERT_TYPE_H_

namespace net {

// Constants to classify the type of a certificate.
// This is only used in the context of CertDatabase, but is defined outside to
// avoid an awkwardly long type name.
// The type is a combination of intrinsic properties, such as the presense of an
// Certificate Authority Basic Constraint, and assigned trust values.  For
// example, a cert with no basic constraints or trust would be classified as
// UNKNOWN_CERT.  If that cert is then trusted with SetCertTrust(cert,
// SERVER_CERT, TRUSTED_SSL), it would become a SERVER_CERT.
enum CertType {
  OTHER_CERT,
  CA_CERT,
  USER_CERT,
  SERVER_CERT,
  NUM_CERT_TYPES
};

}  // namespace net

#endif  // NET_CERT_CERT_TYPE_H_
