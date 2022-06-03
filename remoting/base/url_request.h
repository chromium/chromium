// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_URL_REQUEST_H_
#define REMOTING_BASE_URL_REQUEST_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace remoting {

// Abstract interface for URL requests.
class UrlRequest {
 public:
  enum class Type {
    GET,
    POST,
  };

  struct Result {
    Result() = default;
    Result(int status, std::string response_body)
        : success(true), status(status), response_body(response_body) {}

    static Result Failed() { return Result(); }

    // Set to true when the URL has been fetched successfully.
    bool success = false;

    // HTTP status code received from the server. Valid only when |success| is
    // set to true.
    int status = 0;

    // Body of the response received from the server. Valid only when |success|
    // is set to true.
    std::string response_body;
  };

  typedef base::OnceCallback<void(const Result& result)> OnResultCallback;

  virtual ~UrlRequest() {}

  // Adds an HTTP header to the request. Has no effect if called after Start().
  virtual void AddHeader(const std::string& value) = 0;

  // Sets data to be sent for POST requests.
  virtual void SetPostData(const std::string& content_type,
                           const std::string& post_data) = 0;

  // Sends a request to the server. |on_response_callback| will be called to
  // return result of the request.
  virtual void Start(OnResultCallback on_result_callback) = 0;
};

// Factory for UrlRequest instances.
class UrlRequestFactory {
 public:
  virtual ~UrlRequestFactory() {}
  virtual std::unique_ptr<UrlRequest> CreateUrlRequest(
      UrlRequest::Type type,
      const std::string& url,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) = 0;
};

}  // namespace remoting

#endif  // REMOTING_BASE_URL_REQUEST_H_
