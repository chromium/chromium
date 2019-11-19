// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/test_devtools_list_fetcher.h"

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_test_util.h"

namespace {

// Utility class to get the JSON value of the list URL for a DevTools service on
// localhost.
class DevToolsListFetcher : public net::URLFetcherDelegate {
 public:
  DevToolsListFetcher() {
    request_context_getter_ =
        base::MakeRefCounted<net::TestURLRequestContextGetter>(
            base::ThreadTaskRunnerHandle::Get());
  }
  ~DevToolsListFetcher() override = default;

  base::Value GetDevToolsListFromPort(uint16_t port) {
    std::string url = base::StringPrintf("http://127.0.0.1:%d/json/list", port);
    std::unique_ptr<net::URLFetcher> fetcher = net::URLFetcher::Create(
        GURL(url), net::URLFetcher::GET, this, TRAFFIC_ANNOTATION_FOR_TESTS);
    fetcher->SetRequestContext(request_context_getter_.get());
    fetcher->Start();

    base::RunLoop run_loop;
    on_url_fetch_complete_ack_ = run_loop.QuitClosure();
    run_loop.Run();

    if (fetcher->GetStatus().status() != net::URLRequestStatus::SUCCESS)
      return base::Value();

    if (fetcher->GetResponseCode() != net::HTTP_OK)
      return base::Value();

    std::string result;
    if (!fetcher->GetResponseAsString(&result))
      return base::Value();

    return base::JSONReader::Read(result).value_or(base::Value());
  }

 private:
  // fuchsia::web::URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override {
    DCHECK(on_url_fetch_complete_ack_);
    std::move(on_url_fetch_complete_ack_).Run();
  }

  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  base::OnceClosure on_url_fetch_complete_ack_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsListFetcher);
};

}  // namespace

namespace cr_fuchsia {

base::Value GetDevToolsListFromPort(uint16_t port) {
  DevToolsListFetcher devtools_fetcher;
  return devtools_fetcher.GetDevToolsListFromPort(port);
}

}  // namespace cr_fuchsia
