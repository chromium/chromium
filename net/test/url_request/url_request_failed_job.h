// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_URL_REQUEST_URL_REQUEST_FAILED_JOB_H_
#define NET_TEST_URL_REQUEST_URL_REQUEST_FAILED_JOB_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_request_job.h"
#include "url/gurl.h"

namespace net {

// This class simulates a URLRequestJob failing with a given error code at
// a particular phase while trying to connect.
class URLRequestFailedJob : public URLRequestJob {
 public:
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net.test
  enum FailurePhase {
    START = 0,
    READ_SYNC = 1,
    READ_ASYNC = 2,
    MAX_FAILURE_PHASE = 3,
  };

  URLRequestFailedJob(URLRequest* request,
                      NetworkDelegate* network_delegate,
                      FailurePhase phase,
                      int net_error);

  // Same as above, except that the job fails at FailurePhase.START.
  URLRequestFailedJob(URLRequest* request,
                      NetworkDelegate* network_delegate,
                      int net_error);

  // URLRequestJob implementation:
  void Start() override;
  int ReadRawData(IOBuffer* buf, int buf_size) override;
  void GetResponseInfo(HttpResponseInfo* info) override;
  void PopulateNetErrorDetails(NetErrorDetails* details) const override;
  int64_t GetTotalReceivedBytes() const override;

  // Adds the testing URLs to the URLRequestFilter.
  static void AddUrlHandler();
  static void AddUrlHandlerForHostname(const std::string& hostname);

  // Given a net error code, constructs a mock URL that will return that error
  // asynchronously when started. |net_error| must be a valid net error code
  // other than net::OK. Passing net::ERR_IO_PENDING for |net_error| causes the
  // resulting request to hang.
  static GURL GetMockHttpUrl(int net_error);
  static GURL GetMockHttpsUrl(int net_error);

  // Constructs a mock URL that reports |net_error| at given |phase| of the
  // request. |net_error| must be a valid net error code other than net::OK.
  // Passing net::ERR_IO_PENDING for |net_error| causes the resulting request to
  // hang.
  static GURL GetMockHttpUrlWithFailurePhase(FailurePhase phase, int net_error);

  // Given a net error code and a host name, constructs a mock URL that will
  // return that error asynchronously when started. |net_error| must be a valid
  // net error code other than net::OK. Passing net::ERR_IO_PENDING for
  // |net_error| causes the resulting request to hang.
  static GURL GetMockHttpUrlForHostname(int net_error,
                                        const std::string& hostname);
  static GURL GetMockHttpsUrlForHostname(int net_error,
                                         const std::string& hostname);

 protected:
  ~URLRequestFailedJob() override;
  void StartAsync();

 private:
  HttpResponseInfo response_info_;
  const FailurePhase phase_;
  const int net_error_;
  int64_t total_received_bytes_;

  base::WeakPtrFactory<URLRequestFailedJob> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(URLRequestFailedJob);
};

}  // namespace net

#endif  // NET_TEST_URL_REQUEST_URL_REQUEST_FAILED_JOB_H_
