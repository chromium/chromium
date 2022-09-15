// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PROXY_CHANNEL_H_
#define PPAPI_PROXY_PROXY_CHANNEL_H_

#include <memory>

#include "base/files/scoped_file.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_sync_channel.h"
#include "ppapi/proxy/ppapi_proxy_export.h"

namespace base {
class SingleThreadTaskRunner;
class WaitableEvent;
}

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT ProxyChannel
    : public IPC::Listener,
      public IPC::Sender {
 public:
  class PPAPI_PROXY_EXPORT Delegate {
   public:
    virtual ~Delegate() {}

    // Returns the task runner for processing IPC requests.
    virtual base::SingleThreadTaskRunner* GetIPCTaskRunner() = 0;

    // Returns the event object that becomes signalled when the main thread's
    // message loop exits.
    virtual base::WaitableEvent* GetShutdownEvent() = 0;

    // Duplicates a handle to the provided object, returning one that is valid
    // on the other side of the channel. This is part of the delegate interface
    // because both sides of the channel may not have sufficient permission to
    // duplicate handles directly. The implementation must provide the same
    // guarantees as ProxyChannel::ShareHandleWithRemote below.
    virtual IPC::PlatformFileForTransit ShareHandleWithRemote(
        base::PlatformFile handle,
        base::ProcessId remote_pid,
        bool should_close_source) = 0;

    // Duplicates a shared memory handle, returning one that is valid on the
    // other side of the channel. This is part of the delegate interface
    // because both sides of the channel may not have sufficient permission to
    // duplicate handles directly. The implementation must provide the same
    // guarantees as ProxyChannel::ShareSharedMemoryHandleWithRemote below.
    virtual base::UnsafeSharedMemoryRegion
    ShareUnsafeSharedMemoryRegionWithRemote(
        const base::UnsafeSharedMemoryRegion& region,
        base::ProcessId remote_pid) = 0;
    virtual base::ReadOnlySharedMemoryRegion
    ShareReadOnlySharedMemoryRegionWithRemote(
        const base::ReadOnlySharedMemoryRegion& region,
        base::ProcessId remote_pid) = 0;
  };

  ProxyChannel(const ProxyChannel&) = delete;
  ProxyChannel& operator=(const ProxyChannel&) = delete;

  ~ProxyChannel() override;

  // Alternative to InitWithChannel() for unit tests that want to send all
  // messages sent via this channel to the given test sink. The test sink
  // must outlive this class. In this case, the peer PID will be the current
  // process ID.
  void InitWithTestSink(IPC::Sender* sender);

  // Shares a file handle (HANDLE / file descriptor) with the remote side. It
  // returns a handle that should be sent in exactly one IPC message. Upon
  // receipt, the remote side then owns that handle. Note: if sending the
  // message fails, the returned handle is properly closed by the IPC system. If
  // should_close_source is set to true, the original handle is closed by this
  // operation and should not be used again.
  IPC::PlatformFileForTransit ShareHandleWithRemote(
      base::PlatformFile handle,
      bool should_close_source);

  // Shares a shared memory handle with the remote side. It returns a handle
  // that should be sent in exactly one IPC message. Upon receipt, the remote
  // side then owns that handle. Note: if sending the message fails, the
  // returned handle is properly closed by the IPC system. The original handle
  // is not closed by this operation.
  base::UnsafeSharedMemoryRegion ShareUnsafeSharedMemoryRegionWithRemote(
      const base::UnsafeSharedMemoryRegion& region);
  base::ReadOnlySharedMemoryRegion ShareReadOnlySharedMemoryRegionWithRemote(
      const base::ReadOnlySharedMemoryRegion& region);

  // IPC::Sender implementation.
  bool Send(IPC::Message* msg) override;

  // IPC::Listener implementation.
  void OnChannelError() override;

  // Will be NULL in some unit tests and if the remote side has crashed.
  IPC::SyncChannel* channel() const {
    return channel_.get();
  }

  base::ProcessId peer_pid() { return peer_pid_; }

 protected:
  explicit ProxyChannel();

  // You must call this function before anything else. Returns true on success.
  // The delegate pointer must outlive this class, ownership is not
  // transferred.
  virtual bool InitWithChannel(
      Delegate* delegate,
      base::ProcessId peer_pid,
      const IPC::ChannelHandle& channel_handle,
      bool is_client,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  ProxyChannel::Delegate* delegate() const {
    return delegate_;
  }

 private:
  // Non-owning pointer. Guaranteed non-NULL after init is called.
  ProxyChannel::Delegate* delegate_;

  // PID of the remote process. Use this instead of the Channel::peer_pid since
  // this is set synchronously on construction rather than waiting on the
  // "hello" message from the peer (which introduces a race condition).
  base::ProcessId peer_pid_;

  // When we're unit testing, this will indicate the sink for the messages to
  // be deposited so they can be inspected by the test. When non-NULL, this
  // indicates that the channel should not be used.
  IPC::Sender* test_sink_;

  // Will be null for some tests when there is a test_sink_, and if the
  // remote side has crashed.
  std::unique_ptr<IPC::SyncChannel> channel_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PROXY_CHANNEL_H_
