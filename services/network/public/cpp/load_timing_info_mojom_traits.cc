// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/load_timing_info_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::LoadTimingInfoConnectTimingDataView,
                  net::LoadTimingInfo::ConnectTiming>::
    Read(network::mojom::LoadTimingInfoConnectTimingDataView data,
         net::LoadTimingInfo::ConnectTiming* out) {
  return data.ReadDomainLookupStart(&out->domain_lookup_start) &&
         data.ReadDomainLookupEnd(&out->domain_lookup_end) &&
         data.ReadConnectStart(&out->connect_start) &&
         data.ReadConnectEnd(&out->connect_end) &&
         data.ReadSslStart(&out->ssl_start) && data.ReadSslEnd(&out->ssl_end);
}

// static
bool StructTraits<network::mojom::LoadTimingInfoDataView, net::LoadTimingInfo>::
    Read(network::mojom::LoadTimingInfoDataView data,
         net::LoadTimingInfo* out) {
  out->socket_reused = data.socket_reused();
  out->socket_log_id = data.socket_log_id();
  return data.ReadRequestStartTime(&out->request_start_time) &&
         data.ReadRequestStart(&out->request_start) &&
         data.ReadProxyResolveStart(&out->proxy_resolve_start) &&
         data.ReadProxyResolveEnd(&out->proxy_resolve_end) &&
         data.ReadConnectTiming(&out->connect_timing) &&
         data.ReadSendStart(&out->send_start) &&
         data.ReadSendEnd(&out->send_end) &&
         data.ReadReceiveHeadersStart(&out->receive_headers_start) &&
         data.ReadReceiveHeadersEnd(&out->receive_headers_end) &&
         data.ReadReceiveNonInformationalHeadersStart(
             &out->receive_non_informational_headers_start) &&
         data.ReadFirstEarlyHintsTime(&out->first_early_hints_time) &&
         data.ReadPushStart(&out->push_start) &&
         data.ReadPushEnd(&out->push_end) &&
         data.ReadServiceWorkerStartTime(&out->service_worker_start_time) &&
         data.ReadServiceWorkerReadyTime(&out->service_worker_ready_time) &&
         data.ReadServiceWorkerRouterEvaluationStart(
             &out->service_worker_router_evaluation_start) &&
         data.ReadServiceWorkerCacheLookupStart(
             &out->service_worker_cache_lookup_start) &&
         data.ReadServiceWorkerFetchStart(&out->service_worker_fetch_start) &&
         data.ReadServiceWorkerRespondWithSettled(
             &out->service_worker_respond_with_settled);
}

}  // namespace mojo
