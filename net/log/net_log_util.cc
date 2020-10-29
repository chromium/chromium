// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log_util.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/address_family.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/net_info_source_list.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/internal/simple_path_builder_delegate.h"
#include "net/cert/internal/trust_store.h"
#include "net/disk_cache/disk_cache.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_transaction_factory.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_entry.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_values.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "net/socket/ssl_client_socket.h"
#include "net/third_party/quiche/src/quic/core/quic_error_codes.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_service.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

namespace net {

namespace {

// This should be incremented when significant changes are made that will
// invalidate the old loading code.
const int kLogFormatVersion = 1;

struct StringToConstant {
  const char* name;
  const int constant;
};

const StringToConstant kCertStatusFlags[] = {
#define CERT_STATUS_FLAG(label, value) {#label, value},
#include "net/cert/cert_status_flags_list.h"
#undef CERT_STATUS_FLAG
};

const StringToConstant kLoadFlags[] = {
#define LOAD_FLAG(label, value) {#label, value},
#include "net/base/load_flags_list.h"
#undef LOAD_FLAG
};

const StringToConstant kLoadStateTable[] = {
#define LOAD_STATE(label, value) {#label, LOAD_STATE_##label},
#include "net/base/load_states_list.h"
#undef LOAD_STATE
};

const short kNetErrors[] = {
#define NET_ERROR(label, value) value,
#include "net/base/net_error_list.h"
#undef NET_ERROR
};

// Returns the disk cache backend for |context| if there is one, or NULL.
// Despite the name, can return an in memory "disk cache".
disk_cache::Backend* GetDiskCacheBackend(URLRequestContext* context) {
  if (!context->http_transaction_factory())
    return nullptr;

  HttpCache* http_cache = context->http_transaction_factory()->GetCache();
  if (!http_cache)
    return nullptr;

  return http_cache->GetCurrentBackend();
}

// Returns true if |request1| was created before |request2|.
bool RequestCreatedBefore(const URLRequest* request1,
                          const URLRequest* request2) {
  // Only supported when both requests have the same non-null NetLog.
  DCHECK(request1->net_log().net_log());
  DCHECK_EQ(request1->net_log().net_log(), request2->net_log().net_log());

  if (request1->creation_time() < request2->creation_time())
    return true;
  if (request1->creation_time() > request2->creation_time())
    return false;
  // If requests were created at the same time, sort by NetLogSource ID. Some
  // NetLog tests assume the returned order exactly matches creation order, even
  // creation times of two events are potentially the same.
  return request1->net_log().source().id < request2->net_log().source().id;
}

base::Value GetActiveFieldTrialList() {
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  base::Value field_trial_groups(base::Value::Type::LIST);
  for (const auto& group : active_groups) {
    field_trial_groups.Append(group.trial_name + ":" + group.group_name);
  }
  return field_trial_groups;
}

}  // namespace

base::Value GetNetConstants() {
  base::Value constants_dict(base::Value::Type::DICTIONARY);

  // Version of the file format.
  constants_dict.SetIntKey("logFormatVersion", kLogFormatVersion);

  // Add a dictionary with information on the relationship between event type
  // enums and their symbolic names.
  constants_dict.SetKey("logEventTypes", NetLog::GetEventTypesAsValue());

  // Add a dictionary with information about the relationship between CertStatus
  // flags and their symbolic names.
  {
    base::Value dict(base::Value::Type::DICTIONARY);

    for (const auto& flag : kCertStatusFlags)
      dict.SetIntKey(flag.name, flag.constant);

    constants_dict.SetKey("certStatusFlag", std::move(dict));
  }

  // Add a dictionary with information about the relationship between
  // CertVerifier::VerifyFlags and their symbolic names.
  {
    base::Value dict(base::Value::Type::DICTIONARY);

    dict.SetIntKey("VERIFY_DISABLE_NETWORK_FETCHES",
                   CertVerifier::VERIFY_DISABLE_NETWORK_FETCHES);

    static_assert(CertVerifier::VERIFY_FLAGS_LAST == (1 << 0),
                  "Update with new flags");

    constants_dict.SetKey("certVerifierFlags", std::move(dict));
  }

  {
    base::Value dict(base::Value::Type::DICTIONARY);

    dict.SetIntKey(
        "kStrong",
        static_cast<int>(SimplePathBuilderDelegate::DigestPolicy::kStrong));
    dict.SetIntKey(
        "kWeakAllowSha1",
        static_cast<int>(
            SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1));

    static_assert(SimplePathBuilderDelegate::DigestPolicy::kMaxValue ==
                      SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1,
                  "Update with new flags");

    constants_dict.SetKey("certPathBuilderDigestPolicy", std::move(dict));
  }

  {
    base::Value dict(base::Value::Type::DICTIONARY);

    dict.SetIntKey("DISTRUSTED",
                   static_cast<int>(CertificateTrustType::DISTRUSTED));
    dict.SetIntKey("UNSPECIFIED",
                   static_cast<int>(CertificateTrustType::UNSPECIFIED));
    dict.SetIntKey("TRUSTED_ANCHOR",
                   static_cast<int>(CertificateTrustType::TRUSTED_ANCHOR));
    dict.SetIntKey("TRUSTED_ANCHOR_WITH_CONSTRAINTS",
                   static_cast<int>(
                       CertificateTrustType::TRUSTED_ANCHOR_WITH_CONSTRAINTS));

    static_assert(CertificateTrustType::LAST ==
                      CertificateTrustType::TRUSTED_ANCHOR_WITH_CONSTRAINTS,
                  "Update with new flags");

    constants_dict.SetKey("certificateTrustType", std::move(dict));
  }

  // Add a dictionary with information about the relationship between load flag
  // enums and their symbolic names.
  {
    base::Value dict(base::Value::Type::DICTIONARY);

    for (const auto& flag : kLoadFlags)
      dict.SetIntKey(flag.name, flag.constant);

    constants_dict.SetKey("loadFlag", std::move(dict));
  }

  // Add a dictionary with information about the relationship between load state
  // enums and their symbolic names.
  {
    base::Value dict(base::Value::Type::DICTIONARY);

    for (const auto& state : kLoadStateTable)
      dict.SetIntKey(state.name, state.constant);

    constants_dict.SetKey("loadState", std::move(dict));
  }

  // Add information on the relationship between net error codes and their
  // symbolic names.
  {
    base::Value dict(base::Value::Type::DICTIONARY);

    for (const auto& error : kNetErrors)
      dict.SetIntKey(ErrorToShortString(error), error);

    constants_dict.SetKey("netError", std::move(dict));
  }

  // Add information on the relationship between QUIC error codes and their
  // symbolic names.
  {
    base::Value dict(base::Value::Type::DICTIONARY);

    for (quic::QuicErrorCode error = quic::QUIC_NO_ERROR;
         error < quic::QUIC_LAST_ERROR;
         error = static_cast<quic::QuicErrorCode>(error + 1)) {
      dict.SetIntKey(QuicErrorCodeToString(error), static_cast<int>(error));
    }

    constants_dict.SetKey("quicError", std::move(dict));
  }

  // Add information on the relationship between QUIC RST_STREAM error codes
  // and their symbolic names.
  {
    base::Value dict(base::Value::Type::DICTIONARY);

    for (quic::QuicRstStreamErrorCode error = quic::QUIC_STREAM_NO_ERROR;
         error < quic::QUIC_STREAM_LAST_ERROR;
         error = static_cast<quic::QuicRstStreamErrorCode>(error + 1)) {
      dict.SetIntKey(QuicRstStreamErrorCodeToString(error),
                     static_cast<int>(error));
    }

    constants_dict.SetKey("quicRstStreamError", std::move(dict));
  }

  // Information about the relationship between event phase enums and their
  // symbolic names.
  {
    base::Value dict(base::Value::Type::DICTIONARY);

    dict.SetIntKey("PHASE_BEGIN", static_cast<int>(NetLogEventPhase::BEGIN));
    dict.SetIntKey("PHASE_END", static_cast<int>(NetLogEventPhase::END));
    dict.SetIntKey("PHASE_NONE", static_cast<int>(NetLogEventPhase::NONE));

    constants_dict.SetKey("logEventPhase", std::move(dict));
  }

  // Information about the relationship between source type enums and
  // their symbolic names.
  constants_dict.SetKey("logSourceType", NetLog::GetSourceTypesAsValue());

  // Information about the relationship between address family enums and
  // their symbolic names.
  {
    base::Value dict(base::Value::Type::DICTIONARY);

    dict.SetIntKey("ADDRESS_FAMILY_UNSPECIFIED", ADDRESS_FAMILY_UNSPECIFIED);
    dict.SetIntKey("ADDRESS_FAMILY_IPV4", ADDRESS_FAMILY_IPV4);
    dict.SetIntKey("ADDRESS_FAMILY_IPV6", ADDRESS_FAMILY_IPV6);

    constants_dict.SetKey("addressFamily", std::move(dict));
  }

  // Information about how the "time ticks" values we have given it relate to
  // actual system times.  Time ticks are used throughout since they are stable
  // across system clock changes. Note: |timeTickOffset| is only comparable to
  // TimeTicks values in milliseconds.
  // TODO(csharrison): This is an imprecise way to convert TimeTicks to unix
  // time. In fact, there isn't really a good way to do this unless we log Time
  // and TimeTicks values side by side for every event. crbug.com/593157 tracks
  // a change where the user will be notified if a timing anomaly occured that
  // would skew the conversion (i.e. the machine entered suspend mode while
  // logging).
  {
    base::TimeDelta time_since_epoch =
        base::Time::Now() - base::Time::UnixEpoch();
    base::TimeDelta reference_time_ticks =
        base::TimeTicks::Now() - base::TimeTicks();
    int64_t tick_to_unix_time_ms =
        (time_since_epoch - reference_time_ticks).InMilliseconds();
    constants_dict.SetKey("timeTickOffset",
                          NetLogNumberValue(tick_to_unix_time_ms));
  }

  // TODO(eroman): Is this needed?
  // "clientInfo" key is required for some log readers. Provide a default empty
  // value for compatibility.
  constants_dict.SetKey("clientInfo",
                        base::Value(base::Value::Type::DICTIONARY));

  // Add a list of field experiments active at the start of the capture.
  // Additional trials may be enabled later in the browser session.
  constants_dict.SetKey(kNetInfoFieldTrials, GetActiveFieldTrialList());

  return constants_dict;
}

NET_EXPORT base::Value GetNetInfo(URLRequestContext* context) {
  // May only be called on the context's thread.
  context->AssertCalledOnValidThread();

  base::Value net_info_dict =
      context->proxy_resolution_service()->GetProxyNetLogValues();

  // Log Host Resolver info.
  {
    HostResolver* host_resolver = context->host_resolver();
    DCHECK(host_resolver);
    HostCache* cache = host_resolver->GetHostCache();
    if (cache) {
      base::Value dict(base::Value::Type::DICTIONARY);
      base::Value dns_config = host_resolver->GetDnsConfigAsValue();
      dict.SetKey("dns_config", std::move(dns_config));

      base::Value cache_info_dict(base::Value::Type::DICTIONARY);
      base::Value cache_contents_list(base::Value::Type::LIST);

      cache_info_dict.SetIntKey("capacity",
                                static_cast<int>(cache->max_entries()));
      cache_info_dict.SetIntKey("network_changes", cache->network_changes());

      base::ListValue* list_value = nullptr;
      if (cache_contents_list.GetAsList(&list_value))
        cache->GetAsListValue(list_value, true /* include_staleness */,
                              HostCache::SerializationType::kDebug);
      cache_info_dict.SetKey("entries", std::move(cache_contents_list));

      dict.SetKey("cache", std::move(cache_info_dict));
      net_info_dict.SetKey(kNetInfoHostResolver, std::move(dict));
    }
  }

  HttpNetworkSession* http_network_session =
      context->http_transaction_factory()->GetSession();

  // Log Socket Pool info.
  {
    net_info_dict.SetKey(kNetInfoSocketPool,
                         base::Value::FromUniquePtrValue(
                             http_network_session->SocketPoolInfoToValue()));
  }

  // Log SPDY Sessions.
  {
    net_info_dict.SetKey(
        kNetInfoSpdySessions,
        base::Value::FromUniquePtrValue(
            http_network_session->SpdySessionPoolInfoToValue()));
  }

  // Log SPDY status.
  {
    base::Value status_dict(base::Value::Type::DICTIONARY);

    status_dict.SetBoolKey("enable_http2",
                           http_network_session->params().enable_http2);

    NextProtoVector alpn_protos;
    http_network_session->GetAlpnProtos(&alpn_protos);
    if (!alpn_protos.empty()) {
      std::string next_protos_string;
      for (NextProto proto : alpn_protos) {
        if (!next_protos_string.empty())
          next_protos_string.append(",");
        next_protos_string.append(NextProtoToString(proto));
      }
      status_dict.SetStringKey("alpn_protos", next_protos_string);
    }

    net_info_dict.SetKey(kNetInfoSpdyStatus, std::move(status_dict));
  }

  // Log ALT_SVC mappings.
  {
    const HttpServerProperties& http_server_properties =
        *context->http_server_properties();
    net_info_dict.SetKey(
        kNetInfoAltSvcMappings,
        http_server_properties.GetAlternativeServiceInfoAsValue());
  }

  // Log QUIC info.
  {
    net_info_dict.SetKey(kNetInfoQuic, http_network_session->QuicInfoToValue());
  }

  // Log HTTP Cache info.
  {
    base::Value info_dict(base::Value::Type::DICTIONARY);
    base::Value stats_dict(base::Value::Type::DICTIONARY);

    disk_cache::Backend* disk_cache = GetDiskCacheBackend(context);

    if (disk_cache) {
      // Extract the statistics key/value pairs from the backend.
      base::StringPairs stats;
      disk_cache->GetStats(&stats);
      for (auto& stat : stats) {
        stats_dict.SetKey(stat.first, base::Value(std::move(stat.second)));
      }
    }
    info_dict.SetKey("stats", std::move(stats_dict));

    net_info_dict.SetKey(kNetInfoHTTPCache, std::move(info_dict));
  }

  // Log Reporting API info.
  {
#if BUILDFLAG(ENABLE_REPORTING)
    ReportingService* reporting_service = context->reporting_service();
    if (reporting_service) {
      base::Value reporting_dict = reporting_service->StatusAsValue();
      NetworkErrorLoggingService* network_error_logging_service =
          context->network_error_logging_service();
      if (network_error_logging_service) {
        reporting_dict.SetKey("networkErrorLogging",
                              network_error_logging_service->StatusAsValue());
      }
      net_info_dict.SetKey(kNetInfoReporting, std::move(reporting_dict));
    } else {
      base::Value reporting_dict(base::Value::Type::DICTIONARY);
      reporting_dict.SetKey("reportingEnabled", base::Value(false));
      net_info_dict.SetKey(kNetInfoReporting, std::move(reporting_dict));
    }

#else   // BUILDFLAG(ENABLE_REPORTING)
    base::Value reporting_dict(base::Value::Type::DICTIONARY);
    reporting_dict.SetKey("reportingEnabled", base::Value(false));
    net_info_dict.SetKey(kNetInfoReporting, std::move(reporting_dict));
#endif  // BUILDFLAG(ENABLE_REPORTING)
  }

  // Log currently-active field trials. New trials may have been enabled since
  // the start of this browser session (crbug.com/1133396).
  net_info_dict.SetKey(kNetInfoFieldTrials, GetActiveFieldTrialList());

  return net_info_dict;
}

NET_EXPORT void CreateNetLogEntriesForActiveObjects(
    const std::set<URLRequestContext*>& contexts,
    NetLog::ThreadSafeObserver* observer) {
  // Put together the list of all requests.
  std::vector<const URLRequest*> requests;
  for (auto* context : contexts) {
    // May only be called on the context's thread.
    context->AssertCalledOnValidThread();
    // Contexts should all be using the same NetLog.
    DCHECK_EQ((*contexts.begin())->net_log(), context->net_log());
    for (auto* request : *context->url_requests()) {
      requests.push_back(request);
    }
  }

  // Sort by creation time.
  std::sort(requests.begin(), requests.end(), RequestCreatedBefore);

  // Create fake events.
  for (auto* request : requests) {
    NetLogEntry entry(NetLogEventType::REQUEST_ALIVE,
                      request->net_log().source(), NetLogEventPhase::BEGIN,
                      request->creation_time(), request->GetStateAsValue());
    observer->OnAddEntry(entry);
  }
}

}  // namespace net
