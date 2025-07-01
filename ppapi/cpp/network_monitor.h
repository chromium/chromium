// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_NETWORK_MONITOR_H_
#define PPAPI_CPP_NETWORK_MONITOR_H_

#include <stdint.h>

#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class NetworkList;

template <typename T> class CompletionCallbackWithOutput;

class NetworkMonitor : public Resource {
 public:
  explicit NetworkMonitor(const InstanceHandle& instance);

  int32_t UpdateNetworkList(
      const CompletionCallbackWithOutput<NetworkList>& callback);

  // Returns true if the required interface is available.
  static bool IsAvailable();
};

}  // namespace pp

#endif  // PPAPI_CPP_NETWORK_MONITOR_H_
