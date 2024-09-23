// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_android.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "net/android/network_library.h"
#include "net/cert/internal/platform_trust_store.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/parse_name.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"

namespace net {

class TrustStoreAndroid::Impl
    : public base::RefCountedThreadSafe<TrustStoreAndroid::Impl> {
 public:
  explicit Impl(int generation) : generation_(generation) {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    std::vector<std::string> roots = net::android::GetUserAddedRoots();

    for (auto& root : roots) {
      bssl::CertErrors errors;
      auto parsed = bssl::ParsedCertificate::Create(
          net::x509_util::CreateCryptoBuffer(root),
          net::x509_util::DefaultParseCertificateOptions(), &errors);
      if (!parsed) {
        LOG(ERROR) << "Error parsing certificate:\n" << errors.ToDebugString();
        continue;
      }
      trust_store_.AddTrustAnchor(std::move(parsed));
    }
  }

  // TODO(hchao): see if we can get SyncGetIssueresOf marked const
  void SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                        bssl::ParsedCertificateList* issuers) {
    trust_store_.SyncGetIssuersOf(cert, issuers);
  }

  // TODO(hchao): see if we can get GetTrust marked const again
  bssl::CertificateTrust GetTrust(const bssl::ParsedCertificate* cert) {
    return trust_store_.GetTrust(cert);
  }

  int generation() { return generation_; }

 private:
  friend class base::RefCountedThreadSafe<TrustStoreAndroid::Impl>;
  ~Impl() = default;

  // Generation # that trust_store_ was loaded at.
  const int generation_;

  bssl::TrustStoreInMemory trust_store_;
};

TrustStoreAndroid::TrustStoreAndroid() {
  // It's okay for ObserveCertDBChanges to be called on a different sequence
  // than the object was constructed on.
  DETACH_FROM_SEQUENCE(certdb_observer_sequence_checker_);
}

TrustStoreAndroid::~TrustStoreAndroid() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(certdb_observer_sequence_checker_);
  if (is_observing_certdb_changes_) {
    CertDatabase::GetInstance()->RemoveObserver(this);
  }
}

void TrustStoreAndroid::Initialize() {
  MaybeInitializeAndGetImpl();
}

// This function is not thread safe. CertDatabase observation is added here
// rather than in the constructor to avoid having to add a TaskEnvironment to
// every unit test that uses TrustStoreAndroid.
void TrustStoreAndroid::ObserveCertDBChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(certdb_observer_sequence_checker_);
  if (!is_observing_certdb_changes_) {
    is_observing_certdb_changes_ = true;
    CertDatabase::GetInstance()->AddObserver(this);
  }
}

void TrustStoreAndroid::OnTrustStoreChanged() {
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
  // OnTrustStoreChanged() calls in rapid succession.
  int current_generation = generation_.load();
  if (!impl_ || impl_->generation() != current_generation) {
    SCOPED_UMA_HISTOGRAM_LONG_TIMER("Net.CertVerifier.AndroidTrustStoreInit");
    impl_ = base::MakeRefCounted<TrustStoreAndroid::Impl>(current_generation);
  }

  return impl_;
}

void TrustStoreAndroid::SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                                         bssl::ParsedCertificateList* issuers) {
  MaybeInitializeAndGetImpl()->SyncGetIssuersOf(cert, issuers);
}

bssl::CertificateTrust TrustStoreAndroid::GetTrust(
    const bssl::ParsedCertificate* cert) {
  return MaybeInitializeAndGetImpl()->GetTrust(cert);
}

std::vector<net::PlatformTrustStore::CertWithTrust>
TrustStoreAndroid::GetAllUserAddedCerts() {
  // TODO(crbug.com/40928765): implement this.
  return {};
}

}  // namespace net
