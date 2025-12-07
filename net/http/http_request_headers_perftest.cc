// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_request_headers.h"

#include <string>

#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace net {

namespace {

void BM_HttpRequestHeadersToString(::benchmark::State& state) {
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
    std::string as_string = http_request_headers.ToString();
    ::benchmark::DoNotOptimize(as_string);
  }
}

BENCHMARK(BM_HttpRequestHeadersToString)->MinWarmUpTime(1.0);

}  // namespace

}  // namespace net
