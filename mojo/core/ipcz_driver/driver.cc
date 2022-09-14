// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/driver.h"

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <utility>

#include "base/containers/span.h"
#include "base/containers/stack_container.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/rand_util.h"
#include "mojo/core/ipcz_driver/object.h"
#include "mojo/core/ipcz_driver/shared_buffer.h"
#include "mojo/core/ipcz_driver/shared_buffer_mapping.h"
#include "mojo/core/ipcz_driver/transmissible_platform_handle.h"
#include "mojo/core/ipcz_driver/transport.h"
#include "mojo/public/cpp/platform/platform_handle.h"
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
  ObjectBase* object = ObjectBase::FromHandle(handle);
  Transport* transport = Transport::FromHandle(transport_handle);
  if (!object || !object->IsSerializable()) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  if (!transport) {
    return IPCZ_RESULT_ABORTED;
  }

  const IpczResult result = transport->SerializeObject(*object, data, num_bytes,
                                                       handles, num_handles);
  if (result != IPCZ_RESULT_OK) {
    return result;
  }

  // On success we consume the object reference owned by the input handle.
  std::ignore = ObjectBase::TakeFromHandle(handle);
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API Deserialize(const void* data,
                                size_t num_bytes,
                                const IpczDriverHandle* handles,
                                size_t num_handles,
                                IpczDriverHandle transport_handle,
                                uint32_t flags,
                                const void* options,
                                IpczDriverHandle* driver_handle) {
  Transport* transport = Transport::FromHandle(transport_handle);
  if (!transport || !driver_handle) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  scoped_refptr<ObjectBase> object;
  const IpczResult result = transport->DeserializeObject(
      base::make_span(static_cast<const uint8_t*>(data), num_bytes),
      base::make_span(handles, num_handles), object);
  if (result != IPCZ_RESULT_OK) {
    return result;
  }

  *driver_handle = ObjectBase::ReleaseAsHandle(std::move(object));
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API CreateTransports(IpczDriverHandle transport0_handle,
                                     IpczDriverHandle transport1_handle,
                                     uint32_t flags,
                                     const void* options,
                                     IpczDriverHandle* new_transport0,
                                     IpczDriverHandle* new_transport1) {
  Transport* transport0 = Transport::FromHandle(transport0_handle);
  Transport* transport1 = Transport::FromHandle(transport1_handle);
  if (!transport0 || !transport1) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }
  auto [one, two] = Transport::CreatePair(transport0->destination(),
                                          transport1->destination());
  *new_transport0 = ObjectBase::ReleaseAsHandle(std::move(one));
  *new_transport1 = ObjectBase::ReleaseAsHandle(std::move(two));
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API
ActivateTransport(IpczDriverHandle transport_handle,
                  IpczHandle ipcz_transport,
                  IpczTransportActivityHandler activity_handler,
                  uint32_t flags,
                  const void* options) {
  Transport* transport = Transport::FromHandle(transport_handle);
  if (!transport) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  transport->Activate(ipcz_transport, activity_handler);
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API DeactivateTransport(IpczDriverHandle transport_handle,
                                        uint32_t flags,
                                        const void* options) {
  Transport* transport = Transport::FromHandle(transport_handle);
  if (!transport) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  transport->Deactivate();
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API Transmit(IpczDriverHandle transport_handle,
                             const void* data,
                             size_t num_bytes,
                             const IpczDriverHandle* handles,
                             size_t num_handles,
                             uint32_t flags,
                             const void* options) {
  Transport* transport = Transport::FromHandle(transport_handle);
  if (!transport) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  transport->Transmit(
      base::make_span(static_cast<const uint8_t*>(data), num_bytes),
      base::make_span(handles, num_handles));
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API
ReportBadTransportActivity(IpczDriverHandle transport_handle,
                           uintptr_t context,
                           uint32_t flags,
                           const void* options) {
  Transport* transport = Transport::FromHandle(transport_handle);
  if (!transport) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  std::unique_ptr<std::string> error_message(
      reinterpret_cast<std::string*>(context));
  transport->ReportBadActivity(*error_message);
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API AllocateSharedMemory(size_t num_bytes,
                                         uint32_t flags,
                                         const void* options,
                                         IpczDriverHandle* driver_memory) {
  auto region = base::UnsafeSharedMemoryRegion::Create(num_bytes);
  *driver_memory = SharedBuffer::ReleaseAsHandle(
      SharedBuffer::MakeForRegion(std::move(region)));
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API GetSharedMemoryInfo(IpczDriverHandle driver_memory,
                                        uint32_t flags,
                                        const void* options,
                                        IpczSharedMemoryInfo* info) {
  SharedBuffer* buffer = SharedBuffer::FromHandle(driver_memory);
  if (!buffer || !info || info->size < sizeof(*info)) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  info->region_num_bytes = buffer->region().GetSize();
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API DuplicateSharedMemory(IpczDriverHandle driver_memory,
                                          uint32_t flags,
                                          const void* options,
                                          IpczDriverHandle* new_driver_memory) {
  SharedBuffer* buffer = SharedBuffer::FromHandle(driver_memory);
  if (!buffer || !new_driver_memory) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  base::UnsafeSharedMemoryRegion new_region =
      base::UnsafeSharedMemoryRegion::Deserialize(buffer->region().Duplicate());
  if (!new_region.IsValid()) {
    return IPCZ_RESULT_RESOURCE_EXHAUSTED;
  }

  *new_driver_memory = SharedBuffer::ReleaseAsHandle(
      SharedBuffer::MakeForRegion(std::move(new_region)));
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API MapSharedMemory(IpczDriverHandle driver_memory,
                                    uint32_t flags,
                                    const void* options,
                                    void** address,
                                    IpczDriverHandle* driver_mapping) {
  SharedBuffer* buffer = SharedBuffer::FromHandle(driver_memory);
  if (!buffer || !driver_mapping) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  scoped_refptr<SharedBufferMapping> mapping =
      SharedBufferMapping::Create(buffer->region());
  if (!mapping) {
    return IPCZ_RESULT_RESOURCE_EXHAUSTED;
  }

  *address = mapping->memory();
  *driver_mapping = SharedBufferMapping::ReleaseAsHandle(std::move(mapping));
  return IPCZ_RESULT_OK;
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
    ReportBadTransportActivity,
    AllocateSharedMemory,
    GetSharedMemoryInfo,
    DuplicateSharedMemory,
    MapSharedMemory,
    GenerateRandomBytes,
};

}  // namespace mojo::core::ipcz_driver
