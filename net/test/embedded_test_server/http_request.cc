// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/test/embedded_test_server/http_request.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_chunked_decoder.h"
#include "url/gurl.h"

namespace net::test_server {

namespace {

size_t kRequestSizeLimit = 64 * 1024 * 1024;  // 64 mb.

// Helper function used to trim tokens in http request headers.
std::string Trim(const std::string& value) {
  std::string result;
  base::TrimString(value, " \t", &result);
  return result;
}

}  // namespace

HttpRequest::HttpRequest() = default;

HttpRequest::HttpRequest(const HttpRequest& other) = default;

HttpRequest::~HttpRequest() = default;

GURL HttpRequest::GetURL() const {
  if (base_url.is_valid())
    return base_url.Resolve(relative_url);
  return GURL("http://localhost" + relative_url);
}

HttpRequestParser::HttpRequestParser()
    : http_request_(std::make_unique<HttpRequest>()) {}

HttpRequestParser::~HttpRequestParser() = default;

void HttpRequestParser::ProcessChunk(std::string_view data) {
  buffer_.append(data);
  DCHECK_LE(buffer_.size() + data.size(), kRequestSizeLimit) <<
      "The HTTP request is too large.";
}

std::string HttpRequestParser::ShiftLine() {
  size_t eoln_position = buffer_.find("\r\n", buffer_position_);
  DCHECK_NE(std::string::npos, eoln_position);
  const int line_length = eoln_position - buffer_position_;
  std::string result = buffer_.substr(buffer_position_, line_length);
  buffer_position_ += line_length + 2;
  return result;
}

HttpRequestParser::ParseResult HttpRequestParser::ParseRequest() {
  DCHECK_NE(STATE_ACCEPTED, state_);
  // Parse the request from beginning. However, entire request may not be
  // available in the buffer.
  if (state_ == STATE_HEADERS) {
    if (ParseHeaders() == ACCEPTED)
      return ACCEPTED;
  }
  // This should not be 'else if' of the previous block, as |state_| can be
  // changed in ParseHeaders().
  if (state_ == STATE_CONTENT) {
    if (ParseContent() == ACCEPTED)
      return ACCEPTED;
  }
  return WAITING;
}

HttpRequestParser::ParseResult HttpRequestParser::ParseHeaders() {
  // Check if the all request headers are available.
  if (buffer_.find("\r\n\r\n", buffer_position_) == std::string::npos)
    return WAITING;

  // Parse request's the first header line.
  // Request main main header, eg. GET /foobar.html HTTP/1.1
  std::string request_headers;
  {
    const std::string header_line = ShiftLine();
    http_request_->all_headers += header_line + "\r\n";
    std::vector<std::string> header_line_tokens = base::SplitString(
        header_line, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    DCHECK_EQ(3u, header_line_tokens.size());
    // Method.
    http_request_->method_string = header_line_tokens[0];
    http_request_->method = GetMethodType(http_request_->method_string);
    // Target resource. See
    // https://www.rfc-editor.org/rfc/rfc9112#name-request-line
    // https://www.rfc-editor.org/rfc/rfc9110#name-determining-the-target-reso
    if (http_request_->method == METHOD_CONNECT) {
      // CONNECT uses a special authority-form. Just report the value as
      // `relative_url`.
      // https://www.rfc-editor.org/rfc/rfc9112#section-3.2.3
      CHECK(!HostPortPair::FromString(header_line_tokens[1]).IsEmpty());
      http_request_->relative_url = header_line_tokens[1];
    } else if (http_request_->method == METHOD_OPTIONS &&
               header_line_tokens[1] == "*") {
      // OPTIONS allows a special asterisk-form for the request target.
      // https://www.rfc-editor.org/rfc/rfc9112#section-3.2.4
      http_request_->relative_url = "*";
    } else {
      // The request target should be origin-form, unless connecting through a
      // proxy, in which case it is absolute-form.
      // https://www.rfc-editor.org/rfc/rfc9112#name-origin-form
      // https://www.rfc-editor.org/rfc/rfc9112#name-absolute-form
      if (!header_line_tokens[1].empty() &&
          header_line_tokens[1].front() == '/') {
        http_request_->relative_url = header_line_tokens[1];
      } else {
        GURL url(header_line_tokens[1]);
        CHECK(url.is_valid());
        // TODO(crbug.com/40242862): This should retain the entire URL.
        http_request_->relative_url = url.PathForRequest();
      }
    }

    // Protocol.
    const std::string protocol = base::ToLowerASCII(header_line_tokens[2]);
    CHECK(protocol == "http/1.0" || protocol == "http/1.1") <<
        "Protocol not supported: " << protocol;
  }

  // Parse further headers.
  {
    std::string header_name;
    while (true) {
      std::string header_line = ShiftLine();
      if (header_line.empty())
        break;

      http_request_->all_headers += header_line + "\r\n";
      if (header_line[0] == ' ' || header_line[0] == '\t') {
        // Continuation of the previous multi-line header.
        std::string header_value =
            Trim(header_line.substr(1, header_line.size() - 1));
        http_request_->headers[header_name] += " " + header_value;
      } else {
        // New header.
        size_t delimiter_pos = header_line.find(":");
        DCHECK_NE(std::string::npos, delimiter_pos) << "Syntax error.";
        header_name = Trim(header_line.substr(0, delimiter_pos));
        std::string header_value = Trim(header_line.substr(
            delimiter_pos + 1,
            header_line.size() - delimiter_pos - 1));
        http_request_->headers[header_name] = header_value;
      }
    }
  }

  // Headers done. Is any content data attached to the request?
  declared_content_length_ = 0;
  if (http_request_->headers.count("Content-Length") > 0) {
    http_request_->has_content = true;
    const bool success = base::StringToSizeT(
        http_request_->headers["Content-Length"],
        &declared_content_length_);
    if (!success) {
      declared_content_length_ = 0;
      LOG(WARNING) << "Malformed Content-Length header's value.";
    }
  } else if (http_request_->headers.count("Transfer-Encoding") > 0) {
    if (base::EqualsCaseInsensitiveASCII(
            http_request_->headers["Transfer-Encoding"], "chunked")) {
      http_request_->has_content = true;
      chunked_decoder_ = std::make_unique<HttpChunkedDecoder>();
      state_ = STATE_CONTENT;
      return WAITING;
    }
  }
  if (declared_content_length_ == 0) {
    // No content data, so parsing is finished.
    state_ = STATE_ACCEPTED;
    return ACCEPTED;
  }

  // The request has not yet been parsed yet, content data is still to be
  // processed.
  state_ = STATE_CONTENT;
  return WAITING;
}

HttpRequestParser::ParseResult HttpRequestParser::ParseContent() {
  const size_t available_bytes = buffer_.size() - buffer_position_;
  if (chunked_decoder_.get()) {
    int bytes_written = chunked_decoder_->FilterBuf(
        const_cast<char*>(buffer_.data()) + buffer_position_, available_bytes);
    http_request_->content.append(buffer_.data() + buffer_position_,
                                  bytes_written);

    if (chunked_decoder_->reached_eof()) {
      buffer_ =
          buffer_.substr(buffer_.size() - chunked_decoder_->bytes_after_eof());
      buffer_position_ = 0;
      state_ = STATE_ACCEPTED;
      return ACCEPTED;
    }
    buffer_ = "";
    buffer_position_ = 0;
    state_ = STATE_CONTENT;
    return WAITING;
  }

  const size_t fetch_bytes = std::min(
      available_bytes,
      declared_content_length_ - http_request_->content.size());
  http_request_->content.append(buffer_.data() + buffer_position_,
                                fetch_bytes);
  buffer_position_ += fetch_bytes;

  if (declared_content_length_ == http_request_->content.size()) {
    state_ = STATE_ACCEPTED;
    return ACCEPTED;
  }

  state_ = STATE_CONTENT;
  return WAITING;
}

std::unique_ptr<HttpRequest> HttpRequestParser::GetRequest() {
  DCHECK_EQ(STATE_ACCEPTED, state_);
  std::unique_ptr<HttpRequest> result = std::move(http_request_);

  // Prepare for parsing a new request.
  state_ = STATE_HEADERS;
  http_request_ = std::make_unique<HttpRequest>();
  buffer_.clear();
  buffer_position_ = 0;
  declared_content_length_ = 0;

  return result;
}

// static
HttpMethod HttpRequestParser::GetMethodType(std::string_view token) {
  if (token == "GET") {
    return METHOD_GET;
  } else if (token == "HEAD") {
    return METHOD_HEAD;
  } else if (token == "POST") {
    return METHOD_POST;
  } else if (token == "PUT") {
    return METHOD_PUT;
  } else if (token == "DELETE") {
    return METHOD_DELETE;
  } else if (token == "PATCH") {
    return METHOD_PATCH;
  } else if (token == "CONNECT") {
    return METHOD_CONNECT;
  } else if (token == "OPTIONS") {
    return METHOD_OPTIONS;
  }
  LOG(WARNING) << "Method not implemented: " << token;
  return METHOD_UNKNOWN;
}

}  // namespace net::test_server
