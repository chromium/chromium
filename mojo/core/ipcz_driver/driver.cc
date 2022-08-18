// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/driver.h"

#include <cstddef>
#include <cstdint>
#include <tuple>

#include "base/containers/span.h"
#include "base/containers/stack_container.h"
#include "base/rand_util.h"
#include "mojo/core/ipcz_driver/object.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core::ipcz_driver {

namespace {

IpczResult IPCZ_API Close(IpczDriverHandle handle,
                          uint32_t flags,
                          const void* options) {
  scoped_refptr<ObjectBase> object = ObjectBase::TakeFromHandle(handle);
  if (!object) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }
  object->Close();
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API Serialize(IpczDriverHandle handle,
                              IpczDriverHandle transport_handle,
                              uint32_t flags,
                              const void* options,
                              void* data,
                              size_t* num_bytes,
                              IpczDriverHandle* handles,
                              size_t* num_handles) {
  return IPCZ_RESULT_UNIMPLEMENTED;
}

IpczResult IPCZ_API Deserialize(const void* data,
                                size_t num_bytes,
                                const IpczDriverHandle* handles,
                                size_t num_handles,
                                IpczDriverHandle transport_handle,
                                uint32_t flags,
                                const void* options,
                                IpczDriverHandle* driver_handle) {
  return IPCZ_RESULT_UNIMPLEMENTED;
}

IpczResult IPCZ_API CreateTransports(IpczDriverHandle transport0,
                                     IpczDriverHandle transport1,
                                     uint32_t flags,
                                     const void* options,
                                     IpczDriverHandle* new_transport0,
                                     IpczDriverHandle* new_transport1) {
  return IPCZ_RESULT_UNIMPLEMENTED;
}

IpczResult IPCZ_API
ActivateTransport(IpczDriverHandle driver_transport,
                  IpczHandle ipcz_transport,
                  IpczTransportActivityHandler activity_handler,
                  uint32_t flags,
                  const void* options) {
  return IPCZ_RESULT_UNIMPLEMENTED;
}

IpczResult IPCZ_API DeactivateTransport(IpczDriverHandle driver_transport,
                                        uint32_t flags,
                                        const void* options) {
  return IPCZ_RESULT_UNIMPLEMENTED;
}

IpczResult IPCZ_API Transmit(IpczDriverHandle driver_transport,
                             const void* data,
                             size_t num_bytes,
                             const IpczDriverHandle* handles,
                             size_t num_handles,
                             uint32_t flags,
                             const void* options) {
  return IPCZ_RESULT_UNIMPLEMENTED;
}

IpczResult IPCZ_API AllocateSharedMemory(size_t num_bytes,
                                         uint32_t flags,
                                         const void* options,
                                         IpczDriverHandle* driver_memory) {
  return IPCZ_RESULT_UNIMPLEMENTED;
}

IpczResult IPCZ_API GetSharedMemoryInfo(IpczDriverHandle driver_memory,
                                        uint32_t flags,
                                        const void* options,
                                        IpczSharedMemoryInfo* info) {
  return IPCZ_RESULT_UNIMPLEMENTED;
}

IpczResult IPCZ_API DuplicateSharedMemory(IpczDriverHandle driver_memory,
                                          uint32_t flags,
                                          const void* options,
                                          IpczDriverHandle* new_driver_memory) {
  return IPCZ_RESULT_UNIMPLEMENTED;
}

IpczResult IPCZ_API MapSharedMemory(IpczDriverHandle driver_memory,
                                    uint32_t flags,
                                    const void* options,
                                    void** address,
                                    IpczDriverHandle* driver_mapping) {
  return IPCZ_RESULT_UNIMPLEMENTED;
}

IpczResult IPCZ_API GenerateRandomBytes(size_t num_bytes,
                                        uint32_t flags,
                                        const void* options,
                                        void* buffer) {
  if (!buffer || !num_bytes) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }
  base::RandBytes(buffer, num_bytes);
  return IPCZ_RESULT_OK;
}

}  // namespace

const IpczDriver kDriver = {
    sizeof(kDriver),
    Close,
    Serialize,
    Deserialize,
    CreateTransports,
    ActivateTransport,
    DeactivateTransport,
    Transmit,
    AllocateSharedMemory,
    GetSharedMemoryInfo,
    DuplicateSharedMemory,
    MapSharedMemory,
    GenerateRandomBytes,
};

}  // namespace mojo::core::ipcz_driver
