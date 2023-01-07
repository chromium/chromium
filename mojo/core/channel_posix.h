// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_CHANNEL_POSIX_H_
#define MOJO_CORE_CHANNEL_POSIX_H_

#include "mojo/core/channel.h"

#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/synchronization/lock.h"
#include "base/task/current_thread.h"
#include "base/task/task_runner.h"
#include "build/build_config.h"
#include "mojo/core/core.h"

namespace mojo {
namespace core {

class MessageView;

class ChannelPosix : public Channel,
                     public base::CurrentThread::DestructionObserver,
                     public base::MessagePumpForIO::FdWatcher {
 public:
  ChannelPosix(Delegate* delegate,
               ConnectionParams connection_params,
               HandlePolicy handle_policy,
               scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  ChannelPosix(const ChannelPosix&) = delete;
  ChannelPosix& operator=(const ChannelPosix&) = delete;

  void Start() override;
  void ShutDownImpl() override;
  void Write(MessagePtr message) override;
  void LeakHandle() override;
  bool GetReadPlatformHandles(const void* payload,
                              size_t payload_size,
                              size_t num_handles,
                              const void* extra_header,
                              size_t extra_header_size,
                              std::vector<PlatformHandle>* handles,
                              bool* deferred) override;
  bool GetReadPlatformHandlesForIpcz(
      size_t num_handles,
      std::vector<PlatformHandle>& handles) override;
  bool OnControlMessage(Message::MessageType message_type,
                        const void* payload,
                        size_t payload_size,
                        std::vector<PlatformHandle> handles) override;

 protected:
  ~ChannelPosix() override;
  virtual void StartOnIOThread();
  virtual void ShutDownOnIOThread();
  virtual void OnWriteError(Error error);

  void RejectUpgradeOffer();
  void AcceptUpgradeOffer();

  // Keeps the Channel alive at least until explicit shutdown on the IO thread.
  scoped_refptr<Channel> self_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

 private:
  void WaitForWriteOnIOThread();
  void WaitForWriteOnIOThreadNoLock();

  // base::CurrentThread::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override;

  // base::MessagePumpForIO::FdWatcher:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // Attempts to write a message directly to the channel. If the full message
  // cannot be written, it's queued and a wait is initiated to write the message
  // ASAP on the I/O thread.
  bool WriteNoLock(MessageView message_view);
  bool FlushOutgoingMessagesNoLock();

#if !BUILDFLAG(IS_NACL)
  bool WriteOutgoingMessagesWithWritev();

  // FlushOutgoingMessagesWritevNoLock is equivalent to
  // FlushOutgoingMessagesNoLock except it looks for opportunities to make
  // only a single write syscall by using writev(2) instead of write(2). In
  // most situations this is very straight forward; however, when a handle
  // needs to be transferred we cannot use writev(2) and instead will fall
  // back to the standard write.
  bool FlushOutgoingMessagesWritevNoLock();
#endif  // !BUILDFLAG(IS_NACL)

#if BUILDFLAG(IS_IOS)
  bool CloseHandles(const int* fds, size_t num_fds);
#endif  // BUILDFLAG(IS_IOS)

  // We may be initialized with a server socket, in which case this will be
  // valid until it accepts an incoming connection.
  PlatformChannelServerEndpoint server_;

  // The socket over which to communicate. May be passed in at construction time
  // or accepted over |server_|.
  base::ScopedFD socket_;

  // These watchers must only be accessed on the IO thread.
  std::unique_ptr<base::MessagePumpForIO::FdWatchController> read_watcher_;
  std::unique_ptr<base::MessagePumpForIO::FdWatchController> write_watcher_;

  base::circular_deque<base::ScopedFD> incoming_fds_;

  // Protects |pending_write_| and |outgoing_messages_|.
  base::Lock write_lock_;
  bool pending_write_ = false;
  bool reject_writes_ = false;
  base::circular_deque<MessageView> outgoing_messages_;

  bool leak_handle_ = false;

#if BUILDFLAG(IS_IOS)
  base::Lock fds_to_close_lock_;
  std::vector<base::ScopedFD> fds_to_close_;
#endif  // BUILDFLAG(IS_IOS)
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_CHANNEL_POSIX_H_
