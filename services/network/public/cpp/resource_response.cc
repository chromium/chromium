// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/resource_response.h"

#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/content_security_policy.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network {

ResourceResponseHead::ResourceResponseHead() = default;
ResourceResponseHead::~ResourceResponseHead() = default;
ResourceResponseHead::ResourceResponseHead(const ResourceResponseHead& other) =
    default;

ResourceResponseHead::ResourceResponseHead(
    const mojom::URLResponseHeadPtr& url_response_head) {
  request_time = url_response_head->request_time;
  response_time = url_response_head->response_time;
  headers = url_response_head->headers;
  mime_type = url_response_head->mime_type;
  charset = url_response_head->charset;
  ct_policy_compliance = url_response_head->ct_policy_compliance;
  content_length = url_response_head->content_length;
  encoded_data_length = url_response_head->encoded_data_length;
  encoded_body_length = url_response_head->encoded_body_length;
  network_accessed = url_response_head->network_accessed;
  appcache_id = url_response_head->appcache_id;
  appcache_manifest_url = url_response_head->appcache_manifest_url;
  load_timing = url_response_head->load_timing;
  if (url_response_head->raw_request_response_info) {
    raw_request_response_info = new network::HttpRawRequestResponseInfo;
    raw_request_response_info->http_status_code =
        url_response_head->raw_request_response_info->http_status_code;
    raw_request_response_info->http_status_text =
        url_response_head->raw_request_response_info->http_status_text;
    for (auto& header :
         url_response_head->raw_request_response_info->request_headers) {
      raw_request_response_info->request_headers.push_back(
          std::make_pair(header->key, header->value));
    }
    for (auto& header :
         url_response_head->raw_request_response_info->response_headers) {
      raw_request_response_info->response_headers.push_back(
          std::make_pair(header->key, header->value));
    }
    raw_request_response_info->request_headers_text =
        url_response_head->raw_request_response_info->request_headers_text;
    raw_request_response_info->response_headers_text =
        url_response_head->raw_request_response_info->response_headers_text;
  }
  was_fetched_via_spdy = url_response_head->was_fetched_via_spdy;
  was_alpn_negotiated = url_response_head->was_alpn_negotiated;
  was_alternate_protocol_available =
      url_response_head->was_alternate_protocol_available;
  connection_info = url_response_head->connection_info;
  alpn_negotiated_protocol = url_response_head->alpn_negotiated_protocol;
  remote_endpoint = url_response_head->remote_endpoint;
  was_fetched_via_cache = url_response_head->was_fetched_via_cache;
  proxy_server = url_response_head->proxy_server;
  was_fetched_via_service_worker =
      url_response_head->was_fetched_via_service_worker;
  was_fallback_required_by_service_worker =
      url_response_head->was_fallback_required_by_service_worker;
  url_list_via_service_worker = url_response_head->url_list_via_service_worker;
  response_type = url_response_head->response_type;
  service_worker_start_time = url_response_head->service_worker_start_time;
  service_worker_ready_time = url_response_head->service_worker_ready_time;
  is_in_cache_storage = url_response_head->is_in_cache_storage;
  cache_storage_cache_name = url_response_head->cache_storage_cache_name;
  cert_status = url_response_head->cert_status;
  ssl_info = url_response_head->ssl_info;
  cors_exposed_header_names = url_response_head->cors_exposed_header_names;
  did_service_worker_navigation_preload =
      url_response_head->did_service_worker_navigation_preload;
  should_report_corb_blocking = url_response_head->should_report_corb_blocking;
  async_revalidation_requested =
      url_response_head->async_revalidation_requested;
  did_mime_sniff = url_response_head->did_mime_sniff;
  is_signed_exchange_inner_response =
      url_response_head->is_signed_exchange_inner_response;
  was_in_prefetch_cache = url_response_head->was_in_prefetch_cache;
  intercepted_by_plugin = url_response_head->intercepted_by_plugin;
  is_legacy_tls_version = url_response_head->is_legacy_tls_version;
  auth_challenge_info = url_response_head->auth_challenge_info;
  content_security_policy =
      ContentSecurityPolicy(url_response_head->content_security_policy.Clone());
  request_start = url_response_head->request_start;
  response_start = url_response_head->response_start;
  origin_policy = url_response_head->origin_policy;
  recursive_prefetch_token = url_response_head->recursive_prefetch_token;
}

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
  new_response->head.remote_endpoint = head.remote_endpoint;
  new_response->head.was_fetched_via_cache = head.was_fetched_via_cache;
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
  new_response->head.was_in_prefetch_cache = head.was_in_prefetch_cache;
  new_response->head.intercepted_by_plugin = head.intercepted_by_plugin;
  new_response->head.is_legacy_tls_version = head.is_legacy_tls_version;
  new_response->head.auth_challenge_info = head.auth_challenge_info;
  new_response->head.content_security_policy = head.content_security_policy;
  new_response->head.origin_policy = head.origin_policy;
  new_response->head.recursive_prefetch_token = head.recursive_prefetch_token;
  return new_response;
}

ResourceResponseHead::operator mojom::URLResponseHeadPtr() const {
  network::mojom::HttpRawRequestResponseInfoPtr info = nullptr;
  if (raw_request_response_info) {
    info = network::mojom::HttpRawRequestResponseInfo::New();
    info->http_status_code = raw_request_response_info->http_status_code;
    info->http_status_text = raw_request_response_info->http_status_text;
    for (auto& header : raw_request_response_info->request_headers) {
      info->request_headers.push_back(
          network::mojom::HttpRawHeaderPair::New(header.first, header.second));
    }
    for (auto& header : raw_request_response_info->response_headers) {
      info->response_headers.push_back(
          network::mojom::HttpRawHeaderPair::New(header.first, header.second));
    }
    info->request_headers_text =
        raw_request_response_info->request_headers_text;
    info->response_headers_text =
        raw_request_response_info->response_headers_text;
  }

  return mojom::URLResponseHead::New(
      request_time, response_time, headers, mime_type, charset,
      ct_policy_compliance, content_length, encoded_data_length,
      encoded_body_length, network_accessed, appcache_id, appcache_manifest_url,
      load_timing, std::move(info), was_fetched_via_spdy, was_alpn_negotiated,
      was_alternate_protocol_available, connection_info,
      alpn_negotiated_protocol, remote_endpoint, was_fetched_via_cache,
      proxy_server, was_fetched_via_service_worker,
      was_fallback_required_by_service_worker, url_list_via_service_worker,
      response_type, service_worker_start_time, service_worker_ready_time,
      is_in_cache_storage, cache_storage_cache_name, cert_status, ssl_info,
      cors_exposed_header_names, did_service_worker_navigation_preload,
      should_report_corb_blocking, async_revalidation_requested, did_mime_sniff,
      is_signed_exchange_inner_response, was_in_prefetch_cache,
      intercepted_by_plugin, is_legacy_tls_version, auth_challenge_info,
      content_security_policy, request_start, response_start, origin_policy,
      recursive_prefetch_token);
}

}  // namespace network
