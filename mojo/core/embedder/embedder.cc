// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/embedder/embedder.h"

#include <stdint.h>

#include <optional>
#include <string>
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

#if BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)
#include <atomic>

#include "base/environment.h"
#include "mojo/core/core.h"
#include "mojo/core/entrypoints.h"
#include "mojo/core/node_controller.h"
#endif

#if !BUILDFLAG(IS_NACL)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include "mojo/core/channel_linux.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)
#endif  // !BUILDFLAG(IS_NACL)

namespace mojo::core {

namespace {

#if BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)
#if BUILDFLAG(IS_CHROMEOS) && !defined(ENABLE_IPCZ_ON_CHROMEOS)
std::atomic<bool> g_mojo_ipcz_enabled{false};
#elif !BUILDFLAG(IS_ANDROID)
// Default to enabled even if InitFeatures() is never called.
std::atomic<bool> g_mojo_ipcz_enabled{true};
#endif

bool g_mojo_ipcz_force_disabled = false;

std::optional<std::string> GetMojoIpczEnvVar() {
  std::string value;
  auto env = base::Environment::Create();
  if (!env->GetVar("MOJO_IPCZ", &value)) {
    return std::nullopt;
  }
  return value;
}

// Allows MojoIpcz to be forcibly enabled if and only if MOJO_IPCZ=1 in the
// environment. Note that any other value (or absence) has no influence on
// whether or not MojoIpcz is enabled.
bool IsMojoIpczForceEnabledByEnvironment() {
  static bool force_enabled = GetMojoIpczEnvVar() == "1";
  return force_enabled;
}
#endif  // BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)

bool g_enable_memv2 = false;

}  // namespace

// InitFeatures will be called as soon as the base::FeatureList is initialized.
void InitFeatures() {
  CHECK(base::FeatureList::GetInstance());

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) && \
    !BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
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

#if BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)
  if (base::FeatureList::IsEnabled(kMojoIpcz)) {
    EnableMojoIpcz();
  } else {
    g_mojo_ipcz_enabled.store(false, std::memory_order_release);
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  g_enable_memv2 = base::FeatureList::IsEnabled(kMojoIpczMemV2);
}

void EnableMojoIpcz() {
#if BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)
  g_mojo_ipcz_enabled.store(true, std::memory_order_release);
#endif
}

void Init(const Configuration& configuration) {
  internal::g_configuration = configuration;

#if BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)
  if (configuration.disable_ipcz) {
    // Allow the caller to override MojoIpcz even when enabled by Feature or
    // environment.
    g_mojo_ipcz_force_disabled = true;
  }
#else
  CHECK(!configuration.disable_ipcz);
#endif

  if (IsMojoIpczEnabled()) {
    CHECK(InitializeIpczNodeForProcess({
        .is_broker = configuration.is_broker_process,
        .use_local_shared_memory_allocation =
            configuration.is_broker_process ||
            configuration.force_direct_shared_memory_allocation,
        .enable_memv2 = g_enable_memv2,
    }));
    MojoEmbedderSetSystemThunks(GetMojoIpczImpl());
  } else {
#if BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)
    InitializeCore();
    MojoEmbedderSetSystemThunks(&GetSystemThunks());
#else
    NOTREACHED_NORETURN();
#endif
  }
}

void Init() {
  Init(Configuration());
}

void ShutDown() {
  if (IsMojoIpczEnabled()) {
    DestroyIpczNodeForProcess();
  } else {
#if BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)
    ShutDownCore();
#else
    NOTREACHED_NORETURN();
#endif
  }
}

scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() {
  if (IsMojoIpczEnabled()) {
    return ipcz_driver::Transport::GetIOTaskRunner();
  } else {
#if BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)
    return Core::Get()->GetNodeController()->io_task_runner();
#else
    NOTREACHED_NORETURN();
#endif
  }
}

bool IsMojoIpczEnabled() {
#if !BUILDFLAG(MOJO_SUPPORT_LEGACY_CORE)
  return true;
#else
  // Because Mojo and FeatureList are both brought up early in many binaries, it
  // can be tricky to ensure there aren't races that would lead to two different
  // Mojo implementations being selected at different points throughout the
  // process's lifetime. We cache the result of the first call to this function
  // and DCHECK that every subsequent call produces the same result. Note that
  // setting `disable_ipcz` in the Mojo config overrides both the Feature value
  // and the environment variable if set.
  const bool enabled = (g_mojo_ipcz_enabled.load(std::memory_order_acquire) ||
                        IsMojoIpczForceEnabledByEnvironment()) &&
                       !g_mojo_ipcz_force_disabled;
  static bool enabled_on_first_call = enabled;
  DCHECK_EQ(enabled, enabled_on_first_call);
  return enabled;
#endif
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
