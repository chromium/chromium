// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"

#include "services/network/public/mojom/load_timing_info.mojom-blink.h"

namespace blink {

ResourceLoadTiming::ResourceLoadTiming() = default;

ResourceLoadTiming::ResourceLoadTiming(
    base::TimeTicks request_time,
    base::TimeTicks proxy_start,
    base::TimeTicks proxy_end,
    base::TimeTicks domain_lookup_start,
    base::TimeTicks domain_lookup_end,
    base::TimeTicks connect_start,
    base::TimeTicks connect_end,
    base::TimeTicks worker_start,
    base::TimeTicks worker_ready,
    base::TimeTicks worker_fetch_start,
    base::TimeTicks worker_respond_with_settled,
    base::TimeTicks send_start,
    base::TimeTicks send_end,
    base::TimeTicks receive_headers_start,
    base::TimeTicks receive_headers_end,
    base::TimeTicks receive_non_informational_headers_start,
    base::TimeTicks receive_early_hints_start,
    base::TimeTicks ssl_start,
    base::TimeTicks ssl_end,
    base::TimeTicks push_start,
    base::TimeTicks push_end)
    : request_time_(request_time),
      proxy_start_(proxy_start),
      proxy_end_(proxy_end),
      domain_lookup_start_(domain_lookup_start),
      domain_lookup_end_(domain_lookup_end),
      connect_start_(connect_start),
      connect_end_(connect_end),
      worker_start_(worker_start),
      worker_ready_(worker_ready),
      worker_fetch_start_(worker_fetch_start),
      worker_respond_with_settled_(worker_respond_with_settled),
      send_start_(send_start),
      send_end_(send_end),
      receive_headers_start_(receive_headers_start),
      receive_headers_end_(receive_headers_end),
      receive_non_informational_headers_start_(
          receive_non_informational_headers_start),
      receive_early_hints_start_(receive_early_hints_start),
      ssl_start_(ssl_start),
      ssl_end_(ssl_end),
      push_start_(push_start),
      push_end_(push_end) {}

scoped_refptr<ResourceLoadTiming> ResourceLoadTiming::Create() {
  return base::AdoptRef(new ResourceLoadTiming);
}

network::mojom::blink::LoadTimingInfoPtr ResourceLoadTiming::ToMojo() const {
  network::mojom::blink::LoadTimingInfoPtr timing =
      network::mojom::blink::LoadTimingInfo::New(
          false, 0, base::Time(), request_time_, proxy_start_, proxy_end_,
          network::mojom::blink::LoadTimingInfoConnectTiming::New(
              domain_lookup_start_, domain_lookup_end_, connect_start_,
              connect_end_, ssl_start_, ssl_end_),
          send_start_, send_end_, receive_headers_start_, receive_headers_end_,
          receive_non_informational_headers_start_, receive_early_hints_start_,
          push_start_, push_end_, worker_start_, worker_ready_,
          worker_fetch_start_, worker_respond_with_settled_);
  return timing;
}

void ResourceLoadTiming::SetDomainLookupStart(
    base::TimeTicks domain_lookup_start) {
  domain_lookup_start_ = domain_lookup_start;
}

void ResourceLoadTiming::SetRequestTime(base::TimeTicks request_time) {
  request_time_ = request_time;
}

void ResourceLoadTiming::SetProxyStart(base::TimeTicks proxy_start) {
  proxy_start_ = proxy_start;
}

void ResourceLoadTiming::SetProxyEnd(base::TimeTicks proxy_end) {
  proxy_end_ = proxy_end;
}

void ResourceLoadTiming::SetDomainLookupEnd(base::TimeTicks domain_lookup_end) {
  domain_lookup_end_ = domain_lookup_end;
}

void ResourceLoadTiming::SetConnectStart(base::TimeTicks connect_start) {
  connect_start_ = connect_start;
}

void ResourceLoadTiming::SetConnectEnd(base::TimeTicks connect_end) {
  connect_end_ = connect_end;
}

void ResourceLoadTiming::SetWorkerStart(base::TimeTicks worker_start) {
  worker_start_ = worker_start;
}

void ResourceLoadTiming::SetWorkerReady(base::TimeTicks worker_ready) {
  worker_ready_ = worker_ready;
}

void ResourceLoadTiming::SetWorkerFetchStart(
    base::TimeTicks worker_fetch_start) {
  worker_fetch_start_ = worker_fetch_start;
}

void ResourceLoadTiming::SetWorkerRespondWithSettled(
    base::TimeTicks worker_respond_with_settled) {
  worker_respond_with_settled_ = worker_respond_with_settled;
}

void ResourceLoadTiming::SetSendStart(base::TimeTicks send_start) {
  send_start_ = send_start;
}

void ResourceLoadTiming::SetSendEnd(base::TimeTicks send_end) {
  send_end_ = send_end;
}

void ResourceLoadTiming::SetReceiveHeadersStart(
    base::TimeTicks receive_headers_start) {
  receive_headers_start_ = receive_headers_start;
}

void ResourceLoadTiming::SetReceiveNonInformationalHeaderStart(
    base::TimeTicks time) {
  receive_non_informational_headers_start_ = time;
}
void ResourceLoadTiming::SetReceiveEarlyHintsStart(base::TimeTicks time) {
  receive_early_hints_start_ = time;
}

void ResourceLoadTiming::SetReceiveHeadersEnd(
    base::TimeTicks receive_headers_end) {
  receive_headers_end_ = receive_headers_end;
}

void ResourceLoadTiming::SetSslStart(base::TimeTicks ssl_start) {
  ssl_start_ = ssl_start;
}

void ResourceLoadTiming::SetSslEnd(base::TimeTicks ssl_end) {
  ssl_end_ = ssl_end;
}

void ResourceLoadTiming::SetPushStart(base::TimeTicks push_start) {
  push_start_ = push_start;
}

void ResourceLoadTiming::SetPushEnd(base::TimeTicks push_end) {
  push_end_ = push_end;
}

double ResourceLoadTiming::CalculateMillisecondDelta(
    base::TimeTicks time) const {
  return time.is_null() ? -1 : (time - request_time_).InMillisecondsF();
}

}  // namespace blink
