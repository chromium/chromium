// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_PROXY_H_
#define IPC_IPC_CHANNEL_PROXY_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/generic_pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/bindings/shared_associated_remote.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace IPC {

class ChannelFactory;
class UrgentMessageObserver;

//-----------------------------------------------------------------------------
// IPC::ChannelProxy
//
// This class is a helper class that is useful when you wish to run an IPC
// channel on a background thread.  It provides you with the option of either
// handling IPC messages on that background thread or having them dispatched to
// your main thread (the thread on which the IPC::ChannelProxy is created).
//
// The API for an IPC::ChannelProxy is very similar to that of an IPC::Channel.
// When you send a message to an IPC::ChannelProxy, the message is routed to
// the background thread, where it is then passed to the IPC::Channel's Send
// method.  This means that you can send a message from your thread and your
// message will be sent over the IPC channel when possible instead of being
// delayed until your thread returns to its message loop.  (Often IPC messages
// will queue up on the IPC::Channel when there is a lot of traffic, and the
// channel will not get cycles to flush its message queue until the thread, on
// which it is running, returns to its message loop.)
//
// The consumer of IPC::ChannelProxy is responsible for allocating the Thread
// instance where the IPC::Channel will be created and operated.
//
// Thread-safe send
//
// If a particular |Channel| implementation has a thread-safe |Send()| operation
// then ChannelProxy skips the inter-thread hop and calls |Send()| directly. In
// this case the |channel_| variable is touched by multiple threads so
// |channel_lifetime_lock_| is used to protect it. The locking overhead is only
// paid if the underlying channel supports thread-safe |Send|.
//
class COMPONENT_EXPORT(IPC) ChannelProxy {
 public:
  // Initializes a channel proxy.  The channel_handle and mode parameters are
  // passed directly to the underlying IPC::Channel.  The listener is called on
  // the thread that creates the ChannelProxy.  The filter's OnMessageReceived
  // method is called on the thread where the IPC::Channel is running.  The
  // filter may be null if the consumer is not interested in handling messages
  // on the background thread.  Any message not handled by the filter will be
  // dispatched to the listener.  The given task runner correspond to a thread
  // on which IPC::Channel is created and used (e.g. IO thread).
  static std::unique_ptr<ChannelProxy> Create(
      const mojo::MessagePipeHandle& channel_handle,
      Channel::Mode mode,
      Listener* listener,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& listener_task_runner);

  static std::unique_ptr<ChannelProxy> Create(
      std::unique_ptr<ChannelFactory> factory,
      Listener* listener,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& listener_task_runner);

  // Constructs a ChannelProxy without initializing it.
  ChannelProxy(
      Listener* listener,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& listener_task_runner);

  virtual ~ChannelProxy();

  // Initializes the channel proxy. Only call this once to initialize a channel
  // proxy that was not initialized in its constructor. If |create_pipe_now| is
  // true, the pipe is created synchronously. Otherwise it's created on the IO
  // thread.
  void Init(const mojo::MessagePipeHandle& channel_handle,
            Channel::Mode mode,
            bool create_pipe_now);
  void Init(std::unique_ptr<ChannelFactory> factory,
            bool create_pipe_now);

  // Pause the channel. Subsequent calls to Send() will be internally queued
  // until Unpause() is called. Queued messages will not be sent until the
  // channel is flushed.
  void Pause();

  // Unpause the channel. If |flush| is true the channel will be flushed as soon
  // as it's unpaused (see Flush() below.) Otherwise you must explicitly call
  // Flush() to flush messages which were queued while the channel was paused.
  void Unpause(bool flush);

  // Flush the channel. This sends any messages which were queued before calling
  // Connect. Only useful if Unpause(false) was called previously.
  void Flush();

  // Close the IPC::Channel.  This operation completes asynchronously, once the
  // background thread processes the command to close the channel.  It is ok to
  // call this method multiple times.  Redundant calls are ignored.
  void Close();

  // Set the `UrgentMessageObserver` for the channel. Must be called on the
  // proxy thread before initialization.
  void SetUrgentMessageObserver(UrgentMessageObserver* observer);

  using GenericAssociatedInterfaceFactory =
      base::RepeatingCallback<void(mojo::ScopedInterfaceEndpointHandle)>;

  // Adds a generic associated interface factory to bind incoming interface
  // requests directly on the IO thread. MUST be called either before Init() or
  // before the remote end of the Channel is able to send messages (e.g. before
  // its process is launched.)
  void AddGenericAssociatedInterfaceForIOThread(
      const std::string& name,
      const GenericAssociatedInterfaceFactory& factory);

  template <typename Interface>
  using AssociatedInterfaceFactory =
      base::RepeatingCallback<void(mojo::PendingAssociatedReceiver<Interface>)>;

  // Helper to bind an IO-thread associated interface factory, inferring the
  // interface name from the callback argument's type. MUST be called before
  // Init().
  template <typename Interface>
  void AddAssociatedInterfaceForIOThread(
      const AssociatedInterfaceFactory<Interface>& factory) {
    AddGenericAssociatedInterfaceForIOThread(
        Interface::Name_,
        base::BindRepeating(
            &ChannelProxy::BindPendingAssociatedReceiver<Interface>, factory));
  }

