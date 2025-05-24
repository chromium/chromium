// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/http_status.h"

#include "base/no_destructor.h"
#include "net/http/http_status_code.h"
#include "remoting/base/protobuf_http_client_messages.pb.h"

namespace remoting {

namespace {

constexpr HttpStatus::Code HttpStatusCodeToClientCode(
    net::HttpStatusCode http_status_code) {
  DCHECK_LT(0, http_status_code);
  switch (http_status_code) {
    case net::HttpStatusCode::HTTP_OK:
      return HttpStatus::Code::OK;
    case net::HttpStatusCode::HTTP_BAD_REQUEST:
      return HttpStatus::Code::INVALID_ARGUMENT;
    case net::HttpStatusCode::HTTP_GATEWAY_TIMEOUT:
    case net::HttpStatusCode::HTTP_REQUEST_TIMEOUT:
      return HttpStatus::Code::DEADLINE_EXCEEDED;
    case net::HttpStatusCode::HTTP_NOT_FOUND:
      return HttpStatus::Code::NOT_FOUND;
    case net::HttpStatusCode::HTTP_CONFLICT:
      return HttpStatus::Code::ALREADY_EXISTS;
    case net::HttpStatusCode::HTTP_FORBIDDEN:
      return HttpStatus::Code::PERMISSION_DENIED;
    case net::HttpStatusCode::HTTP_UNAUTHORIZED:
      return HttpStatus::Code::UNAUTHENTICATED;
    case net::HttpStatusCode::HTTP_TOO_MANY_REQUESTS:
      return HttpStatus::Code::RESOURCE_EXHAUSTED;
    case net::HttpStatusCode::HTTP_PRECONDITION_FAILED:
      return HttpStatus::Code::FAILED_PRECONDITION;
    case net::HttpStatusCode::HTTP_NOT_IMPLEMENTED:
      return HttpStatus::Code::UNIMPLEMENTED;
    case net::HttpStatusCode::HTTP_INTERNAL_SERVER_ERROR:
      return HttpStatus::Code::INTERNAL;
    case net::HttpStatusCode::HTTP_SERVICE_UNAVAILABLE:
      return HttpStatus::Code::UNAVAILABLE;
    default:
      return HttpStatus::Code::UNKNOWN;
  }
}

constexpr HttpStatus::Code NetErrorToClientCode(net::Error net_error) {
  DCHECK_GT(0, net_error);
  DCHECK_NE(net::Error::ERR_HTTP_RESPONSE_CODE_FAILURE, net_error)
      << "Use the HttpStatusCode overload";
  // See: net/base/net_error_list.h
  if (net_error <= -100 && net_error >= -199) {
    return HttpStatus::Code::NETWORK_ERROR;
  }
  switch (net_error) {
    case net::Error::OK:
      return HttpStatus::Code::OK;
    case net::Error::ERR_INVALID_ARGUMENT:
      return HttpStatus::Code::INVALID_ARGUMENT;
    case net::Error::ERR_CONNECTION_TIMED_OUT:
    case net::Error::ERR_TIMED_OUT:
      return HttpStatus::Code::DEADLINE_EXCEEDED;
    case net::Error::ERR_INVALID_AUTH_CREDENTIALS:
      return HttpStatus::Code::PERMISSION_DENIED;
    case net::Error::ERR_MISSING_AUTH_CREDENTIALS:
      return HttpStatus::Code::UNAUTHENTICATED;
    case net::Error::ERR_NOT_IMPLEMENTED:
      return HttpStatus::Code::UNIMPLEMENTED;
    case net::Error::ERR_INVALID_RESPONSE:
      return HttpStatus::Code::INTERNAL;
    default:
      return HttpStatus::Code::UNKNOWN;
  }
}

}  // namespace

const HttpStatus& HttpStatus::OK() {
  static const base::NoDestructor<HttpStatus> kOK(Code::OK, "OK");
  return *kOK;
}

HttpStatus::HttpStatus(net::HttpStatusCode http_status_code)
    : error_code_(HttpStatusCodeToClientCode(http_status_code)),
      error_message_(net::GetHttpReasonPhrase(http_status_code)) {}

HttpStatus::HttpStatus(net::Error net_error)
    : error_code_(NetErrorToClientCode(net_error)),
      error_message_(net::ErrorToString(net_error)) {}

HttpStatus::HttpStatus(const protobufhttpclient::Status& status)
    : error_code_(static_cast<HttpStatus::Code>(status.code())),
      error_message_(status.message()) {}

HttpStatus::HttpStatus(Code code, const std::string& error_message)
    : error_code_(code), error_message_(error_message) {}

HttpStatus::HttpStatus(const protobufhttpclient::Status& status,
                       const std::string& response_body)
    : error_code_(static_cast<HttpStatus::Code>(status.code())),
      error_message_(status.message()),
      response_body_(response_body) {}

HttpStatus::HttpStatus(net::HttpStatusCode http_status_code,
                       const std::string& response_body)
    : error_code_(HttpStatusCodeToClientCode(http_status_code)),
      error_message_(net::GetHttpReasonPhrase(http_status_code)),
      response_body_(response_body) {}

HttpStatus::~HttpStatus() = default;

bool HttpStatus::operator==(const HttpStatus& other) const = default;

bool HttpStatus::ok() const {
  return error_code_ == Code::OK;
}

}  // namespace remoting
