// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_CHANNEL_POSIX_H_
#define MOJO_CORE_CHANNEL_POSIX_H_

#include "mojo/core/channel.h"

#include "base/containers/circular_deque.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/synchronization/lock.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"

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
  void Write(MessagePtr message) override LOCKS_EXCLUDED(write_lock_);
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
  virtual void StartOnIOThread() LOCKS_EXCLUDED(write_lock_);
  virtual void ShutDownOnIOThread() LOCKS_EXCLUDED(write_lock_
#if BUILDFLAG(IS_IOS)
                                                   ,
                                                   fds_to_close_lock_
#endif  // BUILDFLAG(IS_IOS)
  );
  virtual void OnWriteError(Error error) LOCKS_EXCLUDED(write_lock_);

  void RejectUpgradeOffer();
  void AcceptUpgradeOffer();

  // Keeps the Channel alive at least until explicit shutdown on the IO thread.
  scoped_refptr<Channel> self_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

 private:
  void WaitForWriteOnIOThread() LOCKS_EXCLUDED(write_lock_);
  void WaitForWriteOnIOThreadNoLock() EXCLUSIVE_LOCKS_REQUIRED(write_lock_);

  // base::CurrentThread::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override;

  // base::MessagePumpForIO::FdWatcher:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override
      LOCKS_EXCLUDED(write_lock_);

  // Attempts to write a message directly to the channel. If the full message
  // cannot be written, it's queued and a wait is initiated to write the message
  // ASAP on the I/O thread.
  bool WriteNoLock(MessageView message_view)
      EXCLUSIVE_LOCKS_REQUIRED(write_lock_)
#if BUILDFLAG(IS_IOS)
          LOCKS_EXCLUDED(fds_to_close_lock_)
#endif  // BUILDFLAG(IS_IOS)
              ;
  bool FlushOutgoingMessagesNoLock() EXCLUSIVE_LOCKS_REQUIRED(write_lock_);

#if !BUILDFLAG(IS_NACL)
  bool WriteOutgoingMessagesWithWritev() EXCLUSIVE_LOCKS_REQUIRED(write_lock_);

  // FlushOutgoingMessagesWritevNoLock is equivalent to
  // FlushOutgoingMessagesNoLock except it looks for opportunities to make
  // only a single write syscall by using writev(2) instead of write(2). In
  // most situations this is very straight forward; however, when a handle
  // needs to be transferred we cannot use writev(2) and instead will fall
  // back to the standard write.
  bool FlushOutgoingMessagesWritevNoLock()
      EXCLUSIVE_LOCKS_REQUIRED(write_lock_);
#endif  // !BUILDFLAG(IS_NACL)

#if BUILDFLAG(IS_IOS)
  bool CloseHandles(const int* fds, size_t num_fds)
      LOCKS_EXCLUDED(fds_to_close_lock_);
#endif  // BUILDFLAG(IS_IOS)

  // The socket over which to communicate.
  base::ScopedFD socket_;

  // These watchers must only be accessed on the IO thread. These are locked for
  // allowing concurrent nullptr checking the unique_ptr but not dereferencing
  // outside of the `io_task_runner_`.
  std::unique_ptr<base::MessagePumpForIO::FdWatchController> read_watcher_
      GUARDED_BY(write_lock_);
  std::unique_ptr<base::MessagePumpForIO::FdWatchController> write_watcher_
      GUARDED_BY(write_lock_);

  base::circular_deque<base::ScopedFD> incoming_fds_;

  base::Lock write_lock_;
  bool pending_write_ GUARDED_BY(write_lock_) = false;
  bool reject_writes_ GUARDED_BY(write_lock_) = false;
  base::circular_deque<MessageView> outgoing_messages_ GUARDED_BY(write_lock_);

  bool leak_handle_ = false;

#if BUILDFLAG(IS_IOS)
  base::Lock fds_to_close_lock_;
  std::vector<base::ScopedFD> fds_to_close_ GUARDED_BY(fds_to_close_lock_);
#endif  // BUILDFLAG(IS_IOS)
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_CHANNEL_POSIX_H_
