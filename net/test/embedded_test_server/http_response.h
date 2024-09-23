// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_HTTP_RESPONSE_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_HTTP_RESPONSE_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "net/http/http_status_code.h"

namespace net::test_server {

class HttpResponse;

// Delegate that actually sends the response bytes. Any response created should
// be owned by the delegate that passed in via HttpResponse::SendResponse().
class HttpResponseDelegate {
 public:
  HttpResponseDelegate();
  virtual ~HttpResponseDelegate();
  HttpResponseDelegate(HttpResponseDelegate&) = delete;
  HttpResponseDelegate& operator=(const HttpResponseDelegate&) = delete;

  // The delegate needs to take ownership of the response to ensure the
  // response can stay alive until the delegate has finished sending it.
  virtual void AddResponse(std::unique_ptr<HttpResponse> response) = 0;

  // Builds and sends header block. Should only be called once.
  virtual void SendResponseHeaders(HttpStatusCode status,
                                   const std::string& status_reason,
                                   const base::StringPairs& headers) = 0;
  // Sends a raw header block, in the form of an HTTP1.1 response header block
  // (separated by "\r\n". Best effort will be maintained to preserve the raw
  // headers.
  virtual void SendRawResponseHeaders(const std::string& headers) = 0;

  // Sends a content block, then calls the closure.
  virtual void SendContents(const std::string& contents,
                            base::OnceClosure callback = base::DoNothing()) = 0;

  // Called after the last content block or after the header block. The response
  // will hang until this is called.
  virtual void FinishResponse() = 0;

  // The following functions are essentially shorthand for common combinations
  // of function calls that may have a more efficient layout than just calling
  // one after the other.
  virtual void SendContentsAndFinish(const std::string& contents) = 0;
  virtual void SendHeadersContentAndFinish(HttpStatusCode status,
                                           const std::string& status_reason,
                                           const base::StringPairs& headers,
                                           const std::string& contents) = 0;
};

// Interface for HTTP response implementations. The response should be owned by
// the HttpResponseDelegate passed into SendResponse(), and should stay alive
// until FinishResponse() is called on the delegate (or the owning delegate is
// destroyed).
class HttpResponse {
 public:
  virtual ~HttpResponse();

  // Note that this is a WeakPtr. WeakPtrs can not be dereferenced or
  // invalidated outside of the thread that created them, so any use of the
  // delegate must either be from the same thread or posted to the original
  // task runner
  virtual void SendResponse(base::WeakPtr<HttpResponseDelegate> delegate) = 0;
};

// This class is used to handle basic HTTP responses with commonly used
// response headers such as "Content-Type". Sends the response immediately.
class BasicHttpResponse : public HttpResponse {
 public:
  BasicHttpResponse();

  BasicHttpResponse(const BasicHttpResponse&) = delete;
  BasicHttpResponse& operator=(const BasicHttpResponse&) = delete;

  ~BasicHttpResponse() override;

  // The response code.
  HttpStatusCode code() const { return code_; }
  void set_code(HttpStatusCode code) { code_ = code; }

  std::string reason() const {
    if (reason_) {
      return *reason_;
    } else {
      return GetHttpReasonPhrase(code_);
    }
  }
  void set_reason(std::optional<std::string> reason) {
    reason_ = std::move(reason);
  }

  // The content of the response.
  const std::string& content() const { return content_; }
  void set_content(std::string_view content) {
    content_ = std::string{content};
  }

  // The content type.
  const std::string& content_type() const { return content_type_; }
  void set_content_type(std::string_view content_type) {
    content_type_ = std::string{content_type};
  }

  // Adds a custom header.
  void AddCustomHeader(std::string_view key, std::string_view value) {
    custom_headers_.emplace_back(key, value);
  }

  // Generates and returns a http response string.
  std::string ToResponseString() const;

  base::StringPairs BuildHeaders() const;

  void SendResponse(base::WeakPtr<HttpResponseDelegate> delegate) override;

 private:
  HttpStatusCode code_ = HTTP_OK;
  std::optional<std::string> reason_;
  std::string content_;
  std::string content_type_;
  base::StringPairs custom_headers_;
  base::WeakPtrFactory<BasicHttpResponse> weak_factory_{this};
};

class DelayedHttpResponse : public BasicHttpResponse {
 public:
  explicit DelayedHttpResponse(const base::TimeDelta delay);

  DelayedHttpResponse(const DelayedHttpResponse&) = delete;
  DelayedHttpResponse& operator=(const DelayedHttpResponse&) = delete;

  ~DelayedHttpResponse() override;

  // Issues a delayed send to the to the task runner.
  void SendResponse(base::WeakPtr<HttpResponseDelegate> delegate) override;

 private:
  // The delay time for the response.
  const base::TimeDelta delay_;
};

class RawHttpResponse : public HttpResponse {
 public:
  RawHttpResponse(const std::string& headers, const std::string& contents);

  RawHttpResponse(const RawHttpResponse&) = delete;
  RawHttpResponse& operator=(const RawHttpResponse&) = delete;

  ~RawHttpResponse() override;

  void SendResponse(base::WeakPtr<HttpResponseDelegate> delegate) override;

  void AddHeader(const std::string& key_value_pair);

 private:
  std::string headers_;
  const std::string contents_;
};

// "Response" where the server doesn't actually respond until the server is
// destroyed.
class HungResponse : public HttpResponse {
 public:
  HungResponse() = default;

  HungResponse(const HungResponse&) = delete;
  HungResponse& operator=(const HungResponse&) = delete;

  ~HungResponse() override = default;

  void SendResponse(base::WeakPtr<HttpResponseDelegate> delegate) override;
};

// Return headers, then hangs.
class HungAfterHeadersHttpResponse : public HttpResponse {
 public:
  explicit HungAfterHeadersHttpResponse(base::StringPairs headers = {});
  ~HungAfterHeadersHttpResponse() override;

  void SendResponse(base::WeakPtr<HttpResponseDelegate> delegate) override;

 private:
  base::StringPairs headers_;
};

}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_HTTP_RESPONSE_H_
