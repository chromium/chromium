// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PROTOBUF_HTTP_STATUS_H_
#define REMOTING_BASE_PROTOBUF_HTTP_STATUS_H_

#include <string>

#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"

namespace remoting {

namespace protobufhttpclient {
class Status;
}  // namespace protobufhttpclient

class ProtobufHttpStatus {
 public:
  // This is the same as the gRPC status code.
  enum class Code : int {
    OK = 0,
    CANCELLED = 1,
    UNKNOWN = 2,
    INVALID_ARGUMENT = 3,
    DEADLINE_EXCEEDED = 4,
    NOT_FOUND = 5,
    ALREADY_EXISTS = 6,
    PERMISSION_DENIED = 7,
    UNAUTHENTICATED = 16,
    RESOURCE_EXHAUSTED = 8,
    FAILED_PRECONDITION = 9,
    ABORTED = 10,
    OUT_OF_RANGE = 11,
    UNIMPLEMENTED = 12,
    INTERNAL = 13,
    UNAVAILABLE = 14,
    DATA_LOSS = 15,
  };

  // An OK pre-defined instance.
  static const ProtobufHttpStatus& OK();

  explicit ProtobufHttpStatus(net::HttpStatusCode http_status_code);
  explicit ProtobufHttpStatus(net::Error net_error);
  explicit ProtobufHttpStatus(const protobufhttpclient::Status& status);
  ProtobufHttpStatus(Code code, const std::string& error_message);
  ~ProtobufHttpStatus();

  // Indicates whether the http request was successful based on the status code.
  bool ok() const;

  // The instance's error code.
  Code error_code() const { return error_code_; }

  // The message that describes the error.
  const std::string& error_message() const { return error_message_; }

 private:
  Code error_code_;
  std::string error_message_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_PROTOBUF_HTTP_STATUS_H_
