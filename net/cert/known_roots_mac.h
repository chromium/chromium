// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_KNOWN_ROOTS_MAC_H_
#define NET_CERT_KNOWN_ROOTS_MAC_H_

#include <Security/Security.h>

#include "net/base/hash_value.h"
#include "net/base/net_export.h"

namespace net {

// IsKnownRoot returns true if the given certificate is one that we believe
// is a standard (as opposed to user-installed) root.
//
// NOTE: The first call will lazily initialize the known roots, which requires
// holding crypto::GetMacSecurityServicesLock(). Callers must therefore either
// acquire that lock prior to calling this, or eagerly initialize beforehand
// using InitializeKnownRoots().
NET_EXPORT_PRIVATE bool IsKnownRoot(SecCertificateRef cert);
bool IsKnownRoot(const HashValue& cert_sha256);

// Calling this is optional as initialization will otherwise be done lazily when
// calling IsKnownRoot(). When calling this, the current thread must NOT already
// be holding/ crypto::GetMacSecurityServicesLock().
void InitializeKnownRoots();

}  // namespace net

#endif  // NET_CERT_KNOWN_ROOTS_MAC_H_
