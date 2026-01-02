// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_CHANNEL_LINUX_H_
#define MOJO_CORE_CHANNEL_LINUX_H_

#include <atomic>
#include <memory>
#include <optional>

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/core/channel_posix.h"

namespace mojo::core {

class DataAvailableNotifier;

// ChannelLinux is a specialization of ChannelPosix which has support for shared
// memory via Mojo channel upgrades. By default on Linux, CrOS, and Android
// every channel will be of type ChannelLinux which can be upgraded at runtime
// to take advantage of shared memory when all required kernel features are
// present.
class MOJO_SYSTEM_IMPL_EXPORT ChannelLinux : public ChannelPosix {
 public:
  ChannelLinux(Delegate* delegate,
               ConnectionParams connection_params,
               HandlePolicy handle_policy,
               scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  ChannelLinux(const ChannelLinux&) = delete;
  ChannelLinux& operator=(const ChannelLinux&) = delete;

  // KernelSupportsUpgradeRequirements will return true if the kernel supports
  // the features necessary to use an upgrade channel. How the channel will be
  // upgraded is an implementation detail and this just tells the caller that
  // calling Channel::UpgradeChannel() will have some effect.
  static bool KernelSupportsUpgradeRequirements();

  // Will return true if at least one feature that is available via upgrade is
  // enabled.
  static bool UpgradesEnabled();

  // Controls whether shared memory is used for this channel.
  static void SetSharedMemParameters(bool enabled, uint32_t num_pages);

  // ChannelPosix impl:
  void Write(MessagePtr message) override;
  void OfferSharedMemUpgrade();
  bool OnControlMessage(Message::MessageType message_type,
                        const void* payload,
                        size_t payload_size,
                        std::vector<PlatformHandle> handles) override;
  void OnWriteError(Error error) override;

  void RejectUpgradeOffer();
  void AcceptUpgradeOffer();

  void StartOnIOThread() override;
  void ShutDownOnIOThread() override;

 private:
  ~ChannelLinux() override;

  class SharedBuffer;

  std::optional<std::vector<PlatformHandle>> SetupMemFdForWrite();
  void OfferSharedMemUpgradeInternal();
  void SharedMemReadReady();

  // We only offer once, we use an atomic flag to guarantee no races to offer.
  std::atomic_flag offered_{false};

  base::Lock memfd_write_lock_;
  // True iff the outgoing communication over shared memory (and memfd) is
  // established. When |shared_mem_writer_| is false, sending messages falls
  // back to ChannelPosix (socket).
  bool shared_mem_writer_ GUARDED_BY(memfd_write_lock_) = false;
  std::unique_ptr<DataAvailableNotifier> write_notifier_
      GUARDED_BY(memfd_write_lock_);
  std::unique_ptr<SharedBuffer> write_buffer_ GUARDED_BY(memfd_write_lock_);
  bool reject_writes_ GUARDED_BY(memfd_write_lock_) = false;

  std::unique_ptr<DataAvailableNotifier> read_notifier_;
  std::unique_ptr<SharedBuffer> shared_read_buffer_;

  uint32_t num_pages_ = 0;

  // This is a temporary buffer we use to remove messages from the shared buffer
  // for validation and dispatching.
  std::vector<uint8_t> read_buf_;
};

}  // namespace mojo::core

#endif  // MOJO_CORE_CHANNEL_LINUX_H_
