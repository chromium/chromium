// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/heartbeat_service_client.h"

namespace remoting {

HeartbeatServiceClient::HeartbeatServiceClient(const std::string& directory_id)
    : directory_id_(directory_id) {}

HeartbeatServiceClient::~HeartbeatServiceClient() = default;

}  // namespace remoting
