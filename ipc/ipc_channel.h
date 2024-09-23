// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_H_
#define IPC_IPC_CHANNEL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc.mojom-forward.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/generic_pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

#if BUILDFLAG(IS_POSIX)
#include <sys/types.h>
#endif

namespace IPC {

class Listener;
class UrgentMessageObserver;

//------------------------------------------------------------------------------
// See
// http://www.chromium.org/developers/design-documents/inter-process-communication
// for overview of IPC in Chromium.

// Channels are implemented using mojo message pipes on all platforms other
// than NaCl.

class COMPONENT_EXPORT(IPC) Channel : public Sender {
  // Security tests need access to the pipe handle.
  friend class ChannelTest;

 public:
  // Flags to test modes
  using ModeFlags = int;
  static constexpr ModeFlags MODE_NO_FLAG = 0x0;
  static constexpr ModeFlags MODE_SERVER_FLAG = 0x1;
  static constexpr ModeFlags MODE_CLIENT_FLAG = 0x2;

  // Some Standard Modes
  // TODO(morrita): These are under deprecation work. You should use Create*()
  // functions instead.
  enum Mode {
    MODE_NONE = MODE_NO_FLAG,
    MODE_SERVER = MODE_SERVER_FLAG,
    MODE_CLIENT = MODE_CLIENT_FLAG,
  };

  // Messages internal to the IPC implementation are defined here.
  // Uses Maximum value of message type (uint16_t), to avoid conflicting
  // with normal message types, which are enumeration constants starting from 0.
  enum {
    // The Hello message is sent by the peer when the channel is connected.
    // The message contains just the process id (pid).
    // The message has a special routing_id (MSG_ROUTING_NONE)
    // and type (HELLO_MESSAGE_TYPE).
    HELLO_MESSAGE_TYPE = UINT16_MAX,
    // The CLOSE_FD_MESSAGE_TYPE is used in the IPC class to
    // work around a bug in sendmsg() on Mac. When an FD is sent
    // over the socket, a CLOSE_FD_MESSAGE is sent with hops = 2.
    // The client will return the message with hops = 1, *after* it
    // has received the message that contains the FD. When we
    // receive it again on the sender side, we close the FD.
    CLOSE_FD_MESSAGE_TYPE = HELLO_MESSAGE_TYPE - 1
  };

  // Helper interface a Channel may implement to expose support for associated
  // Mojo interfaces.
  class COMPONENT_EXPORT(IPC) AssociatedInterfaceSupport {
   public:
    using GenericAssociatedInterfaceFactory =
        base::RepeatingCallback<void(mojo::ScopedInterfaceEndpointHandle)>;

    virtual ~AssociatedInterfaceSupport() {}

    // Returns a ThreadSafeForwarded for this channel which can be used to
    // safely send mojom::Channel requests from arbitrary threads.
    virtual std::unique_ptr<mojo::ThreadSafeForwarder<mojom::Channel>>
    CreateThreadSafeChannel() = 0;

    // Adds an interface factory to this channel for interface |name|. Must be
    // safe to call from any thread.
    virtual void AddGenericAssociatedInterface(
        const std::string& name,
        const GenericAssociatedInterfaceFactory& factory) = 0;

    // Requests an associated interface from the remote endpoint.
    virtual void GetRemoteAssociatedInterface(
        mojo::GenericPendingAssociatedReceiver receiver) = 0;
  };

  // The maximum message size in bytes. Attempting to receive a message of this
  // size or bigger results in a channel error.
  static constexpr size_t kMaximumMessageSize = 128 * 1024 * 1024;

  // Amount of data to read at once from the pipe.
  static const size_t kReadBufferSize = 4 * 1024;

  // Maximum persistent read buffer size. Read buffer can grow larger to
  // accommodate large messages, but it's recommended to shrink back to this
  // value because it fits 99.9% of all messages (see issue 529940 for data).
  static const size_t kMaximumReadBufferSize = 64 * 1024;

