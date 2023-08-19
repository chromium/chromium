// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/caching_cert_verifier.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

// The maximum number of cache entries to use for the ExpiringCache.
const unsigned kMaxCacheEntries = 256;

// The number of seconds to cache entries.
const unsigned kTTLSecs = 1800;  // 30 minutes.

}  // namespace

CachingCertVerifier::CachingCertVerifier(std::unique_ptr<CertVerifier> verifier)
    : verifier_(std::move(verifier)), cache_(kMaxCacheEntries) {
  verifier_->AddObserver(this);
  CertDatabase::GetInstance()->AddObserver(this);
}

CachingCertVerifier::~CachingCertVerifier() {
  CertDatabase::GetInstance()->RemoveObserver(this);
  verifier_->RemoveObserver(this);
}

int CachingCertVerifier::Verify(const CertVerifier::RequestParams& params,
                                CertVerifyResult* verify_result,
                                CompletionOnceCallback callback,
                                std::unique_ptr<Request>* out_req,
                                const NetLogWithSource& net_log) {
  out_req->reset();

  requests_++;

  const CertVerificationCache::value_type* cached_entry =
      cache_.Get(params, CacheValidityPeriod(base::Time::Now()));
  if (cached_entry) {
    ++cache_hits_;
    *verify_result = cached_entry->result;
    return cached_entry->error;
  }

  base::Time start_time = base::Time::Now();
  // Unretained is safe here as `verifier_` is owned by `this`. If `this` is
  // deleted, `verifier_' will also be deleted and guarantees that any
  // outstanding callbacks won't be called. (See CertVerifier::Verify comments.)
  CompletionOnceCallback caching_callback = base::BindOnce(
      &CachingCertVerifier::OnRequestFinished, base::Unretained(this),
      config_id_, params, start_time, std::move(callback), verify_result);
  int result = verifier_->Verify(params, verify_result,
                                 std::move(caching_callback), out_req, net_log);
  if (result != ERR_IO_PENDING) {
    // Synchronous completion; add directly to cache.
    AddResultToCache(config_id_, params, start_time, *verify_result, result);
  }

  return result;
}

void CachingCertVerifier::SetConfig(const CertVerifier::Config& config) {
  verifier_->SetConfig(config);
  config_id_++;
  ClearCache();
}

void CachingCertVerifier::AddObserver(CertVerifier::Observer* observer) {
  verifier_->AddObserver(observer);
}

void CachingCertVerifier::RemoveObserver(CertVerifier::Observer* observer) {
  verifier_->RemoveObserver(observer);
}

CachingCertVerifier::CachedResult::CachedResult() = default;

CachingCertVerifier::CachedResult::~CachedResult() = default;

CachingCertVerifier::CacheValidityPeriod::CacheValidityPeriod(base::Time now)
    : verification_time(now), expiration_time(now) {}

CachingCertVerifier::CacheValidityPeriod::CacheValidityPeriod(
    base::Time now,
    base::Time expiration)
    : verification_time(now), expiration_time(expiration) {}

bool CachingCertVerifier::CacheExpirationFunctor::operator()(
    const CacheValidityPeriod& now,
    const CacheValidityPeriod& expiration) const {
  // Ensure this functor is being used for expiration only, and not strict
  // weak ordering/sorting. |now| should only ever contain a single
  // base::Time.
  // Note: DCHECK_EQ is not used due to operator<< overloading requirements.
  DCHECK(now.verification_time == now.expiration_time);

  // |now| contains only a single time (verification_time), while |expiration|
  // contains the validity range - both when the certificate was verified and
  // when the verification result should expire.
  //
  // If the user receives a "not yet valid" message, and adjusts their clock
  // foward to the correct time, this will (typically) cause
  // now.verification_time to advance past expiration.expiration_time, thus
  // treating the cached result as an expired entry and re-verifying.
  // If the user receives a "expired" message, and adjusts their clock
  // backwards to the correct time, this will cause now.verification_time to
  // be less than expiration_verification_time, thus treating the cached
  // result as an expired entry and re-verifying.
  // If the user receives either of those messages, and does not adjust their
  // clock, then the result will be (typically) be cached until the expiration
  // TTL.
  //
  // This algorithm is only problematic if the user consistently keeps
  // adjusting their clock backwards in increments smaller than the expiration
  // TTL, in which case, cached elements continue to be added. However,
  // because the cache has a fixed upper bound, if no entries are expired, a
  // 'random' entry will be, thus keeping the memory constraints bounded over
  // time.
  return now.verification_time >= expiration.verification_time &&
         now.verification_time < expiration.expiration_time;
}

void CachingCertVerifier::OnRequestFinished(uint32_t config_id,
                                            const RequestParams& params,
                                            base::Time start_time,
                                            CompletionOnceCallback callback,
                                            CertVerifyResult* verify_result,
                                            int error) {
  AddResultToCache(config_id, params, start_time, *verify_result, error);

  // Now chain to the user's callback, which may delete |this|.
  std::move(callback).Run(error);
}

void CachingCertVerifier::AddResultToCache(
    uint32_t config_id,
    const RequestParams& params,
    base::Time start_time,
    const CertVerifyResult& verify_result,
    int error) {
  // If the configuration has changed since this verification was started,
  // don't add it to the cache.
  if (config_id != config_id_)
    return;

  // When caching, this uses the time that validation started as the
  // beginning of the validity, rather than the time that it ended (aka
  // base::Time::Now()), to account for the fact that during validation,
  // the clock may have changed.
  //
  // If the clock has changed significantly, then this result will ideally
  // be evicted and the next time the certificate is encountered, it will
  // be revalidated.
  //
  // Because of this, it's possible for situations to arise where the
  // clock was correct at the start of validation, changed to an
  // incorrect time during validation (such as too far in the past or
  // future), and then was reset to the correct time. If this happens,
  // it's likely that the result will not be a valid/correct result,
  // but will still be used from the cache because the clock was reset
  // to the correct time after the (bad) validation result completed.
  //
  // However, this solution optimizes for the case where the clock is
  // bad at the start of validation, and subsequently is corrected. In
  // that situation, the result is also incorrect, but because the clock
  // was corrected after validation, if the cache validity period was
  // computed at the end of validation, it would continue to serve an
  // invalid result for kTTLSecs.
  CachedResult cached_result;
  cached_result.error = error;
  cached_result.result = verify_result;
  cache_.Put(
      params, cached_result, CacheValidityPeriod(start_time),
      CacheValidityPeriod(start_time, start_time + base::Seconds(kTTLSecs)));
}

void CachingCertVerifier::OnCertVerifierChanged() {
  config_id_++;
  ClearCache();
}

void CachingCertVerifier::OnTrustStoreChanged() {
  config_id_++;
  ClearCache();
}

void CachingCertVerifier::ClearCache() {
  cache_.Clear();
}

size_t CachingCertVerifier::GetCacheSize() const {
  return cache_.size();
}

}  // namespace net
