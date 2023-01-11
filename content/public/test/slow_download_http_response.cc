// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/slow_download_http_response.h"

#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"

namespace content {

// static
const char SlowDownloadHttpResponse::kSlowResponseHostName[] =
    "url.handled.by.slow.response";
const char SlowDownloadHttpResponse::kUnknownSizeUrl[] =
    "/download-unknown-size";
const char SlowDownloadHttpResponse::kKnownSizeUrl[] = "/download-known-size";

// static
base::OnceClosure g_finish_last_request;

// static
std::unique_ptr<net::test_server::HttpResponse>
SlowDownloadHttpResponse::HandleSlowDownloadRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url ==
      SlowDownloadHttpResponse::kFinishSlowResponseUrl) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(g_finish_last_request));
    return std::make_unique<net::test_server::BasicHttpResponse>();
  }

  if (request.relative_url != SlowDownloadHttpResponse::kUnknownSizeUrl &&
      request.relative_url != SlowDownloadHttpResponse::kKnownSizeUrl)
    return nullptr;

  return std::make_unique<SlowDownloadHttpResponse>(
      request.relative_url,
      base::BindLambdaForTesting([&](base::OnceClosure start_response,
                                     base::OnceClosure finish_response) {
        // The response is started immediately, but we delay finishing it.
        std::move(start_response).Run();
        // If there's an active slow download request already, we clobber the
        // finish closure so it will never be finished.
        g_finish_last_request = std::move(finish_response);
      }));
}

SlowDownloadHttpResponse::SlowDownloadHttpResponse(
    const std::string& url,
    GotRequestCallback got_request)
    : SlowHttpResponse(std::move(got_request)), url_(url) {}

SlowDownloadHttpResponse::~SlowDownloadHttpResponse() = default;

base::StringPairs SlowDownloadHttpResponse::ResponseHeaders() {
  base::StringPairs response;
  response.emplace_back("Content-type", "application/octet-stream");
  response.emplace_back("Cache-Control", "max-age=0");

  if (base::EqualsCaseInsensitiveASCII(kKnownSizeUrl, url_)) {
    response.emplace_back(
        "Content-Length",
        base::NumberToString(kFirstResponsePartSize + kSecondResponsePartSize));
  }

  return response;
}

}  // namespace content
