// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_SCOPED_CAPI_TYPES_H_
#define CRYPTO_SCOPED_CAPI_TYPES_H_

#include <windows.h>

#include <memory>

#include "base/check.h"
#include "base/scoped_generic.h"
#include "base/win/wincrypt_shim.h"

namespace crypto {

// Simple traits for the Free family of CryptoAPI functions, such as
// CryptDestroyHash, which take only a single argument to release.
template <typename CAPIHandle, BOOL(WINAPI* Destroyer)(CAPIHandle)>
struct CAPITraits {
  static CAPIHandle InvalidValue() { return 0; }
  static void Free(CAPIHandle handle) {
    BOOL ok = Destroyer(handle);
    DCHECK(ok);
  }
};

// Traits for the Close/Release family of CryptoAPI functions, which take
// a second DWORD parameter indicating flags to use when closing or releasing.
// This includes functions like CertCloseStore or CryptReleaseContext.
template <typename CAPIHandle,
          BOOL(WINAPI* Destroyer)(CAPIHandle, DWORD),
          DWORD flags>
struct CAPITraitsWithFlags {
  static CAPIHandle InvalidValue() { return 0; }
  static void Free(CAPIHandle handle) {
    BOOL ok = Destroyer(handle, flags);
    DCHECK(ok);
  }
};

using ScopedHCERTSTORE =
    base::ScopedGeneric<HCERTSTORE,
                        CAPITraitsWithFlags<HCERTSTORE, CertCloseStore, 0>>;

using ScopedHCRYPTPROV = base::ScopedGeneric<
    HCRYPTPROV,
    CAPITraitsWithFlags<HCRYPTPROV, CryptReleaseContext, 0>>;

using ScopedHCRYPTKEY =
    base::ScopedGeneric<HCRYPTKEY, CAPITraits<HCRYPTKEY, CryptDestroyKey>>;

using ScopedHCRYPTHASH =
    base::ScopedGeneric<HCRYPTHASH, CAPITraits<HCRYPTHASH, CryptDestroyHash>>;

using ScopedHCRYPTMSG =
    base::ScopedGeneric<HCRYPTMSG, CAPITraits<HCRYPTMSG, CryptMsgClose>>;

struct ChainEngineTraits {
  static HCERTCHAINENGINE InvalidValue() { return nullptr; }
  static void Free(HCERTCHAINENGINE engine) {
    CertFreeCertificateChainEngine(engine);
  }
};

using ScopedHCERTCHAINENGINE =
    base::ScopedGeneric<HCERTCHAINENGINE, ChainEngineTraits>;

struct FreeCertContextFunctor {
  void operator()(PCCERT_CONTEXT context) const {
    if (context)
      CertFreeCertificateContext(context);
  }
};

using ScopedPCCERT_CONTEXT =
    std::unique_ptr<const CERT_CONTEXT, FreeCertContextFunctor>;

struct FreeCertChainContextFunctor {
  void operator()(PCCERT_CHAIN_CONTEXT chain_context) const {
    if (chain_context)
      CertFreeCertificateChain(chain_context);
  }
};

using ScopedPCCERT_CHAIN_CONTEXT =
    std::unique_ptr<const CERT_CHAIN_CONTEXT, FreeCertChainContextFunctor>;

struct FreeCtlContextFunctor {
  void operator()(PCCTL_CONTEXT ctl_context) const {
    if (ctl_context)
      CertFreeCTLContext(ctl_context);
  }
};

using ScopedPCCTL_CONTEXT =
    std::unique_ptr<const CTL_CONTEXT, FreeCtlContextFunctor>;

}  // namespace crypto

#endif  // CRYPTO_SCOPED_CAPI_TYPES_H_
