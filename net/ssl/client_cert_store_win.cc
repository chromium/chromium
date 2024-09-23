// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/ssl/client_cert_store_win.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>

#include <windows.h>

#define SECURITY_WIN32
#include <security.h>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/scoped_generic.h"
#include "base/task/single_thread_task_runner.h"
#include "base/win/wincrypt_shim.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_win.h"
#include "net/ssl/ssl_platform_key_util.h"
#include "net/ssl/ssl_platform_key_win.h"
#include "net/ssl/ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

namespace {

using ScopedHCERTSTOREWithChecks = base::ScopedGeneric<
    HCERTSTORE,
    crypto::CAPITraitsWithFlags<HCERTSTORE,
                                CertCloseStore,
                                CERT_CLOSE_STORE_CHECK_FLAG>>;

class ClientCertIdentityWin : public ClientCertIdentity {
 public:
  ClientCertIdentityWin(
      scoped_refptr<net::X509Certificate> cert,
      crypto::ScopedPCCERT_CONTEXT cert_context,
      scoped_refptr<base::SingleThreadTaskRunner> key_task_runner)
      : ClientCertIdentity(std::move(cert)),
        cert_context_(std::move(cert_context)),
        key_task_runner_(std::move(key_task_runner)) {}

  void AcquirePrivateKey(base::OnceCallback<void(scoped_refptr<SSLPrivateKey>)>
                             private_key_callback) override {
    key_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&FetchClientCertPrivateKey,
                       base::Unretained(certificate()), cert_context_.get()),
        std::move(private_key_callback));
  }

 private:
  crypto::ScopedPCCERT_CONTEXT cert_context_;
  scoped_refptr<base::SingleThreadTaskRunner> key_task_runner_;
};

// Callback required by Windows API function CertFindChainInStore(). In addition
// to filtering by extended/enhanced key usage, we do not show expired
// certificates and require digital signature usage in the key usage extension.
//
// This matches our behavior on Mac OS X and that of NSS. It also matches the
// default behavior of IE8. See http://support.microsoft.com/kb/890326 and
// http://blogs.msdn.com/b/askie/archive/2009/06/09/my-expired-client-certifica
//     tes-no-longer-display-when-connecting-to-my-web-server-using-ie8.aspx
static BOOL WINAPI ClientCertFindCallback(PCCERT_CONTEXT cert_context,
                                          void* find_arg) {
  // Verify the certificate key usage is appropriate or not specified.
  BYTE key_usage;
  if (CertGetIntendedKeyUsage(X509_ASN_ENCODING, cert_context->pCertInfo,
                              &key_usage, 1)) {
    if (!(key_usage & CERT_DIGITAL_SIGNATURE_KEY_USAGE))
      return FALSE;
  } else {
    DWORD err = GetLastError();
    // If |err| is non-zero, it's an actual error. Otherwise the extension
    // just isn't present, and we treat it as if everything was allowed.
    if (err) {
      DLOG(ERROR) << "CertGetIntendedKeyUsage failed: " << err;
      return FALSE;
    }
  }

  // Verify the current time is within the certificate's validity period.
  if (CertVerifyTimeValidity(nullptr, cert_context->pCertInfo) != 0)
    return FALSE;

  // Verify private key metadata is associated with this certificate.
  // TODO(ppi): Is this really needed? Isn't it equivalent to leaving
  // CERT_CHAIN_FIND_BY_ISSUER_NO_KEY_FLAG not set in |find_flags| argument of
  // CertFindChainInStore()?
  DWORD size = 0;
  if (!CertGetCertificateContextProperty(
          cert_context, CERT_KEY_PROV_INFO_PROP_ID, nullptr, &size)) {
    return FALSE;
  }

  return TRUE;
}

