// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_TEST_TEST_NETWORK_INTERCEPTOR_H_
#define HEADLESS_TEST_TEST_NETWORK_INTERCEPTOR_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/test/url_loader_interceptor.h"

namespace headless {

class TestNetworkInterceptor {
 public:
  class Impl;

  TestNetworkInterceptor();

  TestNetworkInterceptor(const TestNetworkInterceptor&) = delete;
  TestNetworkInterceptor& operator=(const TestNetworkInterceptor&) = delete;

  ~TestNetworkInterceptor();

  struct Response {
    Response() = delete;
    Response(const std::string data);
    Response(const std::string body, const std::string& mime_type);
    Response(const Response& r);
    Response(Response&& r);
    ~Response();

    scoped_refptr<net::HttpResponseHeaders> headers;
    std::string raw_headers;
    std::string body;
  };

  void InsertResponse(std::string url, Response response);

  const std::vector<std::string>& urls_requested() const {
    return urls_requested_;
  }

  const std::vector<std::string>& methods_requested() const {
    return methods_requested_;
  }

 private:
  void LogRequest(std::string method, std::string url);

  std::vector<std::string> urls_requested_;
  std::vector<std::string> methods_requested_;

  std::unique_ptr<Impl> impl_;
  std::unique_ptr<content::URLLoaderInterceptor> interceptor_;

  base::WeakPtrFactory<TestNetworkInterceptor> weak_factory_{this};
};

}  // namespace headless

#endif  // HEADLESS_TEST_TEST_NETWORK_INTERCEPTOR_H_
