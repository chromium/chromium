// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_CLIENT_SERVICE_MAP_H_
#define GPU_COMMAND_BUFFER_SERVICE_CLIENT_SERVICE_MAP_H_

#include <limits>
#include <unordered_map>
#include <vector>

#include "base/logging.h"

namespace gpu {

namespace gles2 {

template <typename ClientType, typename ServiceType>
class ClientServiceMap {
 public:
  ClientServiceMap()
      : ClientServiceMap(std::numeric_limits<ServiceType>::max()) {}
  explicit ClientServiceMap(ServiceType invalid_service_id)
      : invalid_service_id_(invalid_service_id),
        client_to_service_array_(kInitialFlatArraySize, invalid_service_id),
        client_to_service_map_() {}

  void SetIDMapping(ClientType client_id, ServiceType service_id) {
    DCHECK(service_id != invalid_service_id());
    if (client_id < kMaxFlatArraySize) {
      if (client_id >= client_to_service_array_.size()) {
        // Resize the array to the next power of 2 above client_id
        size_t new_size = client_to_service_array_.size();
        while (client_id >= new_size) {
          new_size *= 2;
        }
        DCHECK(new_size <= kMaxFlatArraySize);
        client_to_service_array_.resize(new_size, invalid_service_id());
      }
      DCHECK(client_to_service_array_[client_id] == invalid_service_id());
      client_to_service_array_[client_id] = service_id;
    } else {
      DCHECK(client_to_service_map_.find(client_id) ==
             client_to_service_map_.end());
      client_to_service_map_[client_id] = service_id;
    }
  }

  void RemoveClientID(ClientType client_id) {
    if (client_id < kMaxFlatArraySize) {
      if (client_id < client_to_service_array_.size()) {
        client_to_service_array_[client_id] = invalid_service_id();
      }
    } else {
      client_to_service_map_.erase(client_id);
    }
  }

  void Clear() {
    client_to_service_array_.clear();
    client_to_service_array_.resize(kInitialFlatArraySize,
                                    invalid_service_id());
    client_to_service_map_.clear();
  }

  ALWAYS_INLINE bool GetServiceID(ClientType client_id,
                                  ServiceType* service_id) const {
    DCHECK(service_id);
    if (client_id >= kMaxFlatArraySize) {
      return GetServiceIDFromMap(client_id, service_id);
    }

    if (client_id < client_to_service_array_.size() &&
        client_to_service_array_[client_id] != invalid_service_id()) {
      *service_id = client_to_service_array_[client_id];
      return true;
    }
    if (client_id == 0) {
      *service_id = ServiceType{};
      return true;
    }
    return false;
  }

  ALWAYS_INLINE bool HasClientID(ClientType client_id) const {
    if (client_id >= kMaxFlatArraySize) {
      return GetServiceIDFromMap(client_id, nullptr);
    }

    return client_id == 0 ||
           (client_id < client_to_service_array_.size() &&
            client_to_service_array_[client_id] != invalid_service_id());
  }

  ServiceType GetServiceIDOrInvalid(ClientType client_id) {
    ServiceType service_id;
    if (GetServiceID(client_id, &service_id)) {
      return service_id;
    }
    return invalid_service_id();
  }

  bool GetClientID(ServiceType service_id, ClientType* client_id) const {
    DCHECK(service_id != invalid_service_id());
    for (size_t client_id_idx = 0;
         client_id_idx < client_to_service_array_.size(); client_id_idx++) {
      if (client_to_service_array_[client_id_idx] == service_id) {
        if (client_id) {
          *client_id = client_id_idx;
        }
        return true;
      }
    }
    for (const auto& mapping : client_to_service_map_) {
      if (mapping.second == service_id) {
        if (client_id) {
          *client_id = mapping.first;
        }
        return true;
      }
    }
    if (service_id == 0) {
      *client_id = 0;
    }
    return false;
  }

  const ServiceType& invalid_service_id() const { return invalid_service_id_; }

  template <typename FunctionType>
  void ForEach(FunctionType func) const {
    for (size_t client_id_idx = 0;
         client_id_idx < client_to_service_array_.size(); client_id_idx++) {
      if (client_to_service_array_[client_id_idx] != invalid_service_id()) {
        func(client_id_idx, client_to_service_array_[client_id_idx]);
      }
    }
    for (const auto& mapping : client_to_service_map_) {
      func(mapping.first, mapping.second);
    }
  }

  // Start with 32 maximum elements in the map, which can grow.
  static constexpr size_t kInitialFlatArraySize = 0x20;

  // Experimental testing suggests that 16k is a reasonable upper limit.
  static constexpr size_t kMaxFlatArraySize = 0x4000;

 private:
  bool GetServiceIDFromMap(ClientType client_id,
                           ServiceType* service_id) const {
    DCHECK(client_id >= kMaxFlatArraySize);
    auto iter = client_to_service_map_.find(client_id);
    if (iter != client_to_service_map_.end()) {
      if (service_id) {
        *service_id = iter->second;
      }
      return true;
    }
    return false;
  }

  ServiceType invalid_service_id_;
  std::vector<ServiceType> client_to_service_array_;
  std::unordered_map<ClientType, ServiceType> client_to_service_map_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_CLIENT_SERVICE_MAP_H_