  // Initialize a Channel.
  //
  // |channel_handle| identifies the communication Channel. For POSIX, if
  // the file descriptor in the channel handle is != -1, the channel takes
  // ownership of the file descriptor and will close it appropriately, otherwise
  // it will create a new descriptor internally.
  // |listener| receives a callback on the current thread for each newly
  // received message.
  //
  // There are four type of modes how channels operate:
  //
  // - Server and named server: In these modes, the Channel is
  //   responsible for setting up the IPC object.
  // - An "open" named server: It accepts connections from ANY client.
  //   The caller must then implement their own access-control based on the
  //   client process' user Id.
  // - Client and named client: In these mode, the Channel merely
  //   connects to the already established IPC object.
  //
  // Each mode has its own Create*() API to create the Channel object.
  static std::unique_ptr<Channel> Create(
      const IPC::ChannelHandle& channel_handle,
      Mode mode,
      Listener* listener);

  static std::unique_ptr<Channel> CreateClient(
      const IPC::ChannelHandle& channel_handle,
      Listener* listener,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner);

  static std::unique_ptr<Channel> CreateServer(
      const IPC::ChannelHandle& channel_handle,
      Listener* listener,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner);

  ~Channel() override;

  // Connect the pipe.  On the server side, this will initiate
  // waiting for connections.  On the client, it attempts to
  // connect to a pre-existing pipe.  Note, calling Connect()
  // will not block the calling thread and may complete
  // asynchronously.
  //
  // The subclass implementation must call WillConnect() at the beginning of its
  // implementation.
  [[nodiscard]] virtual bool Connect() = 0;

  // Pause the channel. Subsequent sends will be queued internally until
  // Unpause() is called and the channel is flushed either by Unpause() or a
  // subsequent call to Flush().
  virtual void Pause();

  // Unpause the channel. This allows subsequent Send() calls to transmit
  // messages immediately, without queueing. If |flush| is true, any messages
  // queued while paused will be flushed immediately upon unpausing. Otherwise
  // you must call Flush() explicitly.
  //
  // Not all implementations support Unpause(). See ConnectPaused() above for
  // details.
  virtual void Unpause(bool flush);

  // Manually flush the pipe. This is only useful exactly once, and only after
  // a call to Unpause(false), in order to explicitly flush out any
  // messages which were queued prior to unpausing.
  //
  // Not all implementations support Flush(). See ConnectPaused() above for
  // details.
  virtual void Flush();

  // Close this Channel explicitly.  May be called multiple times.
  // On POSIX calling close on an IPC channel that listens for connections will
  // cause it to close any accepted connections, and it will stop listening for
  // new connections. If you just want to close the currently accepted
  // connection and listen for new ones, use ResetToAcceptingConnectionState.
  virtual void Close() = 0;

  // Gets a helper for associating Mojo interfaces with this Channel.
  //
  // NOTE: Not all implementations support this.
  virtual AssociatedInterfaceSupport* GetAssociatedInterfaceSupport();

  // Overridden from ipc::Sender.
  // Send a message over the Channel to the listener on the other end.
  //
  // |message| must be allocated using operator new.  This object will be
  // deleted once the contents of the Message have been sent.
  bool Send(Message* message) override = 0;

  // Sets the UrgentMessageObserver for this channel. `observer` must outlive
  // the channel.
  //
  // Only channel associated mojo interfaces support urgent messages.
  virtual void SetUrgentMessageObserver(UrgentMessageObserver* observer);

#if !BUILDFLAG(IS_NACL)
  // Generates a channel ID that's non-predictable and unique.
  static std::string GenerateUniqueRandomChannelID();
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Sandboxed processes live in a PID namespace, so when sending the IPC hello
  // message from client to server we need to send the PID from the global
  // PID namespace.
  static void SetGlobalPid(int pid);
  static int GetGlobalPid();
#endif

 protected:
  // Subclasses must call this method at the beginning of their implementation
  // of Connect().
  void WillConnect();

 private:
  bool did_start_connect_ = false;
};

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_H_
