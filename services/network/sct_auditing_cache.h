// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SCT_AUDITING_CACHE_H_
#define SERVICES_NETWORK_SCT_AUDITING_CACHE_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/mru_cache.h"
#include "base/time/time.h"
#include "net/base/hash_value.h"
#include "net/base/host_port_pair.h"
#include "net/cert/sct_auditing_delegate.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "services/network/public/proto/sct_audit_report.pb.h"

namespace net {
class X509Certificate;
}

namespace network {
class NetworkContext;

// SCTAuditingCache tracks SCTs seen during CT verification. The cache supports
// a configurable sample rate to reduce load, and deduplicates SCTs seen more
// than once. The cache evicts least-recently-used entries after it reaches its
// capacity.
//
// A single SCTAuditingCache should be shared among all contexts that want to
// deduplicate reports and use a single sampling mechanism. Currently, one
// SCTAuditingCache is created and owned by the NetworkService and shared
// across all NetworkContexts.
class COMPONENT_EXPORT(NETWORK_SERVICE) SCTAuditingCache {
 public:
  explicit SCTAuditingCache(size_t cache_size);
  ~SCTAuditingCache();

  SCTAuditingCache(const SCTAuditingCache&) = delete;
  SCTAuditingCache& operator=(const SCTAuditingCache&) = delete;

  // Creates a report containing the details about the connection context and
  // SCTs and adds it to the cache if the SCTs are not already in the
  // cache. If the SCTs were not already in the cache, a random sample is drawn
  // to determine whether to notify the NetworkContextClient (and thus send a
  // report). This means we sample a subset of *certificates* rather than a
  // subset of *connections*. If a new entry is sampled, the associated
  // NetworkContextClient is notified.
  void MaybeEnqueueReport(
      NetworkContext* context,
      const net::HostPortPair& host_port_pair,
      const net::X509Certificate* validated_certificate_chain,
      const net::SignedCertificateTimestampAndStatusList&
          signed_certificate_timestamps);

  sct_auditing::TLSConnectionReport* GetPendingReport(
      const net::SHA256HashValue& cache_key);

  void ClearCache();

  base::MRUCache<net::SHA256HashValue,
                 std::unique_ptr<sct_auditing::TLSConnectionReport>>*
  GetCacheForTesting() {
    return &cache_;
  }

 private:
  base::MRUCache<net::SHA256HashValue,
                 std::unique_ptr<sct_auditing::TLSConnectionReport>>
      cache_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SCT_AUDITING_CACHE_H_
