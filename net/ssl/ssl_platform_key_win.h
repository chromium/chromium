// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_PLATFORM_KEY_WIN_H_
#define NET_SSL_SSL_PLATFORM_KEY_WIN_H_

#include <windows.h>

// Must be after windows.h.
#include <NCrypt.h>

#include "base/memory/scoped_refptr.h"
#include "base/win/wincrypt_shim.h"
#include "crypto/scoped_capi_types.h"
#include "crypto/scoped_cng_types.h"
#include "crypto/unexportable_key.h"
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
// correspond to |certificate|'s public key.
NET_EXPORT_PRIVATE scoped_refptr<SSLPrivateKey> WrapCAPIPrivateKey(
    const X509Certificate* certificate,
    crypto::ScopedHCRYPTPROV prov,
    DWORD key_spec);

// Returns an SSLPrivateKey backed by |key|, which must correspond to
// |certificate|'s public key, or nullptr on error.
NET_EXPORT_PRIVATE scoped_refptr<SSLPrivateKey> WrapCNGPrivateKey(
    const X509Certificate* certificate,
    crypto::ScopedNCryptKey key);

// Uses `key` to load a second NCrypt key handle and return an
// SSLPrivateKey making use of that new handle.
NET_EXPORT scoped_refptr<SSLPrivateKey> WrapUnexportableKeySlowly(
    const crypto::UnexportableSigningKey& key);

}  // namespace net

#endif  // NET_SSL_SSL_PLATFORM_KEY_WIN_H_
