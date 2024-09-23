// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_PLATFORM_PLATFORM_CHANNEL_ENDPOINT_H_
#define MOJO_PUBLIC_CPP_PLATFORM_PLATFORM_CHANNEL_ENDPOINT_H_

#include <string_view>

#include "base/command_line.h"
#include "base/component_export.h"
#include "base/process/launch.h"
#include "build/build_config.h"
#include "mojo/buildflags.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace mojo {

// A PlatformHandle with a little extra type information to convey that it's
// a channel endpoint, i.e. a handle that can be used to send or receive
// invitations as |MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL| to a remote
// PlatformChannelEndpoint.
class COMPONENT_EXPORT(MOJO_CPP_PLATFORM) PlatformChannelEndpoint {
 public:
// Unfortunately base process support code has no unified handle-passing
// data pipe, so we have this.
#if BUILDFLAG(IS_WIN)
  using HandlePassingInfo = base::HandlesToInheritVector;
#elif BUILDFLAG(IS_FUCHSIA)
  using HandlePassingInfo = base::HandlesToTransferVector;
#elif BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
  using HandlePassingInfo = base::MachPortsForRendezvous;
#elif BUILDFLAG(IS_POSIX)
  using HandlePassingInfo = base::FileHandleMappingVector;
#else
#error "Unsupported platform."
#endif

  PlatformChannelEndpoint();
  PlatformChannelEndpoint(PlatformChannelEndpoint&& other);
  explicit PlatformChannelEndpoint(PlatformHandle handle);

  PlatformChannelEndpoint(const PlatformChannelEndpoint&) = delete;
  PlatformChannelEndpoint& operator=(const PlatformChannelEndpoint&) = delete;

  ~PlatformChannelEndpoint();

  PlatformChannelEndpoint& operator=(PlatformChannelEndpoint&& other);

  bool is_valid() const { return handle_.is_valid(); }
  void reset();
  PlatformChannelEndpoint Clone() const;

  const PlatformHandle& platform_handle() const { return handle_; }

  [[nodiscard]] PlatformHandle TakePlatformHandle() {
    return std::move(handle_);
  }

  // Prepares to pass this endpoint handle to a process that will soon be
  // launched. Returns a string that can be used in the remote process with
  // RecoverFromString() (see below). The string can be passed on the new
  // process's command line.
  //
  // NOTE: If this method is called it is important to also call
  // ProcessLaunchAttempted() on this endpoint *after* attempting to launch
  // the new process, regardless of whether the attempt succeeded. Failing to do
  // so can result in leaked handles on some platforms.
  void PrepareToPass(HandlePassingInfo& info, std::string& value);

  // Like above but modifies `command_line` to include the endpoint string
  // via the PlatformChannel::kHandleSwitch flag.
  void PrepareToPass(HandlePassingInfo& info, base::CommandLine& command_line);

  // Like above but adds handle-passing information directly to
  // `launch_options`, eliminating the potential need for callers to write
  // platform-specific code to do the same.
  void PrepareToPass(base::LaunchOptions& options,
                     base::CommandLine& command_line);

  // Like above but returns an appropriate switch value as a string.
  std::string PrepareToPass(base::LaunchOptions& options);

  // Must be called after the corresponding process launch attempt if
  // PrepareToPass() was called.
  void ProcessLaunchAttempted();

  [[nodiscard]] static PlatformChannelEndpoint RecoverFromString(
      std::string_view value);

 private:
  PlatformHandle handle_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_PLATFORM_PLATFORM_CHANNEL_ENDPOINT_H_
