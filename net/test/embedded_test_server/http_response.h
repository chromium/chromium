// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_HTTP_RESPONSE_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_HTTP_RESPONSE_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "net/http/http_status_code.h"

namespace net {
namespace test_server {

// Callback called when the response is done being sent.
using SendCompleteCallback = base::OnceClosure;

// Callback called when the response is ready to be sent that takes the
// |response| that is being sent along with the callback |write_done| that is
// called when the response has been fully written.
using SendBytesCallback =
    base::RepeatingCallback<void(const std::string& response,
                                 SendCompleteCallback write_done)>;

// Interface for HTTP response implementations.
class HttpResponse{
 public:
  virtual ~HttpResponse();

  // |send| will send the specified data to the network socket, and invoke
  // |write_done| when complete. When the entire response has been sent,
  // |done| must be called. Invoking |done| will delete the HttpResponse object.
  virtual void SendResponse(const SendBytesCallback& send,
                            SendCompleteCallback done) = 0;
};

// This class is used to handle basic HTTP responses with commonly used
// response headers such as "Content-Type". Sends the response immediately.
class BasicHttpResponse : public HttpResponse {
 public:
  BasicHttpResponse();
  ~BasicHttpResponse() override;

  // The response code.
  HttpStatusCode code() const { return code_; }
  void set_code(HttpStatusCode code) { code_ = code; }

  // The content of the response.
  const std::string& content() const { return content_; }
  void set_content(const std::string& content) { content_ = content; }

  // The content type.
  const std::string& content_type() const { return content_type_; }
  void set_content_type(const std::string& content_type) {
    content_type_ = content_type;
  }

  // Adds a custom header.
  void AddCustomHeader(const std::string& key, const std::string& value) {
    custom_headers_.push_back(std::make_pair(key, value));
  }

  // Generates and returns a http response string.
  std::string ToResponseString() const;

  void SendResponse(const SendBytesCallback& send,
                    SendCompleteCallback done) override;

 private:
  HttpStatusCode code_;
  std::string content_;
  std::string content_type_;
  base::StringPairs custom_headers_;

  DISALLOW_COPY_AND_ASSIGN(BasicHttpResponse);
};

class DelayedHttpResponse : public BasicHttpResponse {
 public:
  DelayedHttpResponse(const base::TimeDelta delay);
  ~DelayedHttpResponse() override;

  // Issues a delayed send to the to the task runner.
  void SendResponse(const SendBytesCallback& send,
                    SendCompleteCallback done) override;

 private:
  // The delay time for the response.
  const base::TimeDelta delay_;

  DISALLOW_COPY_AND_ASSIGN(DelayedHttpResponse);
};

class RawHttpResponse : public HttpResponse {
 public:
  RawHttpResponse(const std::string& headers, const std::string& contents);
  ~RawHttpResponse() override;

  void SendResponse(const SendBytesCallback& send,
                    SendCompleteCallback done) override;

  void AddHeader(const std::string& key_value_pair);

 private:
  std::string headers_;
  const std::string contents_;

  DISALLOW_COPY_AND_ASSIGN(RawHttpResponse);
};

// "Response" where the server doesn't actually respond until the server is
// destroyed.
class HungResponse : public HttpResponse {
 public:
  HungResponse() {}
  ~HungResponse() override {}

  void SendResponse(const SendBytesCallback& send,
                    SendCompleteCallback done) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HungResponse);
};

}  // namespace test_server
}  // namespace net

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_HTTP_RESPONSE_H_
