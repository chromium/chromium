// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_CHANNEL_BINDER_H_
#define MOJO_CORE_CHANNEL_BINDER_H_

#include <cstdint>
#include <vector>

#include "base/android/binder.h"
#include "base/containers/circular_deque.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "mojo/core/channel.h"
#include "mojo/core/connection_params.h"
#include "mojo/core/system_impl_export.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace mojo::core {

// A Binder-based Channel implementation.
class MOJO_SYSTEM_IMPL_EXPORT ChannelBinder : public Channel {
 public:
  ChannelBinder(Delegate* delegate,
                ConnectionParams connection_params,
                HandlePolicy handle_policy,
                scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

 private:
  DEFINE_BINDER_CLASS(ReceiverInterface);

  friend class Receiver;
  class Receiver : public base::android::SupportsBinder<ReceiverInterface> {
   public:
    using Proxy = ReceiverInterface::BinderRef;

    explicit Receiver(scoped_refptr<ChannelBinder> channel);

    void ShutDown();

    // base::android::SupportsBinder<ReceiverInterface>:
    base::android::BinderStatusOr<void> OnBinderTransaction(
        transaction_code_t code,
        const base::android::ParcelReader& in,
        const base::android::ParcelWriter& out) override;
    void OnBinderDestroyed() override;

   private:
    ~Receiver() override;

    base::Lock lock_;
    scoped_refptr<ChannelBinder> channel_ GUARDED_BY(lock_);
  };

  ~ChannelBinder() override;

  // Channel:
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

  base::android::BinderStatusOr<void> WriteOrEnqueue(MessagePtr message);
  base::android::BinderStatusOr<void> FlushOutgoingMessages()
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void SetPeerReceiver(base::android::BinderRef receiver);
  void Receive(base::span<const uint8_t> bytes,
               std::vector<PlatformHandle> handles);
  void OnDisconnect();

  static base::android::BinderStatusOr<void> SendMessageToReceiver(
      Receiver::Proxy& receiver,
      MessagePtr message);

  // Peer state begins as PendingExchange at ChannelBinder construction time.
  //
  // When Start() is called, a binder exchange is initiated and we enter the
  // PendingConnection state while awaiting a SetPeerReceiver() callback from
  // the exchange.
  //
  // Once we have the peer binder we adopt it as a Receiver::Proxy, and this
  // is retained by `peer_` indefinitely or until disconnection.
  //
  // At any point after Start(), if our own Receiver becomes disconnected (i.e.
  // its binder ref count drops to zero), `peer_` enters a permanent
  // Disconnected state.
  struct PendingExchange {
    base::android::BinderRef binder;
  };
  struct PendingConnection {};
  enum class Disconnected {};
  using Peer = absl::variant<PendingExchange,
                             PendingConnection,
                             Receiver::Proxy,
                             Disconnected>;

  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  base::Lock lock_;

  // Indicates whether LeakHandle() was called by the Channel owner, requiring
  // us to avoid clean destruction of our peer binder once connected.
  bool leak_peer_ GUARDED_BY(lock_) = false;

  // Indicates that writes are no longer accepted on the Channel and that all
  // subsequent outgoing messages will be dropped. This is set permanently once
  // any write fails.
  bool reject_writes_ GUARDED_BY(lock_) = false;

  // The object receiving incoming parcels from our remote peer. Ownership of
  // this object is shared by this ChannelBinder and the peer ChannelBinder (via
  // a binder ref.)
  scoped_refptr<Receiver> receiver_ GUARDED_BY(lock_);

  // The state of our connection to the peer ChannelBinder. In a steady
  // connected state this is the Receiver::Proxy we use to transmit messages to
  // the peer.
  Peer peer_ GUARDED_BY(lock_);

  // A queue of outgoing messages which can accumulate either before connection
  // or while another thread is already actively writing or flushing messages
  // across the channel.
  base::circular_deque<MessagePtr> outgoing_messages_ GUARDED_BY(lock_);

  // Indicates whether a thread is currently writing or flushing messages across
  // the channel. Only one thread may do this at a time.
  bool is_writing_ GUARDED_BY(lock_) = false;
};

}  // namespace mojo::core

#endif  // MOJO_CORE_CHANNEL_BINDER_H_
