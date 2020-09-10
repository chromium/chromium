// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sct_auditing_cache.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "net/base/hash_value.h"
#include "net/base/host_port_pair.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/cert/x509_certificate.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/proto/sct_audit_report.pb.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace network {

namespace {

sct_auditing::SCTWithSourceAndVerifyStatus::SctVerifyStatus
MapSCTVerifyStatusToProtoStatus(net::ct::SCTVerifyStatus status) {
  switch (status) {
    case net::ct::SCTVerifyStatus::SCT_STATUS_NONE:
      return sct_auditing::SCTWithSourceAndVerifyStatus::SctVerifyStatus::
          SCTWithSourceAndVerifyStatus_SctVerifyStatus_NONE;
    case net::ct::SCTVerifyStatus::SCT_STATUS_LOG_UNKNOWN:
      return sct_auditing::SCTWithSourceAndVerifyStatus::SctVerifyStatus::
          SCTWithSourceAndVerifyStatus_SctVerifyStatus_LOG_UNKNOWN;
    case net::ct::SCTVerifyStatus::SCT_STATUS_OK:
      return sct_auditing::SCTWithSourceAndVerifyStatus::SctVerifyStatus::
          SCTWithSourceAndVerifyStatus_SctVerifyStatus_OK;
    case net::ct::SCTVerifyStatus::SCT_STATUS_INVALID_SIGNATURE:
      return sct_auditing::SCTWithSourceAndVerifyStatus::SctVerifyStatus::
          SCTWithSourceAndVerifyStatus_SctVerifyStatus_INVALID_SIGNATURE;
    case net::ct::SCTVerifyStatus::SCT_STATUS_INVALID_TIMESTAMP:
      return sct_auditing::SCTWithSourceAndVerifyStatus::SctVerifyStatus::
          SCTWithSourceAndVerifyStatus_SctVerifyStatus_INVALID_TIMESTAMP;
  }
}

sct_auditing::SCTWithSourceAndVerifyStatus::Source MapSCTOriginToSource(
    net::ct::SignedCertificateTimestamp::Origin origin) {
  switch (origin) {
    case net::ct::SignedCertificateTimestamp::Origin::SCT_EMBEDDED:
      return sct_auditing::SCTWithSourceAndVerifyStatus::Source::
          SCTWithSourceAndVerifyStatus_Source_EMBEDDED;
    case net::ct::SignedCertificateTimestamp::Origin::SCT_FROM_TLS_EXTENSION:
      return sct_auditing::SCTWithSourceAndVerifyStatus::Source::
          SCTWithSourceAndVerifyStatus_Source_TLS_EXTENSION;
    case net::ct::SignedCertificateTimestamp::Origin::SCT_FROM_OCSP_RESPONSE:
      return sct_auditing::SCTWithSourceAndVerifyStatus::Source::
          SCTWithSourceAndVerifyStatus_Source_OCSP_RESPONSE;
    default:
      return sct_auditing::SCTWithSourceAndVerifyStatus::Source::
          SCTWithSourceAndVerifyStatus_Source_SOURCE_UNSPECIFIED;
  }
}

}  // namespace

SCTAuditingCache::SCTAuditingCache(size_t cache_size) : cache_(cache_size) {}
SCTAuditingCache::~SCTAuditingCache() = default;

void SCTAuditingCache::MaybeEnqueueReport(
    NetworkContext* context,
    const net::HostPortPair& host_port_pair,
    const net::X509Certificate* validated_certificate_chain,
    const net::SignedCertificateTimestampAndStatusList&
        signed_certificate_timestamps) {
  if (!base::FeatureList::IsEnabled(features::kSCTAuditing) ||
      !context->is_sct_auditing_enabled()) {
    return;
  }

  // Generate the cache key for this report. In order to have the cache
  // deduplicate reports for the same SCTs, we compute the cache key as the
  // hash of the SCTs. The digest is converted to a string for use over Mojo.
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  for (const auto& sct : signed_certificate_timestamps) {
    std::string serialized_sct;
    net::ct::EncodeSignedCertificateTimestamp(sct.sct, &serialized_sct);
    SHA256_Update(&ctx, serialized_sct.data(), serialized_sct.size());
  }
  net::SHA256HashValue cache_key;
  SHA256_Final(reinterpret_cast<uint8_t*>(&cache_key), &ctx);

  // Check if the SCTs are already in the cache. This will update the last seen
  // time if they are present in the cache.
  auto it = cache_.Get(cache_key);
  if (it != cache_.end())
    return;

  // Insert SCTs into cache.
  auto report = std::make_unique<sct_auditing::TLSConnectionReport>();
  auto* connection_context = report->mutable_context();

  base::TimeDelta time_since_unix_epoch =
      base::Time::Now() - base::Time::UnixEpoch();
  connection_context->set_time_seen(time_since_unix_epoch.InSeconds());
  auto* origin = connection_context->mutable_origin();
  origin->set_hostname(host_port_pair.host());
  origin->set_port(host_port_pair.port());

  // Convert the certificate chain to a PEM encoded vector, and then initialize
  // the proto's |certificate_chain| repeated field using the data in the
  // vector. Note that GetPEMEncodedChain() can fail, but we still want to
  // enqueue the report for the SCTs (in that case, |certificate_chain| is not
  // guaranteed to be valid).
  std::vector<std::string> certificate_chain;
  validated_certificate_chain->GetPEMEncodedChain(&certificate_chain);
  *connection_context->mutable_certificate_chain() = {certificate_chain.begin(),
                                                      certificate_chain.end()};

  for (const auto& sct : signed_certificate_timestamps) {
    auto* sct_source_and_status = report->add_included_scts();
    sct_source_and_status->set_status(
        MapSCTVerifyStatusToProtoStatus(sct.status));
    sct_source_and_status->set_source(MapSCTOriginToSource(sct.sct->origin));
    std::string serialized_sct;
    net::ct::EncodeSignedCertificateTimestamp(sct.sct, &serialized_sct);
    sct_source_and_status->set_sct(serialized_sct);
  }

  cache_.Put(cache_key, std::move(report));

  // TODO(1082860): We should optimize memory usage by only storing an empty
  // report for items we don't sample.
  double sampling_rate = features::kSCTAuditingSamplingRate.Get();
  if (base::RandDouble() > sampling_rate)
    return;

  context->client()->OnSCTReportReady(net::HashValue(cache_key).ToString());
}

sct_auditing::TLSConnectionReport* SCTAuditingCache::GetPendingReport(
    const net::SHA256HashValue& cache_key) {
  NOTIMPLEMENTED();
  return nullptr;
}

void SCTAuditingCache::ClearCache() {
  cache_.Clear();
}

}  // namespace network
