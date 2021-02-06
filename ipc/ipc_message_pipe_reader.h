// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MESSAGE_PIPE_READER_H_
#define IPC_IPC_MESSAGE_PIPE_READER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/atomicops.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/threading/thread_checker.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace IPC {
namespace internal {

// A helper class to handle bytestream directly over mojo::MessagePipe
// in template-method pattern. MessagePipeReader manages the lifetime
// of given MessagePipe and participates the event loop, and
// read the stream and call the client when it is ready.
//
// Each client has to:
//
//  * Provide a subclass implemenation of a specific use of a MessagePipe
//    and implement callbacks.
//  * Create the subclass instance with a MessagePipeHandle.
//    The constructor automatically start listening on the pipe.
//
// All functions must be called on the IO thread, except for Send(), which can
// be called on any thread. All |Delegate| functions will be called on the IO
// thread.
//
class COMPONENT_EXPORT(IPC) MessagePipeReader : public mojom::Channel {
 public:
  class Delegate {
   public:
    virtual void OnPeerPidReceived(int32_t peer_pid) = 0;
    virtual void OnMessageReceived(const Message& message) = 0;
    virtual void OnBrokenDataReceived() = 0;
    virtual void OnPipeError() = 0;
    virtual void OnAssociatedInterfaceRequest(
        const std::string& name,
        mojo::ScopedInterfaceEndpointHandle handle) = 0;
  };

  // Builds a reader that reads messages from |receive_handle| and lets
  // |delegate| know.
  //
  // |pipe| is the message pipe handle corresponding to the channel's primary
  // interface. This is the message pipe underlying both |sender| and
  // |receiver|.
  //
  // Both |sender| and |receiver| must be non-null.
  //
  // Note that MessagePipeReader doesn't delete |delegate|.
  MessagePipeReader(mojo::MessagePipeHandle pipe,
                    mojo::AssociatedRemote<mojom::Channel> sender,
                    mojo::PendingAssociatedReceiver<mojom::Channel> receiver,
                    Delegate* delegate);
  ~MessagePipeReader() override;

  // Close and destroy the MessagePipe.
  void Close();

  // Return true if the MessagePipe is alive.
  bool IsValid() { return sender_.is_bound(); }

  // Sends an IPC::Message to the other end of the pipe. Safe to call from any
  // thread.
  bool Send(std::unique_ptr<Message> message);

  // Requests an associated interface from the other end of the pipe.
  void GetRemoteInterface(const std::string& name,
                          mojo::ScopedInterfaceEndpointHandle handle);

  mojo::AssociatedRemote<mojom::Channel>& sender() { return sender_; }

 protected:
  void OnPipeClosed();
  void OnPipeError(MojoResult error);

 private:
  // mojom::Channel:
  void SetPeerPid(int32_t peer_pid) override;
  void Receive(MessageView message_view) override;
  void GetAssociatedInterface(
      const std::string& name,
      mojo::PendingAssociatedReceiver<mojom::GenericInterface> receiver)
      override;

  // |delegate_| is null once the message pipe is closed.
  Delegate* delegate_;
  mojo::AssociatedRemote<mojom::Channel> sender_;
  mojo::AssociatedReceiver<mojom::Channel> receiver_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MessagePipeReader);
};

}  // namespace internal
}  // namespace IPC

#endif  // IPC_IPC_MESSAGE_PIPE_READER_H_
