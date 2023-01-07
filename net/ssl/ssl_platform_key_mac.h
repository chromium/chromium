// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_PLATFORM_KEY_MAC_H_
#define NET_SSL_SSL_PLATFORM_KEY_MAC_H_

#include <Security/SecBase.h>

#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"

namespace net {

class SSLPrivateKey;
class X509Certificate;

// Returns an SSLPrivateKey backed by the platform private key in |identity|
// which must correspond to |certificate|'s public key.
NET_EXPORT scoped_refptr<SSLPrivateKey> CreateSSLPrivateKeyForSecIdentity(
    const X509Certificate* certificate,
    SecIdentityRef identity);

}  // namespace net

#endif  // NET_SSL_SSL_PLATFORM_KEY_MAC_H_
