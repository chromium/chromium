// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_utils.h"

#include "base/memory/ref_counted.h"
#include "net/http/http_response_headers.h"
#include "net/third_party/quiche/src/quiche/spdy/core/http2_header_block.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace net {

namespace {

spdy::Http2HeaderBlock MakeHeaderBlock() {
  spdy::Http2HeaderBlock headers;
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
        const spdy::Http2HeaderBlock&);

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

}  // namespace

}  // namespace net
