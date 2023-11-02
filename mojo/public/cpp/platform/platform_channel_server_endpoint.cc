// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"

namespace mojo {

PlatformChannelServerEndpoint::PlatformChannelServerEndpoint() = default;

PlatformChannelServerEndpoint::PlatformChannelServerEndpoint(
    PlatformChannelServerEndpoint&& other) = default;

PlatformChannelServerEndpoint::PlatformChannelServerEndpoint(
    PlatformHandle handle)
    : handle_(std::move(handle)) {}

PlatformChannelServerEndpoint::~PlatformChannelServerEndpoint() = default;

PlatformChannelServerEndpoint& PlatformChannelServerEndpoint::operator=(
    PlatformChannelServerEndpoint&& other) = default;

void PlatformChannelServerEndpoint::reset() {
  handle_.reset();
}

PlatformChannelServerEndpoint PlatformChannelServerEndpoint::Clone() const {
  return PlatformChannelServerEndpoint(handle_.Clone());
}

}  // namespace mojo
