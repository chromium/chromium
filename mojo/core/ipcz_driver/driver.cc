// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/ipcz_driver/driver.h"

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <utility>

#include "base/containers/span.h"
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
                              volatile void* data,
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

  // TODO(crbug.com/40270656): Propagate the volatile qualifier on
  // `data`.
  const IpczResult result = transport->SerializeObject(
      *object, const_cast<void*>(data), num_bytes, handles, num_handles);
  if (result != IPCZ_RESULT_OK) {
    return result;
  }

  // On success we consume the object reference owned by the input handle.
  std::ignore = ObjectBase::TakeFromHandle(handle);
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API Deserialize(const volatile void* data,
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

  // TODO(crbug.com/40270656): Propagate the volatile qualifier on
  // `data`.
  scoped_refptr<ObjectBase> object;
  const IpczResult result = transport->DeserializeObject(
      base::make_span(
          static_cast<const uint8_t*>(const_cast<const void*>(data)),
          num_bytes),
      base::make_span(handles, num_handles), object);
  if (result != IPCZ_RESULT_OK) {
    return result;
  }

  *driver_handle = ObjectBase::ReleaseAsHandle(std::move(object));
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API
CreateTransports(IpczDriverHandle existing_transport_from_here_to_a,
                 IpczDriverHandle existing_transport_from_here_to_b,
                 uint32_t flags,
                 const void* options,
                 IpczDriverHandle* new_transport_from_a_to_b,
                 IpczDriverHandle* new_transport_from_b_to_a) {
  // For two existing transports from the calling node (one to a node A and
  // another to a node B), this creates a new transport that will be used to
  // connect A and B directly to each other.
  //
  // The output `new_transport_from_a_to_b` is created to be sent to node A via
  // `existing_transport_from_here_to_a`, while the output
  // `new_transport_from_b_to_a` will be sent to node B via
  // `existing_transport_from_here_to_b`.
  //
  // This function does not actually send the transports though: it only creates
  // them and configures them with appropriate relative levels of trust in each
  // other.
  Transport* our_transport_to_a =
      Transport::FromHandle(existing_transport_from_here_to_a);
  Transport* our_transport_to_b =
      Transport::FromHandle(existing_transport_from_here_to_b);
  if (!our_transport_to_a || !our_transport_to_b) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  const Transport::EndpointType node_a_type =
      our_transport_to_a->destination_type();
  const Transport::EndpointType node_b_type =
      our_transport_to_b->destination_type();
  auto [transport_from_a_to_b, transport_from_b_to_a] =
      Transport::CreatePair(node_a_type, node_b_type);
  if (node_a_type == Transport::EndpointType::kBroker) {
    // If node A is a broker, give its new endpoint a handle to node B's process
    // when possible.
    transport_from_a_to_b->set_remote_process(
        our_transport_to_b->remote_process().Duplicate());
  }

  // A can trust B if we trust B. Note that this is only true if A also trusts
  // us, which A must validate when accepting this transport from us. See
  // Transport::Deserialize() for that validation.
  transport_from_a_to_b->set_is_peer_trusted(
      our_transport_to_b->is_peer_trusted());

  // If A can assume it's trusted by us then it can also assume it's trusted by
  // B, because we will tell B to do so.
  transport_from_a_to_b->set_is_trusted_by_peer(
      our_transport_to_a->is_trusted_by_peer());

  if (node_b_type == Transport::EndpointType::kBroker) {
    // If node B is a broker, give its new endpoint a handle to node A's process
    // when possible.
    transport_from_b_to_a->set_remote_process(
        our_transport_to_a->remote_process().Duplicate());
  }

  // Similar to above: B can trust A if we trust A; and if B can assume it's
  // trusted by us then it can assume it's trusted by A too.
  transport_from_b_to_a->set_is_peer_trusted(
      our_transport_to_a->is_peer_trusted());
  transport_from_b_to_a->set_is_trusted_by_peer(
      our_transport_to_b->is_trusted_by_peer());

  *new_transport_from_a_to_b =
      ObjectBase::ReleaseAsHandle(std::move(transport_from_a_to_b));
  *new_transport_from_b_to_a =
      ObjectBase::ReleaseAsHandle(std::move(transport_from_b_to_a));
  return IPCZ_RESULT_OK;
}

IpczResult IPCZ_API
ActivateTransport(IpczDriverHandle transport_handle,
                  IpczHandle listener,
                  IpczTransportActivityHandler activity_handler,
                  uint32_t flags,
                  const void* options) {
  Transport* transport = Transport::FromHandle(transport_handle);
  if (!transport) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  transport->Activate(listener, activity_handler);
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
                                    volatile void** address,
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
  base::RandBytes(
      // SAFETY: This requires the caller to provide a valid pointer/size pair.
      // The function API is a C API so can't use a span.
      UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(buffer), num_bytes)));
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
