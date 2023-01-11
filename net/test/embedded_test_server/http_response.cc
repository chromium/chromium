// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/http_response.h"

#include <iterator>
#include <map>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"

namespace net::test_server {

HttpResponseDelegate::HttpResponseDelegate() = default;
HttpResponseDelegate::~HttpResponseDelegate() = default;

HttpResponse::~HttpResponse() = default;

RawHttpResponse::RawHttpResponse(const std::string& headers,
                                 const std::string& contents)
    : headers_(headers), contents_(contents) {}

RawHttpResponse::~RawHttpResponse() = default;

void RawHttpResponse::SendResponse(
    base::WeakPtr<HttpResponseDelegate> delegate) {
  if (!headers_.empty()) {
    std::string response = headers_;
    // LocateEndOfHeadersHelper() searches for the first "\n\n" and "\n\r\n" as
    // the end of the header.
    std::size_t index = response.find_last_not_of("\r\n");
    if (index != std::string::npos)
      response.erase(index + 1);
    response += "\n\n";
    delegate->SendRawResponseHeaders(response);
  }

  delegate->SendContentsAndFinish(contents_);
}

void RawHttpResponse::AddHeader(const std::string& key_value_pair) {
  headers_.append(base::StringPrintf("%s\r\n", key_value_pair.c_str()));
}

BasicHttpResponse::BasicHttpResponse() = default;

BasicHttpResponse::~BasicHttpResponse() = default;

std::string BasicHttpResponse::ToResponseString() const {
  base::StringPairs headers = BuildHeaders();
  // Response line with headers.
  std::string response_builder;

  // TODO(mtomasz): For http/1.0 requests, send http/1.0.

  base::StringAppendF(&response_builder, "HTTP/1.1 %d %s\r\n", code_,
                      reason().c_str());

  for (const auto& header : headers)
    base::StringAppendF(&response_builder, "%s: %s\r\n", header.first.c_str(),
                        header.second.c_str());

  base::StringAppendF(&response_builder, "\r\n");

  return response_builder + content_;
}

base::StringPairs BasicHttpResponse::BuildHeaders() const {
  base::StringPairs headers;
  headers.emplace_back("Connection", "close");
  headers.emplace_back("Content-Length", base::NumberToString(content_.size()));
  headers.emplace_back("Content-Type", content_type_);

  base::ranges::copy(custom_headers_, std::back_inserter(headers));

  return headers;
}

void BasicHttpResponse::SendResponse(
    base::WeakPtr<HttpResponseDelegate> delegate) {
  delegate->SendHeadersContentAndFinish(code_, reason(), BuildHeaders(),
                                        content_);
}

DelayedHttpResponse::DelayedHttpResponse(const base::TimeDelta delay)
    : delay_(delay) {}

DelayedHttpResponse::~DelayedHttpResponse() = default;

void DelayedHttpResponse::SendResponse(
    base::WeakPtr<HttpResponseDelegate> delegate) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&HttpResponseDelegate::SendHeadersContentAndFinish,
                     delegate, code(), reason(), BuildHeaders(), content()),
      delay_);
}

void HungResponse::SendResponse(base::WeakPtr<HttpResponseDelegate> delegate) {}

HungAfterHeadersHttpResponse::HungAfterHeadersHttpResponse(
    base::StringPairs headers)
    : headers_(headers) {}
HungAfterHeadersHttpResponse::~HungAfterHeadersHttpResponse() = default;

void HungAfterHeadersHttpResponse::SendResponse(
    base::WeakPtr<HttpResponseDelegate> delegate) {
  delegate->SendResponseHeaders(HTTP_OK, "OK", headers_);
}

}  // namespace net::test_server