  // Requests an associated interface from the remote endpoint.
  void GetRemoteAssociatedInterface(
      mojo::GenericPendingAssociatedReceiver receiver);

  // Template helper to receive associated interfaces from the remote endpoint.
  template <typename Interface>
  void GetRemoteAssociatedInterface(mojo::AssociatedRemote<Interface>* proxy) {
    GetRemoteAssociatedInterface(proxy->BindNewEndpointAndPassReceiver());
  }

  // Called to clear the pointer to the IPC task runner when it's going away.
  void ClearIPCTaskRunner();

 protected:
  class Context;
  // A subclass uses this constructor if it needs to add more information
  // to the internal state.
  explicit ChannelProxy(Context* context);

  // Used internally to hold state that is referenced on the IPC thread.
  class Context : public base::RefCountedThreadSafe<Context>,
                  public Listener {
   public:
    Context(Listener* listener,
            const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
            const scoped_refptr<base::SingleThreadTaskRunner>&
                listener_task_runner);
    void ClearIPCTaskRunner();
    base::SingleThreadTaskRunner* ipc_task_runner() const {
      return ipc_task_runner_.get();
    }

    scoped_refptr<base::SingleThreadTaskRunner> listener_task_runner() {
      return default_listener_task_runner_;
    }

   protected:
    friend class base::RefCountedThreadSafe<Context>;
    ~Context() override;

    // IPC::Listener methods:
    void OnChannelConnected(int32_t peer_pid) override;
    void OnChannelError() override;
    void OnAssociatedInterfaceRequest(
        const std::string& interface_name,
        mojo::ScopedInterfaceEndpointHandle handle) override;

    void PauseChannel();
    void UnpauseChannel(bool flush);
    void FlushChannel();

    // Like Open and Close, but called on the IPC thread.
    virtual void OnChannelOpened();
    virtual void OnChannelClosed();

    // Called on the consumers thread when the ChannelProxy is closed.  At that
    // point the consumer is telling us that they don't want to receive any
    // more messages, so we honor that wish by forgetting them!
    virtual void Clear();

   private:
    friend class ChannelProxy;

    // Create the Channel
    void CreateChannel(std::unique_ptr<ChannelFactory> factory);

    // Methods called on the listener thread.
    void OnDispatchConnected();
    void OnDispatchError();
    void OnDispatchAssociatedInterfaceRequest(
        const std::string& interface_name,
        mojo::ScopedInterfaceEndpointHandle handle);
    void SetUrgentMessageObserver(UrgentMessageObserver* observer);

    void ClearChannel();

    mojom::Channel& thread_safe_channel() {
      return thread_safe_channel_->proxy();
    }

    void AddGenericAssociatedInterfaceForIOThread(
        const std::string& name,
        const GenericAssociatedInterfaceFactory& factory);

    scoped_refptr<base::SingleThreadTaskRunner> default_listener_task_runner_;
    raw_ptr<Listener> listener_;

    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;

    // Note, channel_ may be set on the Listener thread or the IPC thread.
    // But once it has been set, it must only be read or cleared on the IPC
    // thread.
    // One exception is the thread-safe send. See the class comment.
    std::unique_ptr<Channel> channel_;
    bool channel_connected_called_;

    // Lock for |channel_| value. This is only relevant in the context of
    // thread-safe send.
    base::Lock channel_lifetime_lock_;

    // Lock for pending_io_thread_interfaces_ (formerly for pending_filters_)
    base::Lock pending_filters_lock_;

    // Cached copy of the peer process ID. Set on IPC but read on both IPC and
    // listener threads.
    base::ProcessId peer_pid_;
    base::Lock peer_pid_lock_;

    // A thread-safe mojom::Channel interface we use to make remote interface
    // requests from the proxy thread.
    std::unique_ptr<mojo::ThreadSafeForwarder<mojom::Channel>>
        thread_safe_channel_;

    // Holds associated interface binders added by
    // AddGenericAssociatedInterfaceForIOThread until the underlying channel has
    // been initialized.
    base::Lock pending_io_thread_interfaces_lock_;
    std::vector<std::pair<std::string, GenericAssociatedInterfaceFactory>>
        pending_io_thread_interfaces_;
    raw_ptr<UrgentMessageObserver> urgent_message_observer_ = nullptr;
  };

  Context* context() { return context_.get(); }

  bool did_init() const { return did_init_; }

 private:
  template <typename Interface>
  static void BindPendingAssociatedReceiver(
      const AssociatedInterfaceFactory<Interface>& factory,
      mojo::ScopedInterfaceEndpointHandle handle) {
    factory.Run(mojo::PendingAssociatedReceiver<Interface>(std::move(handle)));
  }

  // By maintaining this indirection (ref-counted) to our internal state, we
  // can safely be destroyed while the background thread continues to do stuff
  // that involves this data.
  scoped_refptr<Context> context_;

  // Whether the channel has been initialized.
  bool did_init_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_PROXY_H_
