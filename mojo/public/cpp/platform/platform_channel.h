// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_PLATFORM_PLATFORM_CHANNEL_H_
#define MOJO_PUBLIC_CPP_PLATFORM_PLATFORM_CHANNEL_H_

#include "base/command_line.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/process/launch.h"
#include "build/build_config.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"

namespace mojo {

// PlatformChannel encapsulates construction and ownership of two entangled
// endpoints of a platform-specific communication primitive, e.g. a Windows
// pipe, a Unix domain socket, or a macOS Mach port pair. One endpoint is
// designated as the "local" endpoint and should be retained by the creating
// process; the other endpoint is designated as the "remote" endpoint and
// should be passed to an external process.
//
// PlatformChannels can be used to bootstrap Mojo IPC between one process and
// another. Typically the other process is a child of this process, and there
// are helper methods for passing the endpoint to a child as such; but this
// arrangement is not strictly necessary on all platforms.
//
// For a channel which allows clients to connect by name (i.e. a named pipe
// or socket server, supported only on Windows and POSIX systems) see
// NamedPlatformChannel.
class COMPONENT_EXPORT(MOJO_CPP_PLATFORM) PlatformChannel {
 public:
  // A common helper constant that is used to pass handle values on the
  // command line when the relevant methods are used on this class.
  static const char kHandleSwitch[];

// Unfortunately base process support code has no unified handle-passing
// data pipe, so we have this.
#if defined(OS_WIN)
  using HandlePassingInfo = base::HandlesToInheritVector;
#elif defined(OS_FUCHSIA)
  using HandlePassingInfo = base::HandlesToTransferVector;
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  using HandlePassingInfo = base::MachPortsForRendezvous;
#elif defined(OS_POSIX)
  using HandlePassingInfo = base::FileHandleMappingVector;
#else
#error "Unsupported platform."
#endif

  PlatformChannel();
  PlatformChannel(PlatformChannel&& other);
  ~PlatformChannel();

  PlatformChannel& operator=(PlatformChannel&& other);

  const PlatformChannelEndpoint& local_endpoint() const {
    return local_endpoint_;
  }
  const PlatformChannelEndpoint& remote_endpoint() const {
    return remote_endpoint_;
  }

  PlatformChannelEndpoint TakeLocalEndpoint() WARN_UNUSED_RESULT {
    return std::move(local_endpoint_);
  }

  PlatformChannelEndpoint TakeRemoteEndpoint() WARN_UNUSED_RESULT {
    return std::move(remote_endpoint_);
  }

  // Prepares to pass the remote endpoint handle to a process that will soon be
  // launched. Returns a string that can be used in the remote process with
  // |RecoverPassedEndpointFromString()| (see below). The string can e.g. be
  // passed on the new process's command line.
  //
  // **NOTE**: If this method is called it is important to also call
  // |RemoteProcessLaunchAttempted()| on this PlatformChannel *after* attempting
  // to launch the new process, regardless of whether the attempt succeeded.
  // Failing to do so can result in leaked handles.
  void PrepareToPassRemoteEndpoint(HandlePassingInfo* info, std::string* value);

  // Like above but modifies |*command_line| to include the endpoint string
  // via the |kHandleSwitch| flag.
  void PrepareToPassRemoteEndpoint(HandlePassingInfo* info,
                                   base::CommandLine* command_line);

  // Like above but adds handle-passing information directly to
  // |*launch_options|, eliminating the potential need for callers to write
  // platform-specific code to do the same.
  void PrepareToPassRemoteEndpoint(base::LaunchOptions* options,
                                   base::CommandLine* command_line);

  // Must be called after the corresponding process launch attempt if
  // |PrepareToPassRemoteEndpoint()| was used.
  void RemoteProcessLaunchAttempted();

  // Recovers an endpoint handle which was passed to the calling process by
  // its creator. |value| is a string returned by
  // |PrepareToPassRemoteEndpoint()| in the creator's process.
  static PlatformChannelEndpoint RecoverPassedEndpointFromString(
      base::StringPiece value) WARN_UNUSED_RESULT;

  // Like above but extracts the input string from |command_line| via the
  // |kHandleSwitch| flag.
  static PlatformChannelEndpoint RecoverPassedEndpointFromCommandLine(
      const base::CommandLine& command_line) WARN_UNUSED_RESULT;

 private:
  PlatformChannelEndpoint local_endpoint_;
  PlatformChannelEndpoint remote_endpoint_;

  DISALLOW_COPY_AND_ASSIGN(PlatformChannel);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_PLATFORM_PLATFORM_CHANNEL_H_
