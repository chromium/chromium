// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/resource_timing_info_conversions.h"

#include <string>
#include <vector>

#include "content/common/resource_timing_info.h"
#include "third_party/blink/public/platform/web_resource_timing_info.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace content {

ResourceTimingInfo WebResourceTimingInfoToResourceTimingInfo(
    const blink::WebResourceTimingInfo& info) {
  // TODO(dcheng): This is sad. Move this to Mojo.
  ResourceTimingInfo resource_timing;
  resource_timing.name = info.name.Utf8();
  resource_timing.start_time = info.start_time;

  resource_timing.alpn_negotiated_protocol =
      info.alpn_negotiated_protocol.Utf8();
  resource_timing.connection_info = info.connection_info.Utf8();

  if (!info.timing.IsNull()) {
    resource_timing.timing.emplace();
    resource_timing.timing->request_time = info.timing.RequestTime();
    resource_timing.timing->proxy_start = info.timing.ProxyStart();
    resource_timing.timing->proxy_end = info.timing.ProxyEnd();
    resource_timing.timing->dns_start = info.timing.DnsStart();
    resource_timing.timing->dns_end = info.timing.DnsEnd();
    resource_timing.timing->connect_start = info.timing.ConnectStart();
    resource_timing.timing->connect_end = info.timing.ConnectEnd();
    resource_timing.timing->worker_start = info.timing.WorkerStart();
    resource_timing.timing->worker_ready = info.timing.WorkerReady();
    resource_timing.timing->send_start = info.timing.SendStart();
    resource_timing.timing->send_end = info.timing.SendEnd();
    resource_timing.timing->receive_headers_start =
        info.timing.ReceiveHeadersStart();
    resource_timing.timing->receive_headers_end =
        info.timing.ReceiveHeadersEnd();
    resource_timing.timing->ssl_start = info.timing.SslStart();
    resource_timing.timing->ssl_end = info.timing.SslEnd();
    resource_timing.timing->push_start = info.timing.PushStart();
    resource_timing.timing->push_end = info.timing.PushEnd();
  }

  resource_timing.last_redirect_end_time = info.last_redirect_end_time;
  resource_timing.response_end = info.response_end;

  resource_timing.transfer_size = info.transfer_size;
  resource_timing.encoded_body_size = info.encoded_body_size;
  resource_timing.decoded_body_size = info.decoded_body_size;

  resource_timing.did_reuse_connection = info.did_reuse_connection;

  resource_timing.allow_timing_details = info.allow_timing_details;
  resource_timing.allow_redirect_details = info.allow_redirect_details;

  resource_timing.allow_negative_values = info.allow_negative_values;

  for (const auto& entry : info.server_timing) {
    resource_timing.server_timing.emplace_back();
    auto& new_entry = resource_timing.server_timing.back();
    new_entry.name = entry.name.Utf8();
    new_entry.duration = entry.duration;
    new_entry.description = entry.description.Utf8();
  }

  return resource_timing;
}

blink::WebResourceTimingInfo ResourceTimingInfoToWebResourceTimingInfo(
    const ResourceTimingInfo& resource_timing) {
  // TODO(dcheng): This is sad. Move this to Mojo.
  blink::WebResourceTimingInfo info;
  info.name = blink::WebString::FromUTF8(resource_timing.name);
  info.start_time = resource_timing.start_time;

  info.alpn_negotiated_protocol =
      blink::WebString::FromUTF8(resource_timing.alpn_negotiated_protocol);
  info.connection_info =
      blink::WebString::FromUTF8(resource_timing.connection_info);

  if (resource_timing.timing) {
    info.timing.Initialize();
    info.timing.SetRequestTime(resource_timing.timing->request_time);
    info.timing.SetProxyStart(resource_timing.timing->proxy_start);
    info.timing.SetProxyEnd(resource_timing.timing->proxy_end);
    info.timing.SetDNSStart(resource_timing.timing->dns_start);
    info.timing.SetDNSEnd(resource_timing.timing->dns_end);
    info.timing.SetConnectStart(resource_timing.timing->connect_start);
    info.timing.SetConnectEnd(resource_timing.timing->connect_end);
    info.timing.SetWorkerStart(resource_timing.timing->worker_start);
    info.timing.SetWorkerReady(resource_timing.timing->worker_ready);
    info.timing.SetSendStart(resource_timing.timing->send_start);
    info.timing.SetSendEnd(resource_timing.timing->send_end);
    info.timing.SetReceiveHeadersStart(
        resource_timing.timing->receive_headers_start);
    info.timing.SetReceiveHeadersEnd(
        resource_timing.timing->receive_headers_end);
    info.timing.SetSSLStart(resource_timing.timing->ssl_start);
    info.timing.SetSSLEnd(resource_timing.timing->ssl_end);
    info.timing.SetPushStart(resource_timing.timing->push_start);
    info.timing.SetPushEnd(resource_timing.timing->push_end);
  }

  info.last_redirect_end_time = resource_timing.last_redirect_end_time;
  info.response_end = resource_timing.response_end;

  info.transfer_size = resource_timing.transfer_size;
  info.encoded_body_size = resource_timing.encoded_body_size;
  info.decoded_body_size = resource_timing.decoded_body_size;

  info.did_reuse_connection = resource_timing.did_reuse_connection;
  // TODO(https://crbug.com/970242): This may result in errounous reporting of
  // iframes with different schemes than its parent frame.
  info.is_secure_context = false;

  info.allow_timing_details = resource_timing.allow_timing_details;
  info.allow_redirect_details = resource_timing.allow_redirect_details;

  info.allow_negative_values = resource_timing.allow_negative_values;

  info.server_timing.reserve(resource_timing.server_timing.size());
  for (const auto& server_timing : resource_timing.server_timing) {
    info.server_timing.emplace_back(
        blink::WebString::FromUTF8(server_timing.name), server_timing.duration,
        blink::WebString::FromUTF8(server_timing.description));
  }

  return info;
}

}  // namespace content
