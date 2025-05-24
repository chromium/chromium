// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/platform_channel_endpoint.h"

#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "mojo/public/cpp/platform/platform_channel.h"

#if BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
#include <mach/port.h>

#include "base/apple/mach_port_rendezvous.h"
#include "base/apple/scoped_mach_port.h"
#elif BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/handle.h>
#elif BUILDFLAG(IS_POSIX)
#include "base/files/scoped_file.h"
#include "base/posix/global_descriptors.h"
#elif BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/scoped_handle.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/binder.h"
#endif

namespace mojo {

namespace {

#if BUILDFLAG(IS_ANDROID)
// Leave room for any other descriptors defined in content for example.
// TODO(crbug.com/40499227): Consider changing base::GlobalDescriptors to
// generate a key when setting the file descriptor.
constexpr int kAndroidClientHandleDescriptor =
    base::GlobalDescriptors::kBaseDescriptor + 10000;
constexpr std::string_view kBinderValuePrefix = "binder:";
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
bool IsTargetDescriptorUsed(const base::FileHandleMappingVector& mapping,
                            int target_fd) {
  for (auto& [i, fd] : mapping) {
    if (fd == target_fd)
      return true;
  }
  return false;
}
#endif

}  // namespace

PlatformChannelEndpoint::PlatformChannelEndpoint() = default;

PlatformChannelEndpoint::PlatformChannelEndpoint(
    PlatformChannelEndpoint&& other) = default;

PlatformChannelEndpoint::PlatformChannelEndpoint(PlatformHandle handle)
    : handle_(std::move(handle)) {}

PlatformChannelEndpoint::~PlatformChannelEndpoint() = default;

PlatformChannelEndpoint& PlatformChannelEndpoint::operator=(
    PlatformChannelEndpoint&& other) = default;

void PlatformChannelEndpoint::reset() {
  handle_.reset();
}

PlatformChannelEndpoint PlatformChannelEndpoint::Clone() const {
  return PlatformChannelEndpoint(handle_.Clone());
}

void PlatformChannelEndpoint::PrepareToPass(HandlePassingInfo& info,
                                            base::CommandLine& command_line) {
  std::string value;
  PrepareToPass(info, value);
  if (!value.empty()) {
    command_line.AppendSwitchASCII(PlatformChannel::kHandleSwitch, value);
  }
}

void PlatformChannelEndpoint::PrepareToPass(HandlePassingInfo& info,
                                            std::string& value) {
  DCHECK(is_valid());
#if BUILDFLAG(IS_WIN)
  info.push_back(platform_handle().GetHandle().Get());
  value =
      base::NumberToString(HandleToLong(platform_handle().GetHandle().Get()));
#elif BUILDFLAG(IS_FUCHSIA)
  const uint32_t id = base::LaunchOptions::AddHandleToTransfer(
      &info, platform_handle().GetHandle().get());
  value = base::NumberToString(id);
#elif BUILDFLAG(IS_ANDROID)
  int fd = platform_handle().GetFD().get();
  int mapped_fd = kAndroidClientHandleDescriptor + info.size();
  info.emplace_back(fd, mapped_fd);
  value = base::NumberToString(mapped_fd);
#elif BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
  DCHECK(platform_handle().is_mach_receive());
  base::apple::ScopedMachReceiveRight receive_right =
      TakePlatformHandle().TakeMachReceiveRight();
  base::MachPortsForRendezvous::key_type rendezvous_key = 0;
  do {
    rendezvous_key = static_cast<decltype(rendezvous_key)>(base::RandUint64());
  } while (info.find(rendezvous_key) != info.end());
  auto it = info.insert(std::make_pair(
      rendezvous_key, base::MachRendezvousPort(std::move(receive_right))));
  DCHECK(it.second) << "Failed to insert port for rendezvous.";
  value = base::NumberToString(rendezvous_key);
#elif BUILDFLAG(IS_POSIX)
  // Arbitrary sanity check to ensure the loop below terminates reasonably
  // quickly.
  CHECK_LT(info.size(), 1000u);

  // Find a suitable FD to map the remote endpoint handle to in the child
  // process. This has quadratic time complexity in the size of |*info|, but
  // |*info| should be very small and is usually empty.
  int target_fd = base::GlobalDescriptors::kBaseDescriptor;
  while (IsTargetDescriptorUsed(info, target_fd)) {
    ++target_fd;
  }
  info.emplace_back(platform_handle().GetFD().get(), target_fd);
  value = base::NumberToString(target_fd);
#endif
}

void PlatformChannelEndpoint::PrepareToPass(base::LaunchOptions& options,
                                            base::CommandLine& command_line) {
  const std::string value = PrepareToPass(options);
  if (!value.empty()) {
    command_line.AppendSwitchASCII(PlatformChannel::kHandleSwitch, value);
  }
}

std::string PlatformChannelEndpoint::PrepareToPass(
    base::LaunchOptions& options) {
  std::string value;
#if BUILDFLAG(IS_WIN)
  PrepareToPass(options.handles_to_inherit, value);
#elif BUILDFLAG(IS_FUCHSIA)
  PrepareToPass(options.handles_to_transfer, value);
#elif BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
  PrepareToPass(options.mach_ports_for_rendezvous, value);
#elif BUILDFLAG(IS_POSIX)
#if BUILDFLAG(IS_ANDROID)
  if (platform_handle().is_valid_binder()) {
    value = base::StrCat(
        {kBinderValuePrefix, base::NumberToString(options.binders.size())});
    options.binders.push_back(platform_handle().GetBinder());
    return value;
  }
#endif
  PrepareToPass(options.fds_to_remap, value);
#else
#error "Platform not supported."
#endif
  return value;
}

void PlatformChannelEndpoint::ProcessLaunchAttempted() {
#if BUILDFLAG(IS_FUCHSIA)
  // Unlike other platforms, Fuchsia transfers handle ownership to the new
  // process, rather than duplicating it. For consistency the process-launch
  // call will have consumed the handle regardless of whether launch succeeded.
  DCHECK(platform_handle().is_valid_handle());
  std::ignore = TakePlatformHandle().ReleaseHandle();
#else
  reset();
#endif
}

// static
PlatformChannelEndpoint PlatformChannelEndpoint::RecoverFromString(
    std::string_view value) {
#if BUILDFLAG(IS_WIN)
  int handle_value = 0;
  if (value.empty() || !base::StringToInt(value, &handle_value)) {
    DLOG(ERROR) << "Invalid PlatformChannel endpoint string.";
    return PlatformChannelEndpoint();
  }
  return PlatformChannelEndpoint(
      PlatformHandle(base::win::ScopedHandle(LongToHandle(handle_value))));
#elif BUILDFLAG(IS_FUCHSIA)
  unsigned int handle_value = 0;
  if (value.empty() || !base::StringToUint(value, &handle_value)) {
    DLOG(ERROR) << "Invalid PlatformChannel endpoint string.";
    return PlatformChannelEndpoint();
  }
  return PlatformChannelEndpoint(PlatformHandle(zx::handle(
      zx_take_startup_handle(base::checked_cast<uint32_t>(handle_value)))));
#elif BUILDFLAG(IS_ANDROID)
  if (value.starts_with(kBinderValuePrefix)) {
    size_t index;
    if (!base::StringToSizeT(value.substr(kBinderValuePrefix.size()), &index)) {
      DLOG(ERROR) << "Invalid binder endpoint string";
      return PlatformChannelEndpoint();
    }
    base::android::BinderRef binder =
        base::android::TakeBinderFromParent(index);
    if (!binder) {
      DLOG(ERROR) << "Missing binder endpoint " << index;
      return PlatformChannelEndpoint();
    }
    return PlatformChannelEndpoint(PlatformHandle(std::move(binder)));
  }
  base::GlobalDescriptors::Key key = -1;
  if (value.empty() || !base::StringToUint(value, &key)) {
    DLOG(ERROR) << "Invalid PlatformChannel endpoint string.";
    return PlatformChannelEndpoint();
  }
  return PlatformChannelEndpoint(PlatformHandle(
      base::ScopedFD(base::GlobalDescriptors::GetInstance()->Get(key))));
#elif BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
  auto* client = base::MachPortRendezvousClient::GetInstance();
  if (!client) {
    DLOG(ERROR) << "Mach rendezvous failed.";
    return PlatformChannelEndpoint();
  }
  uint32_t rendezvous_key = 0;
  if (value.empty() || !base::StringToUint(value, &rendezvous_key)) {
    DLOG(ERROR) << "Invalid PlatformChannel rendezvous key.";
    return PlatformChannelEndpoint();
  }
  auto receive = client->TakeReceiveRight(rendezvous_key);
  if (!receive.is_valid()) {
    DLOG(ERROR) << "Invalid PlatformChannel receive right.";
    return PlatformChannelEndpoint();
  }
  return PlatformChannelEndpoint(PlatformHandle(std::move(receive)));
#elif BUILDFLAG(IS_POSIX)
  int fd = -1;
  if (value.empty() || !base::StringToInt(value, &fd) ||
      fd < base::GlobalDescriptors::kBaseDescriptor) {
    DLOG(ERROR) << "Invalid PlatformChannel endpoint string.";
    return PlatformChannelEndpoint();
  }
  return PlatformChannelEndpoint(PlatformHandle(base::ScopedFD(fd)));
#endif
}

}  // namespace mojo
