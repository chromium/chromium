// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MOJO_BOOTSTRAP_H_
#define IPC_IPC_MOJO_BOOTSTRAP_H_

#include <stdint.h>

#include <memory>

#include "base/auto_reset.h"
#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace IPC {

class UrgentMessageObserver;

// Incoming legacy IPCs have always been dispatched to one of two threads: the
// IO thread (when an installed MessageFilter handles the message), or the
// thread which owns the corresponding ChannelProxy receiving the message. There
// were no other places to route legacy IPC messages, so when a message arrived
// the legacy IPC system would run through its MessageFilters and if the message
// was still unhandled, it would be posted to the ChannelProxy thread for
// further processing.
//
// Mojo on the other hand allows for mutually associated endpoints (that is,
// endpoints which locally share the same message pipe) to span any number of
// threads while still guaranteeing that each endpoint on a given thread
// preserves FIFO order of messages dispatched there. This means that if a
// message arrives carrying a PendingAssociatedRemote/Receiver endpoint, and
// then another message arrives which targets that endpoint, the entire pipe
// will be blocked from dispatch until the endpoint is bound: otherwise we have
// no idea where to dispatch the message such that we can uphold the FIFO
// guarantee between the new endpoint and any other endpoints on the thread it
// ends up binding to.
//
// Channel-associated interfaces share a message pipe with the legacy IPC
// Channel, and in order to avoid nasty surprises during the migration process
// we decided to constrain how incoming Channel-associated endpoints could be
// bound: you must either bind them immediately as they arrive on the IO thread,
// or you must immediately post a task to the ChannelProxy thread to bind them.
// This allows all aforementioned FIFO guaratees to be upheld without ever
// stalling dispatch of legacy IPCs (particularly on the IO thread), because
// when we see a message targeting an unbound endpoint we can safely post it to
// the ChannelProxy's task runner before forging ahead to dispatch subsequent
// messages. No stalling.
//
// As there are some cases where a Channel-associated endpoint really wants to
// receive messages on a different TaskRunner, we want to allow that now. It's
// safe as long as the application can guarantee that the endpoint in question
// will be bound to a task runner *before* any messages are received for that
// endpoint.
//
// HOWEVER, it turns out that we cannot simply adhere to the application's
// wishes when an alternative TaskRunner is provided at binding time: over time
// we have accumulated application code which binds Channel-associated endpoints
// to task runners which -- while running tasks exclusively on the ChannelProxy
// thread -- are not the ChannelProxy's own task runner. Such code now
// implicitly relies on the behavior of Channel-associated interfaces always
// dispatching their messages to the ChannelProxy task runner. This is tracked
// by https://crbug.com/1209188.
//
// Finally, the point: if you really know you want to bind your endpoint to an
// alternative task runner and you can really guarantee that no messages may
// have already arrived for it on the IO thread, you can do the binding within
// the extent of a ScopedAllowOffSequenceChannelAssociatedBindings. This will
// flag the endpoint such that it honors your binding configuration, and its
// incoming messages will actually dispatch to the task runner you provide.
class COMPONENT_EXPORT(IPC)
    [[maybe_unused,
      nodiscard]] ScopedAllowOffSequenceChannelAssociatedBindings {
 public:
  ScopedAllowOffSequenceChannelAssociatedBindings();
  ~ScopedAllowOffSequenceChannelAssociatedBindings();

 private:
  const base::AutoReset<bool> resetter_;
};

// MojoBootstrap establishes a pair of associated interfaces between two
// processes in Chrome.
//
// Clients should implement MojoBootstrap::Delegate to get the associated pipes
// from MojoBootstrap object.
//
// This lives on IO thread other than Create(), which can be called from
// UI thread as Channel::Create() can be.
class COMPONENT_EXPORT(IPC) MojoBootstrap {
 public:
  virtual ~MojoBootstrap() {}

  // Create the MojoBootstrap instance, using |handle| as the message pipe, in
  // mode as specified by |mode|. The result is passed to |delegate|.
  static std::unique_ptr<MojoBootstrap> Create(
      mojo::ScopedMessagePipeHandle handle,
      Channel::Mode mode,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner);

  // Initialize the Channel pipe and interface endpoints. This performs all
  // setup except actually starting to read messages off the pipe.
  virtual void Connect(
      mojo::PendingAssociatedRemote<mojom::Channel>* sender,
      mojo::PendingAssociatedReceiver<mojom::Channel>* receiver) = 0;

  // Enable incoming messages to start being read off the pipe and routed to
  // endpoints. Must not be called until the pending endpoints created by
  // Connect() are actually bound somewhere.
  virtual void StartReceiving() = 0;

  // Stop transmitting messages and start queueing them instead.
  virtual void Pause() = 0;

  // Stop queuing new messages and start transmitting them instead.
  virtual void Unpause() = 0;

  // Flush outgoing messages which were queued before Start().
  virtual void Flush() = 0;

  virtual mojo::AssociatedGroup* GetAssociatedGroup() = 0;

  virtual void SetUrgentMessageObserver(UrgentMessageObserver* observer) = 0;
};

}  // namespace IPC

#endif  // IPC_IPC_MOJO_BOOTSTRAP_H_
