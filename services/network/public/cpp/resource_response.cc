// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/resource_response.h"

#include "net/http/http_response_headers.h"

namespace network {

scoped_refptr<ResourceResponse> ResourceResponse::DeepCopy() const {
  scoped_refptr<ResourceResponse> new_response(new ResourceResponse);
  new_response->head.request_time = head.request_time;
  new_response->head.response_time = head.response_time;
  if (head.headers.get()) {
    new_response->head.headers =
        new net::HttpResponseHeaders(head.headers->raw_headers());
  }
  new_response->head.mime_type = head.mime_type;
  new_response->head.charset = head.charset;
  new_response->head.ct_policy_compliance = head.ct_policy_compliance;
  new_response->head.is_legacy_symantec_cert = head.is_legacy_symantec_cert;
  new_response->head.content_length = head.content_length;
  new_response->head.network_accessed = head.network_accessed;
  new_response->head.encoded_data_length = head.encoded_data_length;
  new_response->head.encoded_body_length = head.encoded_body_length;
  new_response->head.appcache_id = head.appcache_id;
  new_response->head.appcache_manifest_url = head.appcache_manifest_url;
  new_response->head.load_timing = head.load_timing;
  if (head.raw_request_response_info.get()) {
    new_response->head.raw_request_response_info =
        head.raw_request_response_info->DeepCopy();
  }
  new_response->head.was_fetched_via_spdy = head.was_fetched_via_spdy;
  new_response->head.was_alpn_negotiated = head.was_alpn_negotiated;
  new_response->head.was_alternate_protocol_available =
      head.was_alternate_protocol_available;
  new_response->head.connection_info = head.connection_info;
  new_response->head.alpn_negotiated_protocol = head.alpn_negotiated_protocol;
  new_response->head.socket_address = head.socket_address;
  new_response->head.was_fetched_via_cache = head.was_fetched_via_cache;
  new_response->head.was_fetched_via_proxy = head.was_fetched_via_proxy;
  new_response->head.proxy_server = head.proxy_server;
  new_response->head.was_fetched_via_service_worker =
      head.was_fetched_via_service_worker;
  new_response->head.was_fallback_required_by_service_worker =
      head.was_fallback_required_by_service_worker;
  new_response->head.url_list_via_service_worker =
      head.url_list_via_service_worker;
  new_response->head.response_type = head.response_type;
  new_response->head.service_worker_start_time = head.service_worker_start_time;
  new_response->head.service_worker_ready_time = head.service_worker_ready_time;
  new_response->head.is_in_cache_storage = head.is_in_cache_storage;
  new_response->head.cache_storage_cache_name = head.cache_storage_cache_name;
  new_response->head.effective_connection_type = head.effective_connection_type;
  new_response->head.cert_status = head.cert_status;
  new_response->head.ssl_info = head.ssl_info;
  new_response->head.cors_exposed_header_names = head.cors_exposed_header_names;
  new_response->head.did_service_worker_navigation_preload =
      head.did_service_worker_navigation_preload;
  new_response->head.should_report_corb_blocking =
      head.should_report_corb_blocking;
  new_response->head.async_revalidation_requested =
      head.async_revalidation_requested;
  new_response->head.did_mime_sniff = head.did_mime_sniff;
  new_response->head.is_signed_exchange_inner_response =
      head.is_signed_exchange_inner_response;
  new_response->head.intercepted_by_plugin = head.intercepted_by_plugin;
  return new_response;
}

}  // namespace network
