// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_api.h"

#include "base/check_op.h"
#include "mojo/core/ipcz_driver/driver.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"
#include "third_party/ipcz/src/api.h"

#include <vector>

namespace mojo::core {

namespace {

class IpczAPIInitializer {
 public:
  explicit IpczAPIInitializer(IpczAPI& api) {
    IpczResult result = IpczGetAPI(&api);
    CHECK_EQ(result, IPCZ_RESULT_OK);
  }
};

IpczHandle g_node = IPCZ_INVALID_HANDLE;
IpczNodeOptions g_options = {.is_broker = false,
                             .use_local_shared_memory_allocation = false};

}  // namespace

const IpczAPI& GetIpczAPI() {
  static IpczAPI api = {sizeof(api)};
  static IpczAPIInitializer initializer(api);
  return api;
}

IpczHandle GetIpczNode() {
  return g_node;
}

bool InitializeIpczNodeForProcess(const IpczNodeOptions& options) {
  g_options = options;
  const IpczCreateNodeFlags flags =
      options.is_broker ? IPCZ_CREATE_NODE_AS_BROKER : IPCZ_NO_FLAGS;
  std::vector<IpczFeature> enabled_features;
  if (options.enable_memv2) {
    enabled_features.push_back(IPCZ_FEATURE_MEM_V2);
  }
  const IpczCreateNodeOptions create_options = {
      .size = sizeof(create_options),

      // TODO(crbug.com/40876289): Enable parcel data allocation capacity
      // to be expanded.
      .memory_flags = IPCZ_MEMORY_FIXED_PARCEL_CAPACITY,

      .enabled_features = enabled_features.data(),
      .num_enabled_features = enabled_features.size(),
  };
  IpczResult result = GetIpczAPI().CreateNode(&ipcz_driver::kDriver, flags,
                                              &create_options, &g_node);
  return result == IPCZ_RESULT_OK;
}

void DestroyIpczNodeForProcess() {
  CHECK(g_node);
  GetIpczAPI().Close(g_node, IPCZ_NO_FLAGS, nullptr);
  g_node = IPCZ_INVALID_HANDLE;
}

const IpczNodeOptions& GetIpczNodeOptions() {
  return g_options;
}

}  // namespace mojo::core