ClientCertIdentityList GetClientCertsImpl(HCERTSTORE cert_store,
                                          const SSLCertRequestInfo& request) {
  ClientCertIdentityList selected_identities;

  scoped_refptr<base::SingleThreadTaskRunner> current_thread =
      base::SingleThreadTaskRunner::GetCurrentDefault();

  const size_t auth_count = request.cert_authorities.size();
  std::vector<CERT_NAME_BLOB> issuers(auth_count);
  for (size_t i = 0; i < auth_count; ++i) {
    issuers[i].cbData = static_cast<DWORD>(request.cert_authorities[i].size());
    issuers[i].pbData = reinterpret_cast<BYTE*>(
        const_cast<char*>(request.cert_authorities[i].data()));
  }

  // Enumerate the client certificates.
  CERT_CHAIN_FIND_BY_ISSUER_PARA find_by_issuer_para;
  memset(&find_by_issuer_para, 0, sizeof(find_by_issuer_para));
  find_by_issuer_para.cbSize = sizeof(find_by_issuer_para);
  find_by_issuer_para.pszUsageIdentifier = szOID_PKIX_KP_CLIENT_AUTH;
  find_by_issuer_para.cIssuer = static_cast<DWORD>(auth_count);
  find_by_issuer_para.rgIssuer =
      reinterpret_cast<CERT_NAME_BLOB*>(issuers.data());
  find_by_issuer_para.pfnFindCallback = ClientCertFindCallback;

  PCCERT_CHAIN_CONTEXT chain_context = nullptr;
  DWORD find_flags = CERT_CHAIN_FIND_BY_ISSUER_CACHE_ONLY_FLAG |
                     CERT_CHAIN_FIND_BY_ISSUER_CACHE_ONLY_URL_FLAG;
  for (;;) {
    // Find a certificate chain.
    chain_context = CertFindChainInStore(cert_store,
                                         X509_ASN_ENCODING,
                                         find_flags,
                                         CERT_CHAIN_FIND_BY_ISSUER,
                                         &find_by_issuer_para,
                                         chain_context);
    if (!chain_context) {
      if (GetLastError() != static_cast<DWORD>(CRYPT_E_NOT_FOUND))
        DPLOG(ERROR) << "CertFindChainInStore failed: ";
      break;
    }

    // Get the leaf certificate.
    PCCERT_CONTEXT cert_context =
        chain_context->rgpChain[0]->rgpElement[0]->pCertContext;
    // Copy the certificate, so that it is valid after |cert_store| is closed.
    crypto::ScopedPCCERT_CONTEXT cert_context2;
    PCCERT_CONTEXT raw = nullptr;
    BOOL ok = CertAddCertificateContextToStore(
        nullptr, cert_context, CERT_STORE_ADD_USE_EXISTING, &raw);
    if (!ok) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }
    cert_context2.reset(raw);

    // Grab the intermediates, if any.
    std::vector<crypto::ScopedPCCERT_CONTEXT> intermediates_storage;
    std::vector<PCCERT_CONTEXT> intermediates;
    for (DWORD i = 1; i < chain_context->rgpChain[0]->cElement; ++i) {
      PCCERT_CONTEXT chain_intermediate =
          chain_context->rgpChain[0]->rgpElement[i]->pCertContext;
      PCCERT_CONTEXT copied_intermediate = nullptr;
      ok = CertAddCertificateContextToStore(nullptr, chain_intermediate,
                                            CERT_STORE_ADD_USE_EXISTING,
                                            &copied_intermediate);
      if (ok) {
        intermediates.push_back(copied_intermediate);
        intermediates_storage.emplace_back(copied_intermediate);
      }
    }

    // Drop the self-signed root, if any. Match Internet Explorer in not sending
    // it. Although the root's signature is irrelevant for authentication, some
    // servers reject chains if the root is explicitly sent and has a weak
    // signature algorithm. See https://crbug.com/607264.
    //
    // The leaf or a intermediate may also have a weak signature algorithm but,
    // in that case, assume it is a configuration error.
    if (!intermediates.empty() &&
        x509_util::IsSelfSigned(intermediates.back())) {
      intermediates.pop_back();
      intermediates_storage.pop_back();
    }

    // Allow UTF-8 inside PrintableStrings in client certificates. See
    // crbug.com/770323.
    X509Certificate::UnsafeCreateOptions options;
    options.printable_string_is_utf8 = true;
    scoped_refptr<X509Certificate> cert =
        x509_util::CreateX509CertificateFromCertContexts(
            cert_context2.get(), intermediates, options);
    if (cert) {
      selected_identities.push_back(std::make_unique<ClientCertIdentityWin>(
          std::move(cert),
          std::move(cert_context2),  // Takes ownership of |cert_context2|.
          current_thread));  // The key must be acquired on the same thread, as
                             // the PCCERT_CONTEXT may not be thread safe.
    }
  }

  std::sort(selected_identities.begin(), selected_identities.end(),
            ClientCertIdentitySorter());
  return selected_identities;
}

}  // namespace

