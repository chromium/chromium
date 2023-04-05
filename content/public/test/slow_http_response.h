// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SLOW_HTTP_RESPONSE_H_
#define CONTENT_PUBLIC_TEST_SLOW_HTTP_RESPONSE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace content {

// An HTTP response that may not complete ever.
class SlowHttpResponse : public net::test_server::HttpResponse {
  using HttpResponseDelegate = net::test_server::HttpResponseDelegate;

 public:
  // Test URLs.
  static const char kSlowResponseUrl[];
  static const char kFinishSlowResponseUrl[];
  static const int kFirstResponsePartSize;
  static const int kSecondResponsePartSize;

  // Callback run once the request has been received that allows the test to
  // start and then later finish the response at the time of its choosing. It
  // need not run either callback, but `start_response` must be run before
  // `finish_response`.
  using GotRequestCallback =
      base::OnceCallback<void(base::OnceClosure start_response,
                              base::OnceClosure finish_response)>;

  // Helper to make a `got_request` callback for when the SlowHttpResponse
  // should actually finish immediately.
  static GotRequestCallback FinishResponseImmediately();
  // Helper to make a `got_request` callback for when the SlowHttpResponse
  // should not reply at all.
  static GotRequestCallback NoResponse();

  // If `url` is `kSlowResponseUrl` this constructs an HttpResponse that will
  // not complete until the closure given to `got_request_callback` is run.
  explicit SlowHttpResponse(GotRequestCallback got_request);
  ~SlowHttpResponse() override;

  SlowHttpResponse(const SlowHttpResponse&) = delete;
  SlowHttpResponse& operator=(const SlowHttpResponse&) = delete;

  // Subclasses can override this method to add custom HTTP response headers.
  // These headers are only applied to the slow response itself, not the
  // response to |kFinishSlowResponseUrl|.
  virtual base::StringPairs ResponseHeaders();

  // Subclasses can override this method to write a custom status line; the
  // default implementation sets a 200 OK response. This status code is applied
  // only to the slow response itself, not the response to
  // |kFinishSlowResponseUrl|.
  virtual std::pair<net::HttpStatusCode, std::string> StatusLine();

  // net::test_server::HttpResponse implementations.
  void SendResponse(base::WeakPtr<HttpResponseDelegate> delegate) override;

 private:
  scoped_refptr<base::SequencedTaskRunner> main_thread_;
  GotRequestCallback got_request_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SLOW_HTTP_RESPONSE_H_
