// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/embedder/embedder.h"

#include <stdint.h>

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "build/build_config.h"
#include "mojo/buildflags.h"
#include "mojo/core/channel.h"
#include "mojo/core/configuration.h"
#include "mojo/core/core_ipcz.h"
#include "mojo/core/embedder/features.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/core/ipcz_driver/base_shared_memory_service.h"
#include "mojo/core/ipcz_driver/driver.h"
#include "mojo/core/ipcz_driver/transport.h"
#include "mojo/public/c/system/thunks.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include "mojo/core/channel_linux.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

namespace mojo::core {

namespace {

bool g_enable_memv2 = false;
IpczMemoryFlags g_memory_flags = IPCZ_MEMORY_FIXED_PARCEL_CAPACITY;

// Returns the IpczMemoryFlags for the parcel capacity policy selected by
// kMojoIpczParcelCapacityPolicy.
IpczMemoryFlags GetParcelCapacityPolicyMemoryFlags() {
  if (!base::FeatureList::IsEnabled(kMojoIpczParcelCapacityPolicy)) {
    return IPCZ_MEMORY_FIXED_PARCEL_CAPACITY;
  }
  switch (kMojoIpczParcelCapacityPolicyParam.Get()) {
    case MojoIpczParcelCapacityPolicy::kFixed:
      return IPCZ_MEMORY_FIXED_PARCEL_CAPACITY;
    case MojoIpczParcelCapacityPolicy::kNoCapacity:
      return IPCZ_MEMORY_NO_PARCEL_CAPACITY;
    case MojoIpczParcelCapacityPolicy::kExpandOnce:
      return IPCZ_MEMORY_EXPAND_PARCEL_CAPACITY_ONCE;
    case MojoIpczParcelCapacityPolicy::kRestrictedDynamic:
      return IPCZ_MEMORY_RESTRICTED_DYNAMIC_PARCEL_CAPACITY;
    case MojoIpczParcelCapacityPolicy::kDynamic:
      return IPCZ_NO_FLAGS;
  }
  NOTREACHED();
}

}  // namespace

// InitFeatures will be called as soon as the base::FeatureList is initialized.
void InitFeatures() {
  CHECK(base::FeatureList::GetInstance());

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  bool shared_mem_enabled = base::FeatureList::IsEnabled(kMojoUseEventFd);
  int num_pages = kMojoUseEventFdPages.Get();
  if (num_pages < 0) {
    num_pages = 4;
  } else if (num_pages > 128) {
    num_pages = 128;
  }

  ChannelLinux::SetSharedMemParameters(shared_mem_enabled,
                                       static_cast<unsigned int>(num_pages));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)

  g_enable_memv2 = base::FeatureList::IsEnabled(kMojoIpczMemV2);
  g_memory_flags = GetParcelCapacityPolicyMemoryFlags();
}

void Init(const Configuration& configuration) {
  internal::g_configuration = configuration;

  CHECK(InitializeIpczNodeForProcess({
      .is_broker = configuration.is_broker_process,
      .use_local_shared_memory_allocation =
          configuration.is_broker_process ||
          configuration.force_direct_shared_memory_allocation,
      .enable_memv2 = g_enable_memv2,
      .memory_flags = g_memory_flags,
  }));
  MojoEmbedderSetSystemThunks(GetMojoIpczImpl());
}

void Init() {
  Init(Configuration());
}

void ShutDown() {
  DestroyIpczNodeForProcess();
}

scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() {
  return ipcz_driver::Transport::GetIOTaskRunner();
}

void InstallMojoIpczBaseSharedMemoryHooks() {
  ipcz_driver::BaseSharedMemoryService::InstallHooks();
}

const IpczAPI& GetIpczAPIForMojo() {
  return GetIpczAPI();
}

const IpczDriver& GetIpczDriverForMojo() {
  return ipcz_driver::GetIpczDriver();
}

IpczDriverHandle CreateIpczTransportFromEndpoint(
    mojo::PlatformChannelEndpoint endpoint,
    const TransportEndpointTypes& endpoint_types,
    base::Process remote_process) {
  auto transport = ipcz_driver::Transport::Create(
      {
          .source = endpoint_types.local_is_broker
                        ? ipcz_driver::Transport::kBroker
                        : ipcz_driver::Transport::kNonBroker,
          .destination = endpoint_types.remote_is_broker
                             ? ipcz_driver::Transport::kBroker
                             : ipcz_driver::Transport::kNonBroker,
      },
      std::move(endpoint), std::move(remote_process));
  return ipcz_driver::ObjectBase::ReleaseAsHandle(std::move(transport));
}

}  // namespace mojo::core