ClientCertStoreWin::ClientCertStoreWin() = default;

ClientCertStoreWin::ClientCertStoreWin(
    base::RepeatingCallback<crypto::ScopedHCERTSTORE()> cert_store_callback)
    : cert_store_callback_(std::move(cert_store_callback)) {
  DCHECK(!cert_store_callback_.is_null());
}

ClientCertStoreWin::~ClientCertStoreWin() = default;

void ClientCertStoreWin::GetClientCerts(
    scoped_refptr<const SSLCertRequestInfo> request,
    ClientCertListCallback callback) {
  GetSSLPlatformKeyTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ClientCertStoreWin::GetClientCertsWithCertStore,
                     std::move(request), cert_store_callback_),
      base::BindOnce(&ClientCertStoreWin::OnClientCertsResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ClientCertStoreWin::OnClientCertsResponse(
    ClientCertListCallback callback,
    ClientCertIdentityList identities) {
  std::move(callback).Run(std::move(identities));
}

// static
ClientCertIdentityList ClientCertStoreWin::GetClientCertsWithCertStore(
    scoped_refptr<const SSLCertRequestInfo> request,
    const base::RepeatingCallback<crypto::ScopedHCERTSTORE()>&
        cert_store_callback) {
  ScopedHCERTSTOREWithChecks cert_store;
  if (cert_store_callback.is_null()) {
    // Always open a new instance of the "MY" store, to ensure that there
    // are no previously cached certificates being reused after they're
    // no longer available (some smartcard providers fail to update the "MY"
    // store handles and instead interpose CertOpenSystemStore). To help confirm
    // this, use `ScopedHCERTSTOREWithChecks` and `CERT_CLOSE_STORE_CHECK_FLAG`
    // to DCHECK that `cert_store` is not inadvertently ref-counted.
    cert_store.reset(CertOpenSystemStore(NULL, L"MY"));
  } else {
    cert_store.reset(cert_store_callback.Run().release());
  }
  if (!cert_store.is_valid()) {
    PLOG(ERROR) << "Could not open certificate store: ";
    return ClientCertIdentityList();
  }
  return GetClientCertsImpl(cert_store.get(), *request);
}

bool ClientCertStoreWin::SelectClientCertsForTesting(
    const CertificateList& input_certs,
    const SSLCertRequestInfo& request,
    ClientCertIdentityList* selected_identities) {
  ScopedHCERTSTOREWithChecks test_store(
      CertOpenStore(CERT_STORE_PROV_MEMORY, 0, NULL, 0, nullptr));
  if (!test_store.is_valid())
    return false;

  // Add available certificates to the test store.
  for (const auto& input_cert : input_certs) {
    // Add the certificate to the test store.
    PCCERT_CONTEXT cert = nullptr;
    if (!CertAddEncodedCertificateToStore(
            test_store.get(), X509_ASN_ENCODING,
            reinterpret_cast<const BYTE*>(
                CRYPTO_BUFFER_data(input_cert->cert_buffer())),
            base::checked_cast<DWORD>(
                CRYPTO_BUFFER_len(input_cert->cert_buffer())),
            CERT_STORE_ADD_NEW, &cert)) {
      return false;
    }
    // Hold the reference to the certificate (since we requested a copy).
    crypto::ScopedPCCERT_CONTEXT scoped_cert(cert);

    // Add dummy private key data to the certificate - otherwise the certificate
    // would be discarded by the filtering routines.
    CRYPT_KEY_PROV_INFO private_key_data;
    memset(&private_key_data, 0, sizeof(private_key_data));
    if (!CertSetCertificateContextProperty(cert,
                                           CERT_KEY_PROV_INFO_PROP_ID,
                                           0, &private_key_data)) {
      return false;
    }
  }

  *selected_identities = GetClientCertsImpl(test_store.get(), request);
  return true;
}

}  // namespace net
