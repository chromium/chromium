// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MOJO_BOOTSTRAP_H_
#define IPC_IPC_MOJO_BOOTSTRAP_H_

#include <stdint.h>

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/lib/message_quota_checker.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace IPC {

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
      const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner,
      const scoped_refptr<mojo::internal::MessageQuotaChecker>& quota_checker);

  // Start the handshake over the underlying message pipe.
  virtual void Connect(
      mojo::AssociatedRemote<mojom::Channel>* sender,
      mojo::PendingAssociatedReceiver<mojom::Channel>* receiver) = 0;

  // Stop transmitting messages and start queueing them instead.
  virtual void Pause() = 0;

  // Stop queuing new messages and start transmitting them instead.
  virtual void Unpause() = 0;

  // Flush outgoing messages which were queued before Start().
  virtual void Flush() = 0;

  virtual mojo::AssociatedGroup* GetAssociatedGroup() = 0;
};

}  // namespace IPC

#endif  // IPC_IPC_MOJO_BOOTSTRAP_H_
