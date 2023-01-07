// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_KNOWN_ROOTS_WIN_H_
#define NET_CERT_KNOWN_ROOTS_WIN_H_

#include "base/win/wincrypt_shim.h"
#include "net/base/net_export.h"

namespace net {

// IsKnownRoot returns true if the given certificate is one that we believe
// is a standard (as opposed to user-installed) root.
NET_EXPORT_PRIVATE bool IsKnownRoot(PCCERT_CONTEXT cert);

}  // namespace net

#endif  // NET_CERT_KNOWN_ROOTS_WIN_H_
