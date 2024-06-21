// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/device_bound_session_create_params.h"

namespace net {

DeviceBoundSessionCreateParams::DeviceBoundSessionCreateParams(
    std::string id,
    std::string refresh,
    Scope incoming_scope,
    std::vector<Credential> creds)
    : session_id(std::move(id)),
      refresh_url(std::move(refresh)),
      scope(std::move(incoming_scope)),
      credentials(std::move(creds)) {}

DeviceBoundSessionCreateParams::DeviceBoundSessionCreateParams(
    DeviceBoundSessionCreateParams&& other) = default;

DeviceBoundSessionCreateParams& DeviceBoundSessionCreateParams::operator=(
    DeviceBoundSessionCreateParams&& other) = default;

DeviceBoundSessionCreateParams::~DeviceBoundSessionCreateParams() = default;

DeviceBoundSessionCreateParams::Scope::Scope() = default;

DeviceBoundSessionCreateParams::Scope::Scope(Scope&& other) = default;

DeviceBoundSessionCreateParams::Scope&
DeviceBoundSessionCreateParams::Scope::operator=(
    DeviceBoundSessionCreateParams::Scope&& other) = default;

DeviceBoundSessionCreateParams::Scope::~Scope() = default;

}  // namespace net
