// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_PLATFORM_KEY_NSS_H_
#define NET_SSL_SSL_PLATFORM_KEY_NSS_H_

#include "base/memory/scoped_refptr.h"
#include "net/base/net_export.h"

typedef struct CERTCertificateStr CERTCertificate;

namespace crypto {
class CryptoModuleBlockingPasswordDelegate;
}

namespace net {

class SSLPrivateKey;
class X509Certificate;

// Returns an SSLPrivateKey backed by the NSS private key that corresponds to
// |certificate|'s public key. If |password_delegate| is non-null, it will be
// used to prompt for a password if necessary to unlock a slot or perform
// signing operations.
NET_EXPORT scoped_refptr<SSLPrivateKey> FetchClientCertPrivateKey(
    const X509Certificate* certificate,
    CERTCertificate* cert_certificate,
    scoped_refptr<crypto::CryptoModuleBlockingPasswordDelegate>
        password_delegate);

}  // namespace net

#endif  // NET_SSL_SSL_PLATFORM_KEY_NSS_H_
