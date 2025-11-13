// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/cpp/devtools_observer_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/service_worker_router_info.mojom-shared.h"
#include "services/network/public/mojom/service_worker_router_info.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(DevToolsObserverUtilTest, ExtractURLResponseHead) {
  base::test::SingleThreadTaskEnvironment task_environment;

  mojom::URLResponseHead head;
  head.response_time = base::Time() + base::Microseconds(10);
  head.headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\0");
  head.mime_type = "mime_type";
  head.load_timing.first_early_hints_time =
      base::TimeTicks() + base::Microseconds(11);
  head.cert_status = 12;
  head.encoded_data_length = 13;
  head.was_in_prefetch_cache = true;
  head.was_fetched_via_service_worker = false;
  head.cache_storage_cache_name = "cache storage name";
  head.alpn_negotiated_protocol = "alpn";
  head.was_fetched_via_spdy = true;
  head.service_worker_response_source = mojom::FetchResponseSource::kHttpCache;
  head.service_worker_router_info = mojom::ServiceWorkerRouterInfo::New(
      /*rule_id_matched*/ 1, mojom::ServiceWorkerRouterSourceType::kNetwork,
      mojom::ServiceWorkerRouterSourceType::kNetwork, 1, base::TimeDelta::Min(),
      base::TimeDelta::Min(), mojom::ServiceWorkerStatus::kRunning);
  head.ssl_info = net::SSLInfo();
  head.remote_endpoint = net::IPEndPoint(net::IPAddress(1, 2, 3, 4), 99);

  mojom::URLResponseHeadDevToolsInfoPtr head_info = ExtractDevToolsInfo(head);

  EXPECT_EQ(head_info->response_time, head.response_time);
  ASSERT_EQ(head_info->headers.get(), head.headers.get());
  EXPECT_EQ(head_info->mime_type, head.mime_type);
  EXPECT_EQ(head_info->load_timing.first_early_hints_time,
            head.load_timing.first_early_hints_time);
  EXPECT_EQ(head_info->cert_status, head.cert_status);
  EXPECT_EQ(head_info->encoded_data_length, head.encoded_data_length);
  EXPECT_EQ(head_info->was_in_prefetch_cache, head.was_in_prefetch_cache);
  EXPECT_EQ(head_info->was_fetched_via_service_worker,
            head.was_fetched_via_service_worker);
  EXPECT_EQ(head_info->cache_storage_cache_name, head.cache_storage_cache_name);
  EXPECT_EQ(head_info->alpn_negotiated_protocol, head.alpn_negotiated_protocol);
  EXPECT_EQ(head_info->was_fetched_via_spdy, head.was_fetched_via_spdy);
  EXPECT_EQ(head_info->service_worker_response_source,
            head.service_worker_response_source);
  ASSERT_TRUE(head_info->service_worker_router_info);
  EXPECT_EQ(*head_info->service_worker_router_info,
            *head.service_worker_router_info);
  EXPECT_EQ(head_info->ssl_info.has_value(), head.ssl_info.has_value());
  EXPECT_EQ(head_info->remote_endpoint, head.remote_endpoint);
}

TEST(DevToolsObserverUtilTest, ExtractResourceRequest) {
  base::test::SingleThreadTaskEnvironment task_environment;

  GURL url("http://example.org");
  ResourceRequest request;
  request.method = "method";
  request.url = url;
  request.priority = net::RequestPriority::MAXIMUM_PRIORITY;
  request.referrer_policy =
      net::ReferrerPolicy::CLEAR_ON_TRANSITION_CROSS_ORIGIN;
  request.trust_token_params = mojom::TrustTokenParams();

  mojom::URLRequestDevToolsInfoPtr request_info = ExtractDevToolsInfo(request);

  EXPECT_EQ(request_info->method, request.method);
  EXPECT_EQ(request_info->url, request.url);
  EXPECT_EQ(request_info->priority, request.priority);
  EXPECT_EQ(request_info->referrer_policy, request.referrer_policy);
  EXPECT_EQ(*request_info->trust_token_params,
            request.trust_token_params.value());
}

}  // namespace network
