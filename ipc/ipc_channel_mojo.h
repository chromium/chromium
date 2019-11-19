// Copyright 2014 The Chromium Authors. All rights reserved.
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
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_factory.h"
#include "ipc/ipc_message_pipe_reader.h"
#include "ipc/ipc_mojo_bootstrap.h"
#include "mojo/public/cpp/bindings/thread_safe_interface_ptr.h"
#include "mojo/public/cpp/system/core.h"

namespace IPC {

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
      const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner,
      const scoped_refptr<mojo::internal::MessageQuotaChecker>& quota_checker);

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

  ~ChannelMojo() override;

  // Channel implementation
  bool Connect() override;
  void Pause() override;
  void Unpause(bool flush) override;
  void Flush() override;
  void Close() override;
  bool Send(Message* message) override;
  Channel::AssociatedInterfaceSupport* GetAssociatedInterfaceSupport() override;

  // These access protected API of IPC::Message, which has ChannelMojo
  // as a friend class.
  static MojoResult WriteToMessageAttachmentSet(
      base::Optional<std::vector<mojo::native::SerializedHandlePtr>> handles,
      Message* message);
  static MojoResult ReadFromMessageAttachmentSet(
      Message* message,
      base::Optional<std::vector<mojo::native::SerializedHandlePtr>>* handles);

  // MessagePipeReader::Delegate
  void OnPeerPidReceived(int32_t peer_pid) override;
  void OnMessageReceived(const Message& message) override;
  void OnBrokenDataReceived() override;
  void OnPipeError() override;
  void OnAssociatedInterfaceRequest(
      const std::string& name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

 private:
  ChannelMojo(
      mojo::ScopedMessagePipeHandle handle,
      Mode mode,
      Listener* listener,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner,
      const scoped_refptr<mojo::internal::MessageQuotaChecker>& quota_checker);

  void ForwardMessageFromThreadSafePtr(mojo::Message message);
  void ForwardMessageWithResponderFromThreadSafePtr(
      mojo::Message message,
      std::unique_ptr<mojo::MessageReceiver> responder);

  // Channel::AssociatedInterfaceSupport:
  std::unique_ptr<mojo::ThreadSafeForwarder<mojom::Channel>>
  CreateThreadSafeChannel() override;
  void AddGenericAssociatedInterface(
      const std::string& name,
      const GenericAssociatedInterfaceFactory& factory) override;
  void GetGenericRemoteAssociatedInterface(
      const std::string& name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  base::WeakPtr<ChannelMojo> weak_ptr_;

  // A TaskRunner which runs tasks on the ChannelMojo's owning thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  const mojo::MessagePipeHandle pipe_;
  std::unique_ptr<MojoBootstrap> bootstrap_;
  Listener* listener_;

  std::unique_ptr<internal::MessagePipeReader> message_reader_;

  base::Lock associated_interface_lock_;
  std::map<std::string, GenericAssociatedInterfaceFactory>
      associated_interfaces_;

  base::WeakPtrFactory<ChannelMojo> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChannelMojo);
};

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_MOJO_H_
