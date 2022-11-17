// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/common/test/test_devtools_list_fetcher.h"

#include <memory>
#include <string>

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"

base::Value::List GetDevToolsListFromPort(uint16_t port) {
  GURL url(base::StringPrintf("http://127.0.0.1:%d/json/list", port));
  auto request_context = net::CreateTestURLRequestContextBuilder()->Build();
  net::TestDelegate delegate;

  std::unique_ptr<net::URLRequest> request(request_context->CreateRequest(
      url, net::DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  delegate.RunUntilComplete();

  if (delegate.request_status() < 0)
    return base::Value::List();

  if (request->response_headers()->response_code() != net::HTTP_OK)
    return base::Value::List();

  const std::string& result = delegate.data_received();
  if (result.empty())
    return base::Value::List();

  return base::JSONReader::Read(result)
      .value_or(base::Value())
      .GetList()
      .Clone();
}
