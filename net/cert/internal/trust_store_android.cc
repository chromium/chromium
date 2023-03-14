// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_android.h"

#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "net/android/network_library.h"
#include "net/cert/pki/cert_errors.h"
#include "net/cert/pki/parse_name.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"

namespace net {

class TrustStoreAndroid::Impl
    : public base::RefCountedThreadSafe<TrustStoreAndroid::Impl> {
 public:
  explicit Impl(int generation) : generation_(generation) {
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

  int generation() { return generation_; }

 private:
  friend class base::RefCountedThreadSafe<TrustStoreAndroid::Impl>;
  ~Impl() = default;

  // Generation # that trust_store_ was loaded at.
  const int generation_;

  TrustStoreInMemory trust_store_;
};

TrustStoreAndroid::TrustStoreAndroid() = default;

TrustStoreAndroid::~TrustStoreAndroid() {
  if (is_observing_certdb_changes) {
    CertDatabase::GetInstance()->RemoveObserver(this);
  }
}

void TrustStoreAndroid::Initialize() {
  MaybeInitializeAndGetImpl();
}

// This function is not thread safe. CertDB observation is added here to avoid
// having to add a TaskEnvironment to every unit test that uses
// TrustStoreAndroid.
void TrustStoreAndroid::ObserveCertDBChanges() {
  if (!is_observing_certdb_changes) {
    is_observing_certdb_changes = true;
    CertDatabase::GetInstance()->AddObserver(this);
  }
}

void TrustStoreAndroid::OnCertDBChanged() {
  // Increment the generation number. This will regenerate the impl_ next time
  // it is fetched. It would be neater to regenerate the impl_ here but
  // complications around blocking of threads prevents this from being easily
  // accomplished.
  generation_++;
}

scoped_refptr<TrustStoreAndroid::Impl>
TrustStoreAndroid::MaybeInitializeAndGetImpl() {
  base::AutoLock lock(init_lock_);
  // It is possible that generation_ might be incremented in between the various
  // statements here, but that's okay as the worst case is that we will cause a
  // bit of extra work in reloading the android trust store if we get many
  // OnCertDBChanged() calls in rapid succession.
  int current_generation = generation_.load();
  if (!impl_ || impl_->generation() != current_generation) {
    impl_ = base::MakeRefCounted<TrustStoreAndroid::Impl>(current_generation);
  }
  return impl_;
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
