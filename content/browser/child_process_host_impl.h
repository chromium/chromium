// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CHILD_PROCESS_HOST_IMPL_H_
#define CONTENT_BROWSER_CHILD_PROCESS_HOST_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "content/common/child_process.mojom.h"
#include "content/common/content_export.h"
#include "content/public/browser/child_process_host.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/invitation.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/memory/memory_pressure_listener.h"
#endif

namespace IPC {
class Channel;
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
class MessageFilter;
#endif
}  // namespace IPC

namespace content {
class ChildProcessHostDelegate;

// Provides common functionality for hosting a child process and processing IPC
// messages between the host and the child process. Users are responsible
// for the actual launching and terminating of the child processes.
class CONTENT_EXPORT ChildProcessHostImpl : public ChildProcessHost,
                                            public IPC::Listener,
                                            public mojom::ChildProcessHost {
 public:
  ChildProcessHostImpl(const ChildProcessHostImpl&) = delete;
  ChildProcessHostImpl& operator=(const ChildProcessHostImpl&) = delete;

  ~ChildProcessHostImpl() override;

  // Derives a tracing process id from a child process id. Child process ids
  // cannot be used directly in child process for tracing due to security
  // reasons (see: discussion in crrev.com/1173263004). This method is meant to
  // be used when tracing for identifying cross-process shared memory from a
  // process which knows the child process id of its endpoints. The value
  // returned by this method is guaranteed to be equal to the value returned by
  // MemoryDumpManager::GetTracingProcessId() in the corresponding child
  // process.
  //
  // Never returns MemoryDumpManager::kInvalidTracingProcessId.
  // Returns only memory_instrumentation::mojom::kServiceTracingProcessId in
  // single-process mode.
  static uint64_t ChildProcessUniqueIdToTracingProcessId(int child_process_id);

  // ChildProcessHost implementation
  bool Send(IPC::Message* message) override;
  void ForceShutdown() override;
  std::optional<mojo::OutgoingInvitation>& GetMojoInvitation() override;
  void CreateChannelMojo() override;
  bool IsChannelOpening() override;
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  void AddFilter(IPC::MessageFilter* filter) override;
#endif
  void BindReceiver(mojo::GenericPendingReceiver receiver) override;
  void SetBatterySaverMode(bool battery_saver_mode_enabled) override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void ReinitializeLogging(uint32_t logging_dest,
                           base::ScopedFD log_file_descriptor) override;
#endif

  base::Process& GetPeerProcess();
  mojom::ChildProcess* child_process() { return child_process_.get(); }

#if BUILDFLAG(IS_ANDROID)
  // Notifies the child process of memory pressure level.
  void NotifyMemoryPressureToChildProcess(
      base::MemoryPressureListener::MemoryPressureLevel level);
#endif

 private:
  friend class content::ChildProcessHost;

  ChildProcessHostImpl(ChildProcessHostDelegate* delegate, IpcMode ipc_mode);

  // mojom::ChildProcessHost implementation:
  void Ping(PingCallback callback) override;
  void BindHostReceiver(mojo::GenericPendingReceiver receiver) override;

  // IPC::Listener methods:
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;
  void OnBadMessageReceived(const IPC::Message& message) override;

  // Initializes the IPC channel and returns true on success. |channel_| must be
  // non-null.
  bool InitChannel();

  void OnDisconnectedFromChildProcess();

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
  void DumpProfilingData(base::OnceClosure callback) override;
  void SetProfilingFile(base::File file) override;
#endif

  // The outgoing Mojo invitation which must be consumed to bootstrap Mojo IPC
  // to the child process.
  std::optional<mojo::OutgoingInvitation> mojo_invitation_{std::in_place};

  const IpcMode ipc_mode_;
  raw_ptr<ChildProcessHostDelegate> delegate_;
  base::Process peer_process_;
  bool opening_channel_;  // True while we're waiting the channel to be opened.
  std::unique_ptr<IPC::Channel> channel_;
  mojo::Remote<mojom::ChildProcess> child_process_;
  mojo::Receiver<mojom::ChildProcessHost> receiver_{this};

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  // Holds all the IPC message filters.  Since this object lives on the IO
  // thread, we don't have a IPC::ChannelProxy and so we manage filters
  // manually.
  std::vector<scoped_refptr<IPC::MessageFilter>> filters_;
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_CHILD_PROCESS_HOST_IMPL_H_
