// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_utils.h"

#include "base/memory/ref_counted.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"
#include "url/gurl.h"

namespace net {

namespace {

quiche::HttpHeaderBlock MakeHeaderBlock() {
  quiche::HttpHeaderBlock headers;
  headers[":status"] = "200";
  headers["date"] = "Thu, 14 Sep 2023 12:40:24 GMT";
  headers["server"] = "server1234.example.com";
  headers["x-content-type-options"] = "nosniff";
  headers["content-language"] = "en";
  headers["accept-ch"] = "";
  headers["vary"] = "Accept-Encoding,Cookie";
  headers["last-modified"] = "Thu, 14 Sep 2023 12:40:22 GMT";
  headers["content-type"] = "text/html; charset=UTF-8";
  headers["age"] = "1984";
  headers["x-cache"] = "server1234 miss, server1235 hit/6664";
  headers["x-cache-status"] = "hit-front";
  headers["server-timing"] = "cache;desc=\"hit-front\", host;desc=\"cp5023\"";
  headers["strict-transport-security"] =
      "max-age=106384710; includeSubDomains; preload";
  headers["report-to"] =
      "{ \"group\": \"wm_nel\", \"max_age\": 604800, \"endpoints\": [{ "
      "\"url\": "
      "\"https://nel.example.net/v1/"
      "events?stream=w3c.reportingapi.network_error&schema_uri=/w3c/"
      "reportingapi/network_error/1.0.0\" }] }";
  headers["nel"] =
      "{ \"report_to\": \"wm_nel\", \"max_age\": 604800, \"failure_fraction\": "
      "0.05, \"success_fraction\": 0.0}";
  headers.AppendValueOrAddHeader(
      "set-cookie",
      "WMF-DP=ba9;Path=/;HttpOnly;secure;Expires=Fri, 15 Sep 2023 00:00:00 "
      "GMT");
  headers["x-client-ip"] = "0102:0203:04:405:0506:0708:0609:090a";
  headers["cache-control"] = "private, s-maxage=0, max-age=0, must-revalidate";
  headers.AppendValueOrAddHeader(
      "set-cookie", "NetworkProbeLimit=0.001;Path=/;Secure;Max-Age=3600");
  headers["accept-ranges"] = "bytes";
  headers["content-length"] = "99545";
  return headers;
}

using SpdyHeadersToHttpResponseHeadersFunctionPtrType =
    base::expected<scoped_refptr<HttpResponseHeaders>, int> (*)(
        const quiche::HttpHeaderBlock&);

// The benchmark code is templated on the function to force it to be specialized
// at compile time so there is no indirection via a function pointer at runtime
// sllowing it down.
template <SpdyHeadersToHttpResponseHeadersFunctionPtrType convert>
void Benchmark(::benchmark::State& state) {
  const auto header_block = MakeHeaderBlock();
  for (auto _ : state) {
    auto headers = convert(header_block);
    ::benchmark::DoNotOptimize(headers);
  }
}

BENCHMARK(Benchmark<SpdyHeadersToHttpResponseHeadersUsingRawString>)
    ->MinWarmUpTime(1.0);
BENCHMARK(Benchmark<SpdyHeadersToHttpResponseHeadersUsingBuilder>)
    ->MinWarmUpTime(1.0);

void BM_CreateSpdyHeadersFromHttpRequest(::benchmark::State& state) {
  HttpRequestInfo info;
  info.url = GURL("https://en.wikipedia.org/wiki/HTTP");
  info.method = "GET";
  HttpRequestHeaders http_request_headers;
  http_request_headers.SetHeader(
      "Accept",
      "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/"
      "webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7");
  http_request_headers.SetHeader("Accept-Encoding", "gzip, deflate, br");
  http_request_headers.SetHeader("Accept-Language", "en-GB,en;q=0.9");
  http_request_headers.SetHeader("Cache-Control", "max-age=0");
  http_request_headers.SetHeader(
      "Cookie",
      "WMF-Last-Access=xxxxxxxxxxx; WMF-Last-Access-Global=xxxxxxxxxxx; "
      "GeoIP=xxxxxxxxxxxxxxxxxxxxxxxxxxx; NetworkProbeLimit=0.001; "
      "enwikimwuser-sessionId=xxxxxxxxxxxxxxxxxxxx");
  http_request_headers.SetHeader(
      "Sec-Ch-Ua",
      "\"Google Chrome\";v=\"117\", \"Not;A=Brand\";v=\"8\", "
      "\"Chromium\";v=\"117\"");
  http_request_headers.SetHeader("Sec-Ch-Ua-Mobile", "?0");
  http_request_headers.SetHeader("Sec-Ch-Ua-Platform", "\"Linux\"");
  http_request_headers.SetHeader("Sec-Fetch-Dest", "document");
  http_request_headers.SetHeader("Sec-Fetch-Mode", "navigate");
  http_request_headers.SetHeader("Sec-Fetch-Site", "none");
  http_request_headers.SetHeader("Sec-Fetch-User", "?1");
  http_request_headers.SetHeader("Upgrade-Insecure-Requests", "1");
  http_request_headers.SetHeader(
      "User-Agent",
      "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
      "Chrome/117.0.0.0 Safari/537.36");

  for (auto _ : state) {
    quiche::HttpHeaderBlock headers;
    CreateSpdyHeadersFromHttpRequest(info, RequestPriority::DEFAULT_PRIORITY,
                                     http_request_headers, &headers);
    ::benchmark::DoNotOptimize(headers);
  }
}

BENCHMARK(BM_CreateSpdyHeadersFromHttpRequest)->MinWarmUpTime(1.0);

}  // namespace

}  // namespace net
