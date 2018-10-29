// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_PLATFORM_KEY_WIN_H_
#define NET_SSL_SSL_PLATFORM_KEY_WIN_H_

#include <windows.h>

// Must be after windows.h.
#include <NCrypt.h>

#include "base/memory/ref_counted.h"
#include "base/win/wincrypt_shim.h"
#include "net/base/net_export.h"

namespace net {

class SSLPrivateKey;
class X509Certificate;

// Returns an SSLPrivateKey backed by the platform private key for
// |cert_context| which must correspond to |certificate|.
scoped_refptr<SSLPrivateKey> FetchClientCertPrivateKey(
    const X509Certificate* certificate,
    PCCERT_CONTEXT cert_context);

// Returns an SSLPrivateKey backed by |prov| and |key_spec|, which must
// correspond to |certificate|'s public key. Takes ownership of |prov|.
NET_EXPORT_PRIVATE scoped_refptr<SSLPrivateKey> WrapCAPIPrivateKey(
    const X509Certificate* certificate,
    HCRYPTPROV prov,
    DWORD key_spec);

// Returns an SSLPrivateKey backed by |key|, which must correspond to
// |certificate|'s public key, or nullptr on error. Takes ownership of |key| in
// both cases.
NET_EXPORT_PRIVATE scoped_refptr<SSLPrivateKey> WrapCNGPrivateKey(
    const X509Certificate* certificate,
    NCRYPT_KEY_HANDLE key);

}  // namespace net

#endif  // NET_SSL_SSL_PLATFORM_KEY_WIN_H_
