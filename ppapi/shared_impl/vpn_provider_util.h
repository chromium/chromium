// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_VPN_PROVIDER_UTIL_H_
#define PPAPI_SHARED_IMPL_VPN_PROVIDER_UTIL_H_

#include <memory>

#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class PPAPI_SHARED_EXPORT VpnProviderSharedBuffer {
 public:
  VpnProviderSharedBuffer(uint32_t capacity,
                          uint32_t packet_size,
                          base::UnsafeSharedMemoryRegion shm,
                          base::WritableSharedMemoryMapping mapping);

  VpnProviderSharedBuffer(const VpnProviderSharedBuffer&) = delete;
  VpnProviderSharedBuffer& operator=(const VpnProviderSharedBuffer&) = delete;

  ~VpnProviderSharedBuffer();

  bool GetAvailable(uint32_t* id);
  void SetAvailable(uint32_t id, bool value);
  void* GetBuffer(uint32_t id);
  uint32_t max_packet_size() { return max_packet_size_; }
  base::UnsafeSharedMemoryRegion DuplicateRegion() const;

 private:
  uint32_t capacity_;
  uint32_t max_packet_size_;
  base::UnsafeSharedMemoryRegion shm_;
  base::WritableSharedMemoryMapping shm_mapping_;
  std::vector<bool> available_;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_VPN_PROVIDER_UTIL_H_
