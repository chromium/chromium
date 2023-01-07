// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_KNOWN_ROOTS_NSS_H_
#define NET_CERT_KNOWN_ROOTS_NSS_H_

#include "net/base/net_export.h"

typedef struct CERTCertificateStr CERTCertificate;

namespace net {

// IsKnownRoot returns true if the given certificate is one that we believe
// is a standard (as opposed to user-installed) root.
NET_EXPORT_PRIVATE bool IsKnownRoot(CERTCertificate* root);

}  // namespace net

#endif  // NET_CERT_KNOWN_ROOTS_NSS_H_
