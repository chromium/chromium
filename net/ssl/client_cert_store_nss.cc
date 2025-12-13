// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "net/ssl/client_cert_store_nss.h"

#include <nss.h>
#include <ssl.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "crypto/nss_crypto_module_delegate.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_nss.h"
#include "net/ssl/client_cert_matcher.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_platform_key_nss.h"
#include "net/ssl/threaded_ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

namespace {

class ClientCertIdentityNSS : public ClientCertIdentity {
 public:
  ClientCertIdentityNSS(
      scoped_refptr<net::X509Certificate> cert,
      ScopedCERTCertificate cert_certificate,
      scoped_refptr<crypto::CryptoModuleBlockingPasswordDelegate>
          password_delegate)
      : ClientCertIdentity(std::move(cert)),
        cert_certificate_(std::move(cert_certificate)),
        password_delegate_(std::move(password_delegate)) {}
  ~ClientCertIdentityNSS() override = default;

  void AcquirePrivateKey(base::OnceCallback<void(scoped_refptr<SSLPrivateKey>)>
                             private_key_callback) override {
    // Caller is responsible for keeping the ClientCertIdentity alive until
    // the |private_key_callback| is run, so it's safe to use Unretained here.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&FetchClientCertPrivateKey,
                       base::Unretained(certificate()), cert_certificate_.get(),
                       password_delegate_),
        std::move(private_key_callback));
  }

 private:
  ScopedCERTCertificate cert_certificate_;
  scoped_refptr<crypto::CryptoModuleBlockingPasswordDelegate>
      password_delegate_;
};

}  // namespace

std::vector<bssl::UniquePtr<CRYPTO_BUFFER>>
ClientCertStoreNSS::IssuerSourceNSS::GetCertsByName(
    base::span<const uint8_t> name) {
  // This method may acquire the NSS lock or reenter this code via extension
  // hooks (such as smart card UI). To ensure threads are not starved or
  // deadlocked, the base::ScopedBlockingCall below increments the thread pool
  // capacity if this method takes too much time to run.
  // (The ScopedBlockingCall here is not redundant with the one below since
  // the IssuerSourceNSS class may be used from other places outside of
  // ClientCertStoreNSS.)
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> result;

  SECItem issuer_item;
  issuer_item.len = name.size();
  issuer_item.data = const_cast<unsigned char*>(name.data());
  ScopedCERTCertificate nss_issuer(
      CERT_FindCertByName(CERT_GetDefaultCertDB(), &issuer_item));
  if (nss_issuer) {
    result.push_back(x509_util::CreateCryptoBuffer(
        x509_util::CERTCertificateAsSpan(nss_issuer.get())));
  }

  return result;
}

ClientCertStoreNSS::ClientCertStoreNSS(
    const PasswordDelegateFactory& password_delegate_factory)
    : password_delegate_factory_(password_delegate_factory) {}

ClientCertStoreNSS::~ClientCertStoreNSS() = default;

void ClientCertStoreNSS::GetClientCerts(
    scoped_refptr<const SSLCertRequestInfo> request,
    ClientCertListCallback callback) {
  scoped_refptr<crypto::CryptoModuleBlockingPasswordDelegate> password_delegate;
  if (!password_delegate_factory_.is_null()) {
    password_delegate = password_delegate_factory_.Run(request->host_and_port);
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ClientCertStoreNSS::GetAndFilterCertsOnWorkerThread,
                     std::move(password_delegate), std::move(request)),
      base::BindOnce(&ClientCertStoreNSS::OnClientCertsResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ClientCertStoreNSS::OnClientCertsResponse(
    ClientCertListCallback callback,
    ClientCertIdentityList identities) {
  std::move(callback).Run(std::move(identities));
}

// static
void ClientCertStoreNSS::FilterCertsOnWorkerThread(
    ClientCertIdentityList* identities,
    const SSLCertRequestInfo& request) {
  ClientCertIssuerSourceCollection sources;
  sources.push_back(std::make_unique<IssuerSourceNSS>());
  FilterMatchingClientCertIdentities(identities, request, sources);
}

// static
ClientCertIdentityList ClientCertStoreNSS::GetAndFilterCertsOnWorkerThread(
    scoped_refptr<crypto::CryptoModuleBlockingPasswordDelegate>
        password_delegate,
    scoped_refptr<const SSLCertRequestInfo> request) {
  // This method may acquire the NSS lock or reenter this code via extension
  // hooks (such as smart card UI). To ensure threads are not starved or
  // deadlocked, the base::ScopedBlockingCall below increments the thread pool
  // capacity if this method takes too much time to run.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  ClientCertIdentityList selected_identities;
  GetPlatformCertsOnWorkerThread(std::move(password_delegate), CertFilter(),
                                 &selected_identities);
  FilterCertsOnWorkerThread(&selected_identities, *request);
  return selected_identities;
}

// static
void ClientCertStoreNSS::GetPlatformCertsOnWorkerThread(
    scoped_refptr<crypto::CryptoModuleBlockingPasswordDelegate>
        password_delegate,
    const CertFilter& cert_filter,
    ClientCertIdentityList* identities) {
  crypto::EnsureNSSInit();

  crypto::ScopedCERTCertList found_certs(CERT_FindUserCertsByUsage(
      CERT_GetDefaultCertDB(), certUsageSSLClient, PR_FALSE, PR_FALSE,
      password_delegate ? password_delegate->wincx() : nullptr));
  if (!found_certs) {
    DVLOG(2) << "No client certs found.";
    return;
  }
  for (CERTCertListNode* node = CERT_LIST_HEAD(found_certs);
       !CERT_LIST_END(node, found_certs); node = CERT_LIST_NEXT(node)) {
    if (!cert_filter.is_null() && !cert_filter.Run(node->cert))
      continue;
    // Allow UTF-8 inside PrintableStrings in client certificates. See
    // crbug.com/770323.
    X509Certificate::UnsafeCreateOptions options;
    options.printable_string_is_utf8 = true;
    scoped_refptr<X509Certificate> cert =
        x509_util::CreateX509CertificateFromCERTCertificate(node->cert, {},
                                                            options);
    if (!cert) {
      DVLOG(2) << "x509_util::CreateX509CertificateFromCERTCertificate failed";
      continue;
    }
    identities->push_back(std::make_unique<ClientCertIdentityNSS>(
        cert, x509_util::DupCERTCertificate(node->cert), password_delegate));
  }
}

}  // namespace net
