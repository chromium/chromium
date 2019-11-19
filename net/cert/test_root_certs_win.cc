// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/test_root_certs.h"

#include <stdint.h>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/win/win_util.h"
#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

namespace {

// Provides a CertDllOpenStoreProv callback provider function, to be called
// by CertOpenStore when the CERT_STORE_PROV_SYSTEM_W store is opened. See
// http://msdn.microsoft.com/en-us/library/aa376043(VS.85).aspx.
BOOL WINAPI InterceptedOpenStoreW(LPCSTR store_provider,
                                  DWORD encoding,
                                  HCRYPTPROV crypt_provider,
                                  DWORD flags,
                                  const void* extra,
                                  HCERTSTORE memory_store,
                                  PCERT_STORE_PROV_INFO store_info);

// CryptoAPIInjector is used to inject a store provider function for system
// certificate stores before the one provided internally by Crypt32.dll.
// Once injected, there is no way to remove, so every call to open a system
// store will be redirected to the injected function.
struct CryptoAPIInjector {
  // The previous default function for opening system stores. For most
  // configurations, this should point to Crypt32's internal
  // I_CertDllOpenSystemStoreProvW function.
  PFN_CERT_DLL_OPEN_STORE_PROV_FUNC original_function;

  // The handle that CryptoAPI uses to ensure the DLL implementing
  // |original_function| remains loaded in memory.
  HCRYPTOIDFUNCADDR original_handle;

 private:
  friend struct base::LazyInstanceTraitsBase<CryptoAPIInjector>;

  CryptoAPIInjector() : original_function(nullptr), original_handle(nullptr) {
    HCRYPTOIDFUNCSET registered_functions =
        CryptInitOIDFunctionSet(CRYPT_OID_OPEN_STORE_PROV_FUNC, 0);

    // Preserve the original handler function in |original_function|. If other
    // functions are overridden, they will also need to be preserved.
    BOOL ok = CryptGetOIDFunctionAddress(
        registered_functions, 0, CERT_STORE_PROV_SYSTEM_W, 0,
        reinterpret_cast<void**>(&original_function), &original_handle);
    DCHECK(ok);

    // For now, intercept only the numeric form of the system store
    // function, CERT_STORE_PROV_SYSTEM_W (0x0A), which is what Crypt32
    // functionality uses exclusively. Depending on the machine that tests
    // are being run on, it may prove necessary to also intercept
    // sz_CERT_STORE_PROV_SYSTEM_[A/W] and CERT_STORE_PROV_SYSTEM_A, based
    // on whether or not any third-party CryptoAPI modules have been
    // installed.
    const CRYPT_OID_FUNC_ENTRY kFunctionToIntercept = {
        CERT_STORE_PROV_SYSTEM_W,
        reinterpret_cast<void*>(&InterceptedOpenStoreW)};

    // Inject kFunctionToIntercept at the front of the linked list that
    // crypt32 uses when CertOpenStore is called, replacing the existing
    // registered function.
    ok = CryptInstallOIDFunctionAddress(
        nullptr, 0, CRYPT_OID_OPEN_STORE_PROV_FUNC, 1, &kFunctionToIntercept,
        CRYPT_INSTALL_OID_FUNC_BEFORE_FLAG);
    DCHECK(ok);
  }

