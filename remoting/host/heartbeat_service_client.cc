// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/heartbeat_service_client.h"

#include <optional>

#include "base/functional/callback.h"
#include "remoting/base/protobuf_http_status.h"

namespace remoting {

HeartbeatServiceClient::HeartbeatServiceClient() = default;
HeartbeatServiceClient::~HeartbeatServiceClient() = default;

void HeartbeatServiceClient::OnError(HeartbeatResponseCallback callback,
                                     const ProtobufHttpStatus& status) {
  CHECK(!status.ok());
  std::move(callback).Run(status, /*wait_interval=*/std::nullopt,
                          /*primary_user_email=*/"",
                          /*require_session_authorization=*/std::nullopt,
                          /*use_lite_heartbeat=*/std::nullopt);
}

}  // namespace remoting
