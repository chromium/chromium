// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/test/test_devtools_list_fetcher.h"

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"

namespace cr_fuchsia {

base::Value GetDevToolsListFromPort(uint16_t port) {
  GURL url(base::StringPrintf("http://127.0.0.1:%d/json/list", port));
  auto request_context = net::CreateTestURLRequestContextBuilder()->Build();
  net::TestDelegate delegate;

  std::unique_ptr<net::URLRequest> request(request_context->CreateRequest(
      url, net::DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  delegate.RunUntilComplete();

  if (delegate.request_status() < 0)
    return base::Value();

  if (request->response_headers()->response_code() != net::HTTP_OK)
    return base::Value();

  const std::string& result = delegate.data_received();
  if (result.empty())
    return base::Value();

  return base::JSONReader::Read(result).value_or(base::Value());
}

}  // namespace cr_fuchsia
