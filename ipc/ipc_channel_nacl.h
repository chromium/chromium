// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_NACL_H_
#define IPC_IPC_CHANNEL_NACL_H_

#include <memory>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/threading/simple_thread.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_reader.h"

namespace IPC {

class MessageAttachment;

// Contains the results from one call to imc_recvmsg (data and file
// descriptors).
struct MessageContents;

// Similar to the ChannelPosix but for Native Client code.
// This is somewhat different because sendmsg/recvmsg here do not follow POSIX
// semantics. Instead, they are implemented by a custom embedding of
// NaClDescCustom. See NaClIPCAdapter for the trusted-side implementation.
//
// We don't need to worry about complicated set up and READWRITE mode for
// sharing handles. We also currently do not support passing file descriptors or
// named pipes, and we use background threads to emulate signaling when we can
// read or write without blocking.
class ChannelNacl : public Channel,
                    public internal::ChannelReader {
 public:
  // Mirror methods of Channel, see ipc_channel.h for description.
  ChannelNacl(const IPC::ChannelHandle& channel_handle,
              Mode mode,
              Listener* listener);
  ~ChannelNacl() override;

  // Channel implementation.
  bool Connect() override;
  void Close() override;
  bool Send(Message* message) override;

  // Posted to the main thread by ReaderThreadRunner.
  void DidRecvMsg(std::unique_ptr<MessageContents> contents);
  void ReadDidFail();

 private:
  class ReaderThreadRunner;

  bool CreatePipe(const IPC::ChannelHandle& channel_handle);
  bool ProcessOutgoingMessages();
  void CallOnChannelConnected();

  // ChannelReader implementation.
  ReadState ReadData(char* buffer,
                     int buffer_len,
                     int* bytes_read) override;
  bool ShouldDispatchInputMessage(Message* msg) override;
  bool GetAttachments(Message* msg) override;
  bool DidEmptyInputBuffers() override;
  void HandleInternalMessage(const Message& msg) override;

  Mode mode_;
  bool waiting_connect_;

  // The pipe used for communication.
  int pipe_;

  // We use a thread for reading, so that we can simply block on reading and
  // post the received data back to the main thread to be properly interleaved
  // with other tasks in the MessagePump.
  //
  // imc_recvmsg supports non-blocking reads, but there's no easy way to be
  // informed when a write or read can be done without blocking (this is handled
  // by libevent in Posix).
  std::unique_ptr<ReaderThreadRunner> reader_thread_runner_;
  std::unique_ptr<base::DelegateSimpleThread> reader_thread_;

  // IPC::ChannelReader expects to be able to call ReadData on us to
  // synchronously read data waiting in the pipe's buffer without blocking.
  // Since we can't do that (see 1 and 2 above), the reader thread does blocking
  // reads and posts the data over to the main thread in MessageContents. Each
  // MessageContents object is the result of one call to "imc_recvmsg".
  // DidRecvMsg breaks the MessageContents out in to the data and the file
  // descriptors, and puts them on these two queues.
  // TODO(dmichael): There's probably a more efficient way to emulate this with
  //                 a circular buffer or something, so we don't have to do so
  //                 many heap allocations. But it maybe isn't worth
  //                 the trouble given that we probably want to implement 1 and
  //                 2 above in NaCl eventually.
  // When ReadData is called, it pulls the bytes out of this queue in order.
  base::circular_deque<std::unique_ptr<std::vector<char>>> read_queue_;
  // Queue of file descriptor attachments extracted from imc_recvmsg messages.
  std::vector<scoped_refptr<MessageAttachment>> input_attachments_;

  // This queue is used when a message is sent prior to Connect having been
  // called. Normally after we're connected, the queue is empty.
  base::circular_deque<std::unique_ptr<Message>> output_queue_;

  base::WeakPtrFactory<ChannelNacl> weak_ptr_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ChannelNacl);
};

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_NACL_H_
