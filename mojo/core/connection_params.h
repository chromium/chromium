// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_CONNECTION_PARAMS_H_
#define MOJO_CORE_CONNECTION_PARAMS_H_

#include "build/build_config.h"
#include "mojo/core/system_impl_export.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"

namespace mojo {
namespace core {

// A set of parameters used when establishing a connection to another process.
class MOJO_SYSTEM_IMPL_EXPORT ConnectionParams {
 public:
  ConnectionParams();
  explicit ConnectionParams(PlatformChannelEndpoint endpoint);
  ConnectionParams(ConnectionParams&&);

  ConnectionParams(const ConnectionParams&) = delete;
  ConnectionParams& operator=(const ConnectionParams&) = delete;

  ~ConnectionParams();

  ConnectionParams& operator=(ConnectionParams&&);

  const PlatformChannelEndpoint& endpoint() const { return endpoint_; }

  PlatformChannelEndpoint TakeEndpoint() { return std::move(endpoint_); }

  void set_is_async(bool is_async) { is_async_ = is_async; }
  bool is_async() const { return is_async_; }

  void set_is_untrusted_process(bool is_untrusted_process) {
    is_untrusted_process_ = is_untrusted_process;
  }
  bool is_untrusted_process() const { return is_untrusted_process_; }

  void set_leak_endpoint(bool leak_endpoint) { leak_endpoint_ = leak_endpoint; }
  bool leak_endpoint() const { return leak_endpoint_; }

 private:
  bool is_async_ = false;
  bool is_untrusted_process_ = false;
  bool leak_endpoint_ = false;
  PlatformChannelEndpoint endpoint_;
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_CONNECTION_PARAMS_H_
