// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_LOGGING_SERVICE_CLIENT_H_
#define REMOTING_BASE_LOGGING_SERVICE_CLIENT_H_

#include "base/functional/callback_forward.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/proto/logging_service.h"

namespace remoting {

// Interface for communicating with the corp logging service using the Corp API.
// This is not used for external users. For internal details, see
// go/crd-corp-logging.
class LoggingServiceClient {
 public:
  using StatusCallback = base::OnceCallback<void(const ProtobufHttpStatus&)>;

  LoggingServiceClient() = default;
  virtual ~LoggingServiceClient() = default;

  LoggingServiceClient(const LoggingServiceClient&) = delete;
  LoggingServiceClient& operator=(const LoggingServiceClient&) = delete;

  virtual void ReportSessionDisconnected(
      const internal::ReportSessionDisconnectedRequestStruct& request,
      StatusCallback done) = 0;
};

}  // namespace remoting

#endif  // REMOTING_BASE_LOGGING_SERVICE_CLIENT_H_
