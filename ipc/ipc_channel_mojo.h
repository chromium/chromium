// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_MOJO_H_
#define IPC_IPC_CHANNEL_MOJO_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_factory.h"
#include "ipc/ipc_message_pipe_reader.h"
#include "ipc/ipc_mojo_bootstrap.h"

namespace IPC {

class UrgentMessageObserver;

// Mojo-based IPC::Channel implementation over a Mojo message pipe.
//
// ChannelMojo builds a Mojo MessagePipe using the provided message pipe
// |handle| and builds an associated interface for each direction on the
// channel.
//
// TODO(morrita): Add APIs to create extra MessagePipes to let
//                Mojo-based objects talk over this Channel.
//
class COMPONENT_EXPORT(IPC) ChannelMojo
    : public Channel,
      public Channel::AssociatedInterfaceSupport,
      public internal::MessagePipeReader::Delegate {
 public:
  // Creates a ChannelMojo.
  static std::unique_ptr<ChannelMojo> Create(
      mojo::ScopedMessagePipeHandle handle,
      Mode mode,
      Listener* listener,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner);

  // Create a factory object for ChannelMojo.
  // The factory is used to create Mojo-based ChannelProxy family.
  // |host| must not be null.
  static std::unique_ptr<ChannelFactory> CreateServerFactory(
      mojo::ScopedMessagePipeHandle handle,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner);

  static std::unique_ptr<ChannelFactory> CreateClientFactory(
      mojo::ScopedMessagePipeHandle handle,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner);

  ChannelMojo(const ChannelMojo&) = delete;
  ChannelMojo& operator=(const ChannelMojo&) = delete;

  ~ChannelMojo() override;

  // Channel implementation
  bool Connect() override;
  void Pause() override;
  void Unpause(bool flush) override;
  void Flush() override;
  void Close() override;
  bool Send(Message* message) override;
  Channel::AssociatedInterfaceSupport* GetAssociatedInterfaceSupport() override;
  void SetUrgentMessageObserver(UrgentMessageObserver* observer) override;

  // These access protected API of IPC::Message, which has ChannelMojo
  // as a friend class.
  static MojoResult WriteToMessageAttachmentSet(
      std::optional<std::vector<mojo::native::SerializedHandlePtr>> handles,
      Message* message);
  static MojoResult ReadFromMessageAttachmentSet(
      Message* message,
      std::optional<std::vector<mojo::native::SerializedHandlePtr>>* handles);

  // MessagePipeReader::Delegate
  void OnPeerPidReceived(int32_t peer_pid) override;
  void OnMessageReceived(const Message& message) override;
  void OnBrokenDataReceived() override;
  void OnPipeError() override;
  void OnAssociatedInterfaceRequest(
      mojo::GenericPendingAssociatedReceiver receiver) override;

 private:
  ChannelMojo(
      mojo::ScopedMessagePipeHandle handle,
      Mode mode,
      Listener* listener,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner);

  void ForwardMessage(mojo::Message message);

  // Channel::AssociatedInterfaceSupport:
  std::unique_ptr<mojo::ThreadSafeForwarder<mojom::Channel>>
  CreateThreadSafeChannel() override;
  void AddGenericAssociatedInterface(
      const std::string& name,
      const GenericAssociatedInterfaceFactory& factory) override;
  void GetRemoteAssociatedInterface(
      mojo::GenericPendingAssociatedReceiver receiver) override;

  void FinishConnectOnIOThread();

  base::WeakPtr<ChannelMojo> weak_ptr_;

  // A TaskRunner which runs tasks on the ChannelMojo's owning thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  const mojo::MessagePipeHandle pipe_;
  std::unique_ptr<MojoBootstrap> bootstrap_;
  raw_ptr<Listener, DanglingUntriaged> listener_;

  std::unique_ptr<internal::MessagePipeReader> message_reader_;

  base::Lock associated_interface_lock_;
  std::map<std::string, GenericAssociatedInterfaceFactory>
      associated_interfaces_;

  base::WeakPtrFactory<ChannelMojo> weak_factory_{this};
};

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_MOJO_H_
