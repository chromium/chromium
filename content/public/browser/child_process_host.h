// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_HOST_H_
#define CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_HOST_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/clang_profiling_buildflags.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "content/public/common/content_constants.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"

namespace base {
#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
class File;
#endif
class FilePath;
}  // namespace base

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
namespace IPC {
class MessageFilter;
}
#endif

namespace mojo {
class OutgoingInvitation;
}

namespace content {

class ChildProcessHostDelegate;

// This represents a non-browser process. This can include traditional child
// processes like plugins, or an embedder could even use this for long lived
// processes that run independent of the browser process.
class CONTENT_EXPORT ChildProcessHost : public IPC::Sender {
 public:
  ~ChildProcessHost() override;

  // This is a value never returned as the unique id of any child processes of
  // any kind, including the values returned by RenderProcessHost::GetID().
  enum : int { kInvalidUniqueID = kInvalidChildProcessUniqueId };

  // Every ChildProcessHost provides a single primordial Mojo message pipe to
  // the launched child process, with the other end held by the
  // ChildProcessHost.
  //
  // This enum (given to |Create()|) determines how the ChildProcessHost uses
  // the pipe.
  enum class IpcMode {
    // In this mode, the primordial pipe is a content.mojom.ChildProcess pipe.
    // The ChildProcessHost is fully functional in this mode, and all new
    // process hosts should prefer to use this mode.
    kNormal,

    // In this mode, the primordial pipe is a legacy IPC Channel bootstrapping
    // pipe (IPC.mojom.ChannelBootstrap). This should be used when the child
    // process only uses legacy Chrome IPC (e.g. Chrome's NaCl processes.)
    //
    // In this mode, ChildProcessHost methods like |BindReceiver()| are not
    // functional.
    //
    // DEPRECATED: Do not introduce new uses of this mode.
    kLegacy,
  };

  // Used to create a child process host. The delegate must outlive this object.
  static std::unique_ptr<ChildProcessHost> Create(
      ChildProcessHostDelegate* delegate,
      IpcMode ipc_mode);

  // Returns a unique ID to identify a child process. Used by both child
  // processes that are derived from ChildProcessHost, but also used to generate
  // IDs for RenderProcessHost as well as embedded specific child processes.
  // This ensures that IDs are unique for all different types of child
  // processes.
  //
  // This function is threadsafe since RenderProcessHost is on the UI thread,
  // but normally this will be used on the IO thread.
  //
  // This will never return kInvalidUniqueID.
  static int GenerateChildProcessUniqueId();

  // These flags may be passed to GetChildPath in order to alter its behavior,
  // causing it to return a child path more suited to a specific task.
  enum {
    // No special behavior requested.
    CHILD_NORMAL = 0,

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // Indicates that the child execed after forking may be execced from
    // /proc/self/exe rather than using the "real" app path. This prevents
    // autoupdate from confusing us if it changes the file out from under us.
    // You will generally want to set this on Linux, except when there is an
    // override to the command line (for example, we're forking a renderer in
    // gdb). In this case, you'd use GetChildPath to get the real executable
    // file name, and then prepend the GDB command to the command line.
    CHILD_ALLOW_SELF = 1 << 0,
#elif BUILDFLAG(IS_MAC)
    // Note, on macOS these are not bitwise flags and each value is mutually
    // exclusive with the others. Each one of these options should correspond
    // to a value in //content/public/app/mac_helpers.gni.

    // Starts a child process with the macOS entitlement that allows JIT (i.e.
    // memory that is writable and executable). In order to make use of this,
    // memory cannot simply be allocated as read-write-execute; instead, the
    // MAP_JIT flag must be passed to mmap() when allocating the memory region
    // into which the writable-and-executable data are stored.
    CHILD_RENDERER,

    // Starts a child process with the macOS entitlement that allows unsigned
    // executable memory.
    // TODO(crbug.com/40636855): Change this to use MAP_JIT and the
    // allow-jit entitlement instead.
    CHILD_GPU,

