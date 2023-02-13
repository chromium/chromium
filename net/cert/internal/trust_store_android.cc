// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_android.h"

#include "base/logging.h"
#include "base/threading/scoped_blocking_call.h"
#include "net/android/network_library.h"
#include "net/cert/pki/cert_errors.h"
#include "net/cert/pki/parse_name.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"

namespace net {

// TODO(hchao): reload list of roots on updates to the android trust stores
class TrustStoreAndroid::Impl {
 public:
  Impl() {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    std::vector<std::string> roots = net::android::GetUserAddedRoots();

    for (auto& root : roots) {
      CertErrors errors;
      auto parsed = net::ParsedCertificate::Create(
          net::x509_util::CreateCryptoBuffer(root),
          net::x509_util::DefaultParseCertificateOptions(), &errors);
      if (!parsed) {
        LOG(ERROR) << "Error parsing certificate:\n" << errors.ToDebugString();
        continue;
      }
      trust_store_.AddTrustAnchor(parsed);
    }
  }

  // TODO(hchao): see if we can get SyncGetIssueresOf marked const
  void SyncGetIssuersOf(const ParsedCertificate* cert,
                        ParsedCertificateList* issuers) {
    trust_store_.SyncGetIssuersOf(cert, issuers);
  }

  // TODO(hchao): see if we can get GetTrust marked const again
  CertificateTrust GetTrust(const ParsedCertificate* cert,
                            base::SupportsUserData* debug_data) {
    return trust_store_.GetTrust(cert, debug_data);
  }

 private:
  TrustStoreInMemory trust_store_;
};

TrustStoreAndroid::TrustStoreAndroid() = default;
TrustStoreAndroid::~TrustStoreAndroid() = default;

void TrustStoreAndroid::Initialize() {
  MaybeInitializeAndGetImpl();
}

TrustStoreAndroid::Impl* TrustStoreAndroid::MaybeInitializeAndGetImpl() {
  base::AutoLock lock(init_lock_);
  if (!impl_) {
    impl_ = std::make_unique<TrustStoreAndroid::Impl>();
  }
  return impl_.get();
}

void TrustStoreAndroid::SyncGetIssuersOf(const ParsedCertificate* cert,
                                         ParsedCertificateList* issuers) {
  MaybeInitializeAndGetImpl()->SyncGetIssuersOf(cert, issuers);
}

CertificateTrust TrustStoreAndroid::GetTrust(
    const ParsedCertificate* cert,
    base::SupportsUserData* debug_data) {
  return MaybeInitializeAndGetImpl()->GetTrust(cert, debug_data);
}

}  // namespace net
