// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_PLATFORM_PLATFORM_CHANNEL_SERVER_ENDPOINT_H_
#define MOJO_PUBLIC_CPP_PLATFORM_PLATFORM_CHANNEL_SERVER_ENDPOINT_H_

#include "base/component_export.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace mojo {

// A PlatformHandle with a little extra type information to convey that it's
// a channel server endpoint, i.e. a handle that should be used with
// PlatformChannelServer to wait for a new connection and ultimately provide
// a connected PlatformChannelEndpoint suitable for use with the Mojo
// invitations API.
class COMPONENT_EXPORT(MOJO_CPP_PLATFORM) PlatformChannelServerEndpoint {
 public:
  PlatformChannelServerEndpoint();
  PlatformChannelServerEndpoint(PlatformChannelServerEndpoint&& other);
  explicit PlatformChannelServerEndpoint(PlatformHandle handle);

  PlatformChannelServerEndpoint(const PlatformChannelServerEndpoint&) = delete;
  PlatformChannelServerEndpoint& operator=(
      const PlatformChannelServerEndpoint&) = delete;

  ~PlatformChannelServerEndpoint();

  PlatformChannelServerEndpoint& operator=(
      PlatformChannelServerEndpoint&& other);

  bool is_valid() const { return handle_.is_valid(); }
  void reset();
  PlatformChannelServerEndpoint Clone() const;

  const PlatformHandle& platform_handle() const { return handle_; }

  [[nodiscard]] PlatformHandle TakePlatformHandle() {
    return std::move(handle_);
  }

 private:
  PlatformHandle handle_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_PLATFORM_PLATFORM_CHANNEL_SERVER_ENDPOINT_H_