    // Starts a child process with the macOS entitlement that ignores the
    // library validation code signing enforcement. Library validation mandates
    // that all executable pages be backed by a code signature that either 1)
    // is signed by Apple, or 2) signed by the same Team ID as the main
    // executable. Binary plug-ins that are not always signed by the same Team
    // ID as the main binary, so this flag should be used when needing to load
    // third-party plug-ins.
    CHILD_PLUGIN,

    // Marker for the start of embedder-specific helper child process types.
    // Values greater than CHILD_EMBEDDER_FIRST are reserved to be used by the
    // embedder to add custom process types and will be resolved via
    // ContentClient::GetChildPath().
    CHILD_EMBEDDER_FIRST,
#endif
  };

  // Returns the pathname to be used for a child process.  If a subprocess
  // pathname was specified on the command line, that will be used.  Otherwise,
  // the default child process pathname will be returned.  On most platforms,
  // this will be the same as the currently-executing process.
  //
  // The |flags| argument accepts one or more flags such as CHILD_ALLOW_SELF.
  // Pass only CHILD_NORMAL if none of these special behaviors are required.
  //
  // On failure, returns an empty FilePath.
  static base::FilePath GetChildPath(int flags);

  // Send the shutdown message to the child process.
  virtual void ForceShutdown() = 0;

  // Exposes the outgoing Mojo invitation for this ChildProcessHost. The
  // invitation can be given to ChildProcessLauncher to ensure that this
  // ChildProcessHost's primordial Mojo IPC calls can properly communicate with
  // the launched process.
  //
  // Always valid immediately after ChildProcessHost construction, but may be
  // null if someone else has taken ownership.
  virtual std::optional<mojo::OutgoingInvitation>& GetMojoInvitation() = 0;

  // Creates a legacy IPC channel over a Mojo message pipe. Must be called if
  // legacy IPC will be used to communicate with the child process, but
  // otherwise should not be called.
  virtual void CreateChannelMojo() = 0;

  // Returns true iff the IPC channel is currently being opened; this means
  // CreateChannelMojo() has been called, but OnChannelConnected() has not yet
  // been invoked.
  virtual bool IsChannelOpening() = 0;

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  // Adds an IPC message filter.  A reference will be kept to the filter.
  virtual void AddFilter(IPC::MessageFilter* filter) = 0;
#endif

  // Bind an interface exposed by the child process. Whether or not the
  // interface in |receiver| can be bound depends on the process type and
  // potentially on the Content embedder.
  //
  // Receivers passed to this call arrive in the child process and go through
  // the following flow, stopping if any step decides to bind the receiver:
  //
  //   1. IO thread, ChildProcessImpl::BindReceiver.
  //   2. IO thread, ContentClient::BindChildProcessInterface.
  //   3. Main thread, ChildThreadImpl::BindReceiver (virtual).
  virtual void BindReceiver(mojo::GenericPendingReceiver receiver) = 0;

  // Asks the child process to prioritize energy efficiency because the
  // embedder is in battery saver mode. The default state is `false`, meaning
  // the power/speed tuning is left up to the different components to figure
  // out.
  virtual void SetBatterySaverMode(bool battery_saver_mode_enabled) = 0;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Reinitializes the child process's logging with the given settings. This
  // is needed on Chrome OS, which switches to a log file in the user's home
  // directory once they log in.
  virtual void ReinitializeLogging(uint32_t logging_dest,
                                   base::ScopedFD log_file_descriptor) = 0;
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
  // Write out the accumulated code profiling profile to the configured file.
  // The callback is invoked once the profile has been flushed to disk.
  virtual void DumpProfilingData(base::OnceClosure callback) = 0;

  // Sets the profiling file for the child process.
  // Used for the coverage builds.
  virtual void SetProfilingFile(base::File file) = 0;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_HOST_H_
