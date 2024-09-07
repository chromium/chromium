// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/base_shared_memory_service.h"

#include <tuple>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/shared_memory_hooks.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "mojo/core/broker.h"
#include "mojo/core/broker_host.h"
#include "mojo/core/connection_params.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/core/ipcz_driver/transport.h"
#include "mojo/core/scoped_ipcz_handle.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

#define SHARED_MEMORY_SERVICE_REQUIRED()                                \
  BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_APPLE) && \
      !BUILDFLAG(IS_ANDROID)

namespace mojo::core::ipcz_driver {

namespace {

#if SHARED_MEMORY_SERVICE_REQUIRED()

void CreateBrokerHostOnIOThread(PlatformChannelEndpoint endpoint) {
  // Self-owned. Note that a valid remote process handle is only needed by
  // BrokerHost on Windows, but MojoIpcz doesn't yet support any version of
  // Windows which requires use of the shared memory service (i.e., Windows
  // versions older than 8.1).
  new BrokerHost(base::Process(), ConnectionParams(std::move(endpoint)),
                 base::NullCallback());
}

void CreateServiceFromNextParcel(ScopedIpczHandle portal) {
  ScopedIpczHandle box;
  size_t num_handles = 1;
  const IpczResult result =
      GetIpczAPI().Get(portal.get(), IPCZ_NO_FLAGS, nullptr, nullptr, nullptr,
                       ScopedIpczHandle::Receiver(box), &num_handles, nullptr);

  if (result != IPCZ_RESULT_OK) {
    DLOG(ERROR) << "Invalid shared memory client connection";
    return;
  }

  auto transport = Transport::Unbox(box.get());
  if (!transport) {
    DLOG(ERROR) << "Invalid shared memory client connection";
    return;
  }

  // On successful unboxing, the box is consumed.
  std::ignore = box.release();
  Transport::GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CreateBrokerHostOnIOThread, transport->TakeEndpoint()));
}

void WaitForClientConnection(ScopedIpczHandle portal) {
  const IpczTrapConditions conditions{
      .size = sizeof(conditions),
      .flags = IPCZ_TRAP_DEAD | IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS,
      .min_local_parcels = 0,
  };

  auto handler = [](const IpczTrapEvent* event) {
    ScopedIpczHandle portal(static_cast<IpczHandle>(event->context));
    if (event->condition_flags & IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS) {
      CreateServiceFromNextParcel(std::move(portal));
    }
  };

  IpczTrapConditionFlags satisfied_conditions;
  const IpczResult result =
      GetIpczAPI().Trap(portal.get(), &conditions, handler,
                        reinterpret_cast<uintptr_t>(portal.get()),
                        IPCZ_NO_FLAGS, nullptr, &satisfied_conditions, nullptr);
  // Trap installed; we'll invoke CreateServiceFromNextParcel() as soon as a
  // parcel arrives with a channel endpoint.
  if (result == IPCZ_RESULT_OK) {
    // Ownership assumed by the trap event handler above.
    std::ignore = portal.release();
    return;
  }

  // If there's already a parcel waiting, we can read it immediately and set up
  // the service instance.
  if (result == IPCZ_RESULT_FAILED_PRECONDITION &&
      (satisfied_conditions & IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS) != 0) {
    CreateServiceFromNextParcel(std::move(portal));
    return;
  }
}

Broker* g_client = nullptr;

#endif  // SHARED_MEMORY_SERVICE_REQUIRED()

base::WritableSharedMemoryRegion CreateWritableSharedMemoryRegion(size_t size) {
  return BaseSharedMemoryService::CreateWritableRegion(size);
}

base::MappedReadOnlyRegion CreateReadOnlySharedMemoryRegion(
    size_t size,
    base::SharedMemoryMapper* mapper) {
  auto writable_region = CreateWritableSharedMemoryRegion(size);
  if (!writable_region.IsValid()) {
    return {};
  }

  base::WritableSharedMemoryMapping mapping = writable_region.Map(mapper);
  return {base::WritableSharedMemoryRegion::ConvertToReadOnly(
              std::move(writable_region)),
          std::move(mapping)};
}

base::UnsafeSharedMemoryRegion CreateUnsafeSharedMemoryRegion(size_t size) {
  auto writable_region = CreateWritableSharedMemoryRegion(size);
  if (!writable_region.IsValid()) {
    return {};
  }

  return base::WritableSharedMemoryRegion::ConvertToUnsafe(
      std::move(writable_region));
}

}  // namespace

// static
void BaseSharedMemoryService::CreateService(ScopedIpczHandle portal) {
#if SHARED_MEMORY_SERVICE_REQUIRED()
  WaitForClientConnection(std::move(portal));
#endif
}

// static
void BaseSharedMemoryService::CreateClient(ScopedIpczHandle portal) {
#if SHARED_MEMORY_SERVICE_REQUIRED()
  PlatformChannel channel;

  ScopedIpczHandle box{Transport::Box(Transport::Create(
      {.source = Transport::kBroker, .destination = Transport::kNonBroker},
      channel.TakeRemoteEndpoint()))};
  const IpczResult result = GetIpczAPI().Put(
      portal.get(), nullptr, 0, &box.get(), 1, IPCZ_NO_FLAGS, nullptr);
  if (result == IPCZ_RESULT_OK) {
    // On success, ownership of the box is passed into the portal.
    std::ignore = box.release();
    g_client = new Broker(channel.TakeLocalEndpoint().TakePlatformHandle(),
                          /*wait_for_channel_handle=*/false);
  }
#endif
}

// static
void BaseSharedMemoryService::InstallHooks() {
  base::SharedMemoryHooks::SetCreateHooks(&CreateReadOnlySharedMemoryRegion,
                                          &CreateUnsafeSharedMemoryRegion,
                                          &CreateWritableSharedMemoryRegion);
}

// static
base::WritableSharedMemoryRegion BaseSharedMemoryService::CreateWritableRegion(
    size_t size) {
#if SHARED_MEMORY_SERVICE_REQUIRED()
  if (!g_client) {
    return {};
  }
  return g_client->GetWritableSharedMemoryRegion(size);
#else
  // The shared memory service is not needed on other platforms, so this method
  // should never be called.
  NOTREACHED();
#endif
}

}  // namespace mojo::core::ipcz_driver
