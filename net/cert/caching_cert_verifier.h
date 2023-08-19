// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CACHING_CERT_VERIFIER_H_
#define NET_CERT_CACHING_CERT_VERIFIER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/expiring_cache.h"
#include "net/base/net_export.h"
#include "net/cert/cert_database.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"

namespace net {

// CertVerifier that caches the results of certificate verifications.
//
// In general, certificate verification results will vary on only three
// parameters:
//   - The time of validation (as certificates are only valid for a period of
//     time)
//   - The revocation status (a certificate may be revoked at any time, but
//     revocation statuses themselves have validity period, so a 'good' result
//     may be reused for a period of time)
//   - The trust settings (a user may change trust settings at any time)
//
// This class tries to optimize by allowing certificate verification results
// to be cached for a limited amount of time (presently, 30 minutes), which
// tries to balance the implementation complexity of needing to monitor the
// above for meaningful changes and the practical utility of being able to
// cache results when they're not expected to change.
class NET_EXPORT CachingCertVerifier : public CertVerifier,
                                       public CertVerifier::Observer,
                                       public CertDatabase::Observer {
 public:
  // Creates a CachingCertVerifier that will use |verifier| to perform the
  // actual verifications if they're not already cached or if the cached
  // item has expired.
  explicit CachingCertVerifier(std::unique_ptr<CertVerifier> verifier);

  CachingCertVerifier(const CachingCertVerifier&) = delete;
  CachingCertVerifier& operator=(const CachingCertVerifier&) = delete;

  ~CachingCertVerifier() override;

  // CertVerifier implementation:
  int Verify(const RequestParams& params,
             CertVerifyResult* verify_result,
             CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req,
             const NetLogWithSource& net_log) override;
  void SetConfig(const Config& config) override;
  void AddObserver(CertVerifier::Observer* observer) override;
  void RemoveObserver(CertVerifier::Observer* observer) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(CachingCertVerifierTest, CacheHit);
  FRIEND_TEST_ALL_PREFIXES(CachingCertVerifierTest, CacheHitCTResultsCached);
  FRIEND_TEST_ALL_PREFIXES(CachingCertVerifierTest, DifferentCACerts);
  FRIEND_TEST_ALL_PREFIXES(CachingCertVerifierCacheClearingTest,
                           CacheClearedSyncVerification);
  FRIEND_TEST_ALL_PREFIXES(CachingCertVerifierCacheClearingTest,
                           CacheClearedAsyncVerification);

  // CachedResult contains the result of a certificate verification.
  struct NET_EXPORT_PRIVATE CachedResult {
    CachedResult();
    ~CachedResult();

    int error = ERR_FAILED;   // The return value of CertVerifier::Verify.
    CertVerifyResult result;  // The output of CertVerifier::Verify.
  };

  // Rather than having a single validity point along a monotonically increasing
  // timeline, certificate verification is based on falling within a range of
  // the certificate's NotBefore and NotAfter and based on what the current
  // system clock says (which may advance forwards or backwards as users correct
  // clock skew). CacheValidityPeriod and CacheExpirationFunctor are helpers to
  // ensure that expiration is measured both by the 'general' case (now + cache
  // TTL) and by whether or not significant enough clock skew was introduced
  // since the last verification.
  struct CacheValidityPeriod {
    explicit CacheValidityPeriod(base::Time now);
    CacheValidityPeriod(base::Time now, base::Time expiration);

    base::Time verification_time;
    base::Time expiration_time;
  };

  struct CacheExpirationFunctor {
    // Returns true iff |now| is within the validity period of |expiration|.
    bool operator()(const CacheValidityPeriod& now,
                    const CacheValidityPeriod& expiration) const;
  };

  using CertVerificationCache = ExpiringCache<RequestParams,
                                              CachedResult,
                                              CacheValidityPeriod,
                                              CacheExpirationFunctor>;

  // Handles completion of the request matching |params|, which started at
  // |start_time| and with config |config_id|, completing. |verify_result| and
  // |result| are added to the cache, and then |callback| (the original caller's
  // callback) is invoked.
  void OnRequestFinished(uint32_t config_id,
                         const RequestParams& params,
                         base::Time start_time,
                         CompletionOnceCallback callback,
                         CertVerifyResult* verify_result,
                         int error);

  // Adds |verify_result| and |error| to the cache for |params|, whose
  // verification attempt began at |start_time| with config |config_id|. See the
  // implementation for more details about the necessity of |start_time|.
  void AddResultToCache(uint32_t config_id,
                        const RequestParams& params,
                        base::Time start_time,
                        const CertVerifyResult& verify_result,
                        int error);

  // CertVerifier::Observer methods:
  void OnCertVerifierChanged() override;

  // CertDatabase::Observer methods:
  void OnTrustStoreChanged() override;

  // For unit testing.
  void ClearCache();
  size_t GetCacheSize() const;
  uint64_t cache_hits() const { return cache_hits_; }
  uint64_t requests() const { return requests_; }

  std::unique_ptr<CertVerifier> verifier_;

  uint32_t config_id_ = 0u;
  CertVerificationCache cache_;

  uint64_t requests_ = 0u;
  uint64_t cache_hits_ = 0u;
};

}  // namespace net

#endif  // NET_CERT_CACHING_CERT_VERIFIER_H_
