// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/embedder/embedder.h"

#include <stdint.h>
#include <atomic>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "build/build_config.h"
#include "mojo/core/channel.h"
#include "mojo/core/configuration.h"
#include "mojo/core/core.h"
#include "mojo/core/core_ipcz.h"
#include "mojo/core/embedder/features.h"
#include "mojo/core/entrypoints.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/core/ipcz_driver/base_shared_memory_service.h"
#include "mojo/core/ipcz_driver/driver.h"
#include "mojo/core/ipcz_driver/transport.h"
#include "mojo/core/node_controller.h"
#include "mojo/public/c/system/thunks.h"

#if !BUILDFLAG(IS_NACL)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include "mojo/core/channel_linux.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)
#endif  // !BUILDFLAG(IS_NACL)

namespace mojo::core {

namespace {

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_FUCHSIA)
std::atomic<bool> g_mojo_ipcz_enabled{false};
#else
// Default to enabled even if InitFeatures() is never called.
std::atomic<bool> g_mojo_ipcz_enabled{true};
#endif

}  // namespace

// InitFeatures will be called as soon as the base::FeatureList is initialized.
void InitFeatures() {
  CHECK(base::FeatureList::GetInstance());

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_MAC)
  Channel::set_posix_use_writev(
      base::FeatureList::IsEnabled(kMojoPosixUseWritev));

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  bool shared_mem_enabled =
      base::FeatureList::IsEnabled(kMojoLinuxChannelSharedMem);
  bool use_zero_on_wake = kMojoLinuxChannelSharedMemEfdZeroOnWake.Get();
  int num_pages = kMojoLinuxChannelSharedMemPages.Get();
  if (num_pages < 0) {
    num_pages = 4;
  } else if (num_pages > 128) {
    num_pages = 128;
  }

  ChannelLinux::SetSharedMemParameters(shared_mem_enabled,
                                       static_cast<unsigned int>(num_pages),
                                       use_zero_on_wake);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_MAC)

  Channel::set_use_trivial_messages(
      base::FeatureList::IsEnabled(kMojoInlineMessagePayloads));

  Core::set_avoid_random_pipe_id(
      base::FeatureList::IsEnabled(kMojoAvoidRandomPipeId));

  if (base::FeatureList::IsEnabled(kMojoIpcz)) {
    EnableMojoIpcz();
  } else {
    g_mojo_ipcz_enabled.store(false, std::memory_order_release);
  }
}

void EnableMojoIpcz() {
  g_mojo_ipcz_enabled.store(true, std::memory_order_release);
}

void Init(const Configuration& configuration) {
  internal::g_configuration = configuration;

  if (configuration.disable_ipcz) {
    // Allow the caller to override MojoIpcz even when enabled as a Feature.
    g_mojo_ipcz_enabled.store(false, std::memory_order_release);
  }

  if (IsMojoIpczEnabled()) {
    CHECK(InitializeIpczNodeForProcess({
        .is_broker = configuration.is_broker_process,
        .use_local_shared_memory_allocation =
            configuration.is_broker_process ||
            configuration.force_direct_shared_memory_allocation,
    }));
    MojoEmbedderSetSystemThunks(GetMojoIpczImpl());
  } else {
    InitializeCore();
    MojoEmbedderSetSystemThunks(&GetSystemThunks());
  }
}

void Init() {
  Init(Configuration());
}

void ShutDown() {
  if (IsMojoIpczEnabled()) {
    DestroyIpczNodeForProcess();
  } else {
    ShutDownCore();
  }
}

scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() {
  if (IsMojoIpczEnabled()) {
    return ipcz_driver::Transport::GetIOTaskRunner();
  } else {
    return Core::Get()->GetNodeController()->io_task_runner();
  }
}

bool IsMojoIpczEnabled() {
  // Because Mojo and FeatureList are both brought up early in many binaries, it
  // can be tricky to ensure there aren't races that would lead to two different
  // Mojo implementations being selected at different points throughout the
  // process's lifetime. We cache the result of the first atomic load of this
  // flag; but we also DCHECK that any subsequent loads would match the cached
  // value, as a way to detect initialization races.
  static bool enabled = g_mojo_ipcz_enabled.load(std::memory_order_acquire);
  DCHECK_EQ(enabled, g_mojo_ipcz_enabled.load(std::memory_order_acquire));
  return enabled;
}

void InstallMojoIpczBaseSharedMemoryHooks() {
  DCHECK(IsMojoIpczEnabled());
  ipcz_driver::BaseSharedMemoryService::InstallHooks();
}

const IpczAPI& GetIpczAPIForMojo() {
  return GetIpczAPI();
}

const IpczDriver& GetIpczDriverForMojo() {
  return ipcz_driver::kDriver;
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