  // This is never called, because this object is intentionally leaked.
  // Certificate verification happens on a non-joinable worker thread, which
  // may still be running when ~AtExitManager is called, so the LazyInstance
  // must be leaky.
  ~CryptoAPIInjector() {
    original_function = nullptr;
    CryptFreeOIDFunctionAddress(original_handle, NULL);
  }
};

base::LazyInstance<CryptoAPIInjector>::Leaky
    g_capi_injector = LAZY_INSTANCE_INITIALIZER;

BOOL WINAPI InterceptedOpenStoreW(LPCSTR store_provider,
                                  DWORD encoding,
                                  HCRYPTPROV crypt_provider,
                                  DWORD flags,
                                  const void* store_name,
                                  HCERTSTORE memory_store,
                                  PCERT_STORE_PROV_INFO store_info) {
  // If the high word is all zeroes, then |store_provider| is a numeric ID.
  // Otherwise, it's a pointer to a null-terminated ASCII string. See the
  // documentation for CryptGetOIDFunctionAddress for more information.
  uintptr_t store_as_uintptr = reinterpret_cast<uintptr_t>(store_provider);
  if (store_as_uintptr > 0xFFFF || store_provider != CERT_STORE_PROV_SYSTEM_W ||
      !g_capi_injector.Get().original_function)
    return FALSE;

  BOOL ok = g_capi_injector.Get().original_function(store_provider, encoding,
                                                    crypt_provider, flags,
                                                    store_name, memory_store,
                                                    store_info);
  // Only the Root store should have certificates injected. If
  // CERT_SYSTEM_STORE_RELOCATE_FLAG is set, then |store_name| points to a
  // CERT_SYSTEM_STORE_RELOCATE_PARA structure, rather than a
  // NULL-terminated wide string, so check before making a string
  // comparison.
  if (!ok || TestRootCerts::GetInstance()->IsEmpty() ||
      (flags & CERT_SYSTEM_STORE_RELOCATE_FLAG) ||
      lstrcmpiW(reinterpret_cast<LPCWSTR>(store_name), L"root"))
    return ok;

  // The result of CertOpenStore with CERT_STORE_PROV_SYSTEM_W is documented
  // to be a collection store, and that appears to hold for |memory_store|.
  // Attempting to add an individual certificate to |memory_store| causes
  // the request to be forwarded to the first physical store in the
  // collection that accepts modifications, which will cause a secure
  // confirmation dialog to be displayed, confirming the user wishes to
  // trust the certificate. However, appending a store to the collection
  // will merely modify the temporary collection store, and will not persist
  // any changes to the underlying physical store. When the |memory_store| is
  // searched to see if a certificate is in the Root store, all the
  // underlying stores in the collection will be searched, and any certificate
  // in temporary_roots() will be found and seen as trusted.
  return CertAddStoreToCollection(
      memory_store, TestRootCerts::GetInstance()->temporary_roots(), 0, 0);
}

}  // namespace

bool TestRootCerts::Add(X509Certificate* certificate) {
  // Ensure that the default CryptoAPI functionality has been intercepted.
  // If a test certificate is never added, then no interception should
  // happen.
  g_capi_injector.Get();

  BOOL ok = CertAddEncodedCertificateToStore(
      temporary_roots_, X509_ASN_ENCODING,
      reinterpret_cast<const BYTE*>(
          CRYPTO_BUFFER_data(certificate->cert_buffer())),
      base::checked_cast<DWORD>(CRYPTO_BUFFER_len(certificate->cert_buffer())),
      CERT_STORE_ADD_NEW, nullptr);
  if (!ok) {
    // If the certificate is already added, return successfully.
    return GetLastError() == static_cast<DWORD>(CRYPT_E_EXISTS);
  }

  empty_ = false;
  return true;
}

void TestRootCerts::Clear() {
  empty_ = true;

  for (PCCERT_CONTEXT prev_cert =
           CertEnumCertificatesInStore(temporary_roots_, nullptr);
       prev_cert;
       prev_cert = CertEnumCertificatesInStore(temporary_roots_, nullptr))
    CertDeleteCertificateFromStore(prev_cert);
}

bool TestRootCerts::IsEmpty() const {
  return empty_;
}

HCERTCHAINENGINE TestRootCerts::GetChainEngine() const {
  if (IsEmpty())
    return nullptr;  // Default chain engine will suffice.

  // Windows versions before 8 don't accept the struct size for later versions.
  // We report the size of the old struct since we don't need the new members.
  static const DWORD kSizeofCertChainEngineConfig =
      SIZEOF_STRUCT_WITH_SPECIFIED_LAST_MEMBER(CERT_CHAIN_ENGINE_CONFIG,
                                               hExclusiveTrustedPeople);

  // Each HCERTCHAINENGINE caches both the configured system stores and
  // information about each chain that has been built. In order to ensure
  // that changes to |temporary_roots_| are properly propagated and that the
  // various caches are flushed, when at least one certificate is added,
  // return a new chain engine for every call. Each chain engine creation
  // should re-open the root store, ensuring the most recent changes are
  // visible.
  CERT_CHAIN_ENGINE_CONFIG engine_config = {
    kSizeofCertChainEngineConfig
  };
  engine_config.dwFlags =
      CERT_CHAIN_ENABLE_CACHE_AUTO_UPDATE |
      CERT_CHAIN_ENABLE_SHARE_STORE;
  HCERTCHAINENGINE chain_engine = nullptr;
  BOOL ok = CertCreateCertificateChainEngine(&engine_config, &chain_engine);
  DCHECK(ok);
  return chain_engine;
}

TestRootCerts::~TestRootCerts() {
  CertCloseStore(temporary_roots_, 0);
}

void TestRootCerts::Init() {
  empty_ = true;
  temporary_roots_ =
      CertOpenStore(CERT_STORE_PROV_MEMORY, 0, NULL,
                    CERT_STORE_DEFER_CLOSE_UNTIL_LAST_FREE_FLAG, nullptr);
  DCHECK(temporary_roots_);
}

}  // namespace net
