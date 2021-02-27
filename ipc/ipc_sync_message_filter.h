// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_SYNC_MESSAGE_FILTER_H_
#define IPC_IPC_SYNC_MESSAGE_FILTER_H_

#include <set>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_sync_message.h"
#include "ipc/message_filter.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace base {
class SingleThreadTaskRunner;
class WaitableEvent;
}

namespace IPC {
class SyncChannel;

// This MessageFilter allows sending synchronous IPC messages from a thread
// other than the listener thread associated with the SyncChannel.  It does not
// support fancy features that SyncChannel does, such as handling recursion or
// receiving messages while waiting for a response.  Note that this object can
// be used to send simultaneous synchronous messages from different threads.
class COMPONENT_EXPORT(IPC) SyncMessageFilter : public MessageFilter,
                                                public Sender {
 public:
  // Sender implementation.
  bool Send(Message* message) override;

  // MessageFilter implementation.
  void OnFilterAdded(Channel* channel) override;
  void OnChannelError() override;
  void OnChannelClosing() override;
  bool OnMessageReceived(const Message& message) override;

  // Binds an associated interface proxy to an interface in the browser process.
  // Interfaces acquired through this method are associated with the IPC Channel
  // and as such retain FIFO with legacy IPC messages.
  //
  // NOTE: This must ONLY be called on the Channel's thread, after
  // OnFilterAdded.
  template <typename Interface>
  void GetRemoteAssociatedInterface(
      mojo::PendingAssociatedRemote<Interface>* proxy) {
    auto receiver = proxy->InitWithNewEndpointAndPassReceiver();
    GetGenericRemoteAssociatedInterface(Interface::Name_,
                                        receiver.PassHandle());
  }

 protected:
  explicit SyncMessageFilter(base::WaitableEvent* shutdown_event);
  ~SyncMessageFilter() override;

 private:
  friend class SyncChannel;

  void SendOnIOThread(Message* message);
  // Signal all the pending sends as done, used in an error condition.
  void SignalAllEvents();

  // NOTE: This must ONLY be called on the Channel's thread.
  void GetGenericRemoteAssociatedInterface(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle);

  // The channel to which this filter was added.
  Channel* channel_;

  // The process's main thread.
  scoped_refptr<base::SingleThreadTaskRunner> listener_task_runner_;

  // The message loop where the Channel lives.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  typedef std::set<PendingSyncMsg*> PendingSyncMessages;
  PendingSyncMessages pending_sync_messages_;

  // Messages waiting to be delivered after IO initialization.
  std::vector<std::unique_ptr<Message>> pending_messages_;

  // Locks data members above.
  base::Lock lock_;

  base::WaitableEvent* const shutdown_event_;

  DISALLOW_COPY_AND_ASSIGN(SyncMessageFilter);
};

}  // namespace IPC

#endif  // IPC_IPC_SYNC_MESSAGE_FILTER_H_
