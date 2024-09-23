// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/channel_binder.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "base/android/binder.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/containers/circular_deque.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/lock.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/unguessable_token.h"
#include "mojo/core/embedder/features.h"
#include "mojo/public/cpp/platform/binder_exchange.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace mojo::core {

namespace {

// Transaction code for generic channel messages. Parcel layout is:
//
//   1. WirePayloadType (int32)
//        a. If kInline, a byte array for the payload
//        b. If kSharedMemory:
//           i.   Buffer size (uint32)
//           ii.  GUID high + low (uint64 x2)
//           iii. File descriptor
//   2. Handle count N (uint32)
//   3. N consecutive PlatformHandles which are each:
//        a. WireHandleType (int32)
//        b. A file descriptor or binder (for kFd or kBinder).
//
// This layout is not generally regarded as stable and is free to change.
static constexpr transaction_code_t kReceive = 1;

// Maximum payload size we're willing to serialize directly into a parcel.
// Anything larger than this has to be shuttled over by a shared memory object.
//
// Android imposes a hard 1 MB limit on total concurrent binder transaction size
// for each process. We want to stay well below that limit, and messages
// exceeding 64 kB are relatively rare in Chrome. Note that this threshold is
// chosen arbitrarily and we might be better served by something larger or
// smaller.
static constexpr size_t kMaxInlinePayloadSizeInBytes = 65536;

// Wire enum for message payload type.
enum class WirePayloadType : int32_t {
  // The payload data is inlined within the parcel as a byte array.
  kInline = 0,

  // The payload data is stored in a read-only ashmem region whose file
  // descriptor is included in the parcel.
  kSharedMemory = 1,
};

// Wire enum for PlatformHandle::Type.
enum class WireHandleType : int32_t {
  // The next position in the parcel is a file descriptor.
  kFd = 1,

  // The next position in the parcel is a binder.
  kBinder = 2,
};

base::android::BinderStatusOr<void> WriteSharedMemory(
    const base::android::ParcelWriter& out,
    base::ReadOnlySharedMemoryRegion region) {
  DCHECK_GE(region.GetSize(), kMaxInlinePayloadSizeInBytes);
  auto platform_region =
      base::ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
          std::move(region));
  const auto& guid = platform_region.GetGUID();
  RETURN_IF_ERROR(
      out.WriteUint32(base::checked_cast<uint32_t>(platform_region.GetSize())));
  RETURN_IF_ERROR(out.WriteUint64(guid.GetHighForSerialization()));
  RETURN_IF_ERROR(out.WriteUint64(guid.GetLowForSerialization()));
  return out.WriteFileDescriptor(platform_region.PassPlatformHandle());
}

base::android::BinderStatusOr<base::ReadOnlySharedMemoryMapping>
ReadSharedMemory(const base::android::ParcelReader& in) {
  ASSIGN_OR_RETURN(const size_t size, in.ReadUint32());
  ASSIGN_OR_RETURN(const uint64_t guid_high, in.ReadUint64());
  ASSIGN_OR_RETURN(const uint64_t guid_low, in.ReadUint64());
  ASSIGN_OR_RETURN(auto fd, in.ReadFileDescriptor());
  const auto guid = base::UnguessableToken::Deserialize(guid_high, guid_low);
  if (size < kMaxInlinePayloadSizeInBytes || !guid || !fd.is_valid()) {
    return base::unexpected(STATUS_BAD_VALUE);
  }

  auto platform_region = base::subtle::PlatformSharedMemoryRegion::Take(
      std::move(fd), base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly,
      size, *guid);
  if (!platform_region.IsValid()) {
    return base::unexpected(STATUS_BAD_VALUE);
  }

  auto region =
      base::ReadOnlySharedMemoryRegion::Deserialize(std::move(platform_region));
  auto mapping = region.Map();
  if (!mapping.IsValid()) {
    return base::unexpected(STATUS_BAD_VALUE);
  }
  return mapping;
}

base::android::BinderStatusOr<void> WriteMessagePayload(
    const base::android::ParcelWriter& out,
    base::span<const uint8_t> bytes) {
  if (bytes.size() <= kMaxInlinePayloadSizeInBytes) {
    RETURN_IF_ERROR(
        out.WriteInt32(static_cast<int32_t>(WirePayloadType::kInline)));
    return out.WriteByteArray(bytes);
  }

  auto [region, mapping] =
      base::ReadOnlySharedMemoryRegion::Create(bytes.size());
  if (!mapping.IsValid()) {
    return base::unexpected(STATUS_NO_MEMORY);
  }

  memcpy(mapping.memory(), bytes.data(), bytes.size());

  RETURN_IF_ERROR(
      out.WriteInt32(static_cast<int32_t>(WirePayloadType::kSharedMemory)));
  return WriteSharedMemory(out, std::move(region));
}

struct PayloadBuffer {
  std::unique_ptr<uint8_t> data;
  size_t size;
};
using ReceivedPayload =
    absl::variant<PayloadBuffer, base::ReadOnlySharedMemoryMapping>;
base::android::BinderStatusOr<ReceivedPayload> ReadMessagePayload(
    const base::android::ParcelReader& in) {
  ASSIGN_OR_RETURN(const auto type, in.ReadInt32());
  switch (static_cast<WirePayloadType>(type)) {
    case WirePayloadType::kInline: {
      PayloadBuffer buffer;
      RETURN_IF_ERROR(in.ReadByteArray([&buffer](size_t size) {
        buffer.data.reset(static_cast<uint8_t*>(operator new(size)));
        buffer.size = size;
        return buffer.data.get();
      }));
      return buffer;
    }

    case WirePayloadType::kSharedMemory:
      return ReadSharedMemory(in);

    default:
      return base::unexpected(STATUS_BAD_TYPE);
  }
}

base::android::BinderStatusOr<void> WritePlatformHandle(
    const base::android::ParcelWriter& out,
    PlatformHandle handle) {
  switch (handle.type()) {
    case PlatformHandle::Type::kFd:
      RETURN_IF_ERROR(
          out.WriteInt32(static_cast<int32_t>(WireHandleType::kFd)));
      return out.WriteFileDescriptor(handle.TakeFD());

    case PlatformHandle::Type::kBinder:
      RETURN_IF_ERROR(
          out.WriteInt32(static_cast<int32_t>(WireHandleType::kBinder)));
      return out.WriteBinder(handle.TakeBinder());

    default:
      NOTREACHED();
  }
}

base::android::BinderStatusOr<PlatformHandle> ReadPlatformHandle(
    const base::android::ParcelReader& in) {
  ASSIGN_OR_RETURN(const auto type, in.ReadInt32());
  switch (static_cast<WireHandleType>(type)) {
    case WireHandleType::kFd: {
      ASSIGN_OR_RETURN(base::ScopedFD fd, in.ReadFileDescriptor());
      if (!fd.is_valid()) {
        return base::unexpected(STATUS_BAD_VALUE);
      }
      return PlatformHandle(std::move(fd));
    }

    case WireHandleType::kBinder: {
      ASSIGN_OR_RETURN(base::android::BinderRef binder, in.ReadBinder());
      if (!binder) {
        return base::unexpected(STATUS_BAD_VALUE);
      }
      return PlatformHandle(std::move(binder));
    }

    default:
      return base::unexpected(STATUS_BAD_VALUE);
  }
}

bool ShouldUseSyncTransactions() {
  static const bool use_sync_transactions = GetFieldTrialParamByFeatureAsBool(
      kMojoUseBinder, "use_sync_transactions", true);
  return use_sync_transactions;
}

}  // namespace

ChannelBinder::ChannelBinder(
    Delegate* delegate,
    ConnectionParams connection_params,
    HandlePolicy handle_policy,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : Channel(delegate, handle_policy, DispatchBufferPolicy::kUnmanaged),
      io_task_runner_(std::move(io_task_runner)),
      peer_(PendingExchange{
          connection_params.TakeEndpoint().TakePlatformHandle().TakeBinder()}) {
}

ChannelBinder::~ChannelBinder() = default;

void ChannelBinder::Start() {
  base::android::BinderRef self;
  std::optional<base::android::BinderRef> exchange;
  {
    base::AutoLock lock(lock_);
    exchange = absl::get<PendingExchange>(peer_).binder;
    CHECK(exchange);
    peer_ = PendingConnection{};
    receiver_ = base::MakeRefCounted<Receiver>(this);
    self = receiver_->GetBinder();
  }
  const auto result =
      ExchangeBinders(std::move(*exchange), std::move(self),
                      base::BindOnce(&ChannelBinder::SetPeerReceiver, this));
  if (!result.has_value()) {
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ChannelBinder::OnError, this,
                                  Channel::Error::kConnectionFailed));
  }
}

void ChannelBinder::ShutDownImpl() {
  // Changes to binder state (namely in this case, dropping binder refs) can
  // synchronously enter other objects in the same process. This could lead to
  // surprises when resetting `peer_` or `outgoing_messages` if they contain
  // binders to objects in the same process, and those objects happen to do
  // work which then re-enters this object. As such we are careful not to drop
  // binder references while holding `lock_`.
  scoped_refptr<Receiver> receiver;
  Peer peer = Disconnected{};
  base::circular_deque<MessagePtr> outgoing_messages;
  {
    base::AutoLock lock(lock_);
    receiver_.swap(receiver);
    outgoing_messages_.swap(outgoing_messages);
    peer_.swap(peer);
    if (leak_peer_ && absl::holds_alternative<Receiver::Proxy>(peer)) {
      std::ignore = absl::get<Receiver::Proxy>(peer).release();
    }
  }
  receiver->ShutDown();
}

void ChannelBinder::Write(MessagePtr message) {
  if (WriteOrEnqueue(std::move(message)).has_value()) {
    return;
  }

  base::AutoLock lock(lock_);
  reject_writes_ = true;
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ChannelBinder::OnError, this, Error::kDisconnected));
}

void ChannelBinder::LeakHandle() {
  base::AutoLock lock(lock_);
  leak_peer_ = true;
}

bool ChannelBinder::GetReadPlatformHandles(const void* payload,
                                           size_t payload_size,
                                           size_t num_handles,
                                           const void* extra_header,
                                           size_t extra_header_size,
                                           std::vector<PlatformHandle>* handles,
                                           bool* deferred) {
  // Never called because we use DispatchBufferPolicy::kUnmanaged.
  NOTREACHED();
}

bool ChannelBinder::GetReadPlatformHandlesForIpcz(
    size_t num_handles,
    std::vector<PlatformHandle>& handles) {
  // Never called because we use DispatchBufferPolicy::kUnmanaged.
  NOTREACHED();
}

base::android::BinderStatusOr<void> ChannelBinder::WriteOrEnqueue(
    MessagePtr message) {
  std::optional<Receiver::Proxy> receiver;
  {
    base::AutoLock lock(lock_);
    if (absl::holds_alternative<Disconnected>(peer_) || reject_writes_) {
      return base::ok();
    }

    if (absl::holds_alternative<PendingExchange>(peer_) ||
        absl::holds_alternative<PendingConnection>(peer_) ||
        !outgoing_messages_.empty() || is_writing_) {
      outgoing_messages_.push_back(std::move(message));
      return base::ok();
    }

    is_writing_ = true;
    receiver = absl::get<Receiver::Proxy>(peer_);
  }

  // If this returns on error, `is_writing_` will remain true. This is fine
  // because all writes will be rejected after this anyway.
  RETURN_IF_ERROR(SendMessageToReceiver(*receiver, std::move(message)));

  // Ensure the outgoing queue is flushed before we unblock writes.
  base::AutoLock lock(lock_);
  const auto flush_result = FlushOutgoingMessages();
  is_writing_ = false;
  return flush_result;
}

base::android::BinderStatusOr<void> ChannelBinder::FlushOutgoingMessages()
    EXCLUSIVE_LOCKS_REQUIRED(lock_) {
  DCHECK(is_writing_);
  if (absl::holds_alternative<Disconnected>(peer_)) {
    // If we're already disconnected we don't need to do any flushing.
    return base::ok();
  }

  Receiver::Proxy receiver = absl::get<Receiver::Proxy>(peer_);
  while (!outgoing_messages_.empty()) {
    base::circular_deque<MessagePtr> messages;
    messages.swap(outgoing_messages_);

    // Note that we do not want to hold this lock while transacting on the
    // peer binder, in case the transaction elicits side-effects which might
    // re-enter this object. `is_writing_` is already held true to prevent
    // concurrent sends on other threads.
    base::AutoUnlock unlock(lock_);
    for (auto& message : messages) {
      RETURN_IF_ERROR(SendMessageToReceiver(receiver, std::move(message)));
    }
  }
  return base::ok();
}

void ChannelBinder::SetPeerReceiver(base::android::BinderRef receiver) {
  base::AutoLock lock(lock_);
  if (absl::holds_alternative<Disconnected>(peer_)) {
    // Channel is already shutdown. Silently drop the peer endpoint.
    return;
  }

  DCHECK(absl::holds_alternative<PendingConnection>(peer_));
  if (!receiver) {
    // Connection failed.
    peer_ = Disconnected{};
    return;
  }

  auto proxy = Receiver::Proxy::Adopt(std::move(receiver));
  if (!proxy) {
    // Class association may fail if the remote process is already dead.
    peer_ = Disconnected{};
    return;
  }

  peer_ = std::move(proxy);
  const base::AutoReset<bool> writing_scope(&is_writing_, true);
  if (!FlushOutgoingMessages().has_value()) {
    reject_writes_ = true;
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ChannelBinder::OnError, this, Error::kDisconnected));
  }
}

void ChannelBinder::Receive(base::span<const uint8_t> bytes,
                            std::vector<PlatformHandle> handles) {
  size_t ignored_size_hint;
  const DispatchResult result = TryDispatchMessage(
      base::as_chars(bytes), std::move(handles), &ignored_size_hint);
  if (result != DispatchResult::kOK) {
    OnError(Error::kReceivedMalformedData);
  }
}

void ChannelBinder::OnDisconnect() {
  OnError(Error::kDisconnected);
}

base::android::BinderStatusOr<void> ChannelBinder::SendMessageToReceiver(
    Receiver::Proxy& receiver,
    MessagePtr message) {
  ASSIGN_OR_RETURN(auto parcel, receiver.PrepareTransaction());
  const base::android::ParcelWriter writer(parcel);

  const auto bytes = base::make_span(
      static_cast<const uint8_t*>(message->data()), message->data_num_bytes());
  RETURN_IF_ERROR(WriteMessagePayload(writer, bytes));

  auto handles = message->TakeHandles();
  RETURN_IF_ERROR(
      writer.WriteUint32(base::checked_cast<uint32_t>(handles.size())));
  for (auto& handle : handles) {
    RETURN_IF_ERROR(WritePlatformHandle(writer, handle.TakeHandle()));
  }

  if (ShouldUseSyncTransactions()) {
    ASSIGN_OR_RETURN(auto empty_reply,
                     receiver.Transact(kReceive, std::move(parcel)));
    return base::ok();
  } else {
    return receiver.TransactOneWay(kReceive, std::move(parcel));
  }
}

ChannelBinder::Receiver::Receiver(scoped_refptr<ChannelBinder> channel)
    : channel_(std::move(channel)) {}

ChannelBinder::Receiver::~Receiver() = default;

void ChannelBinder::Receiver::ShutDown() {
  // We avoid holding `lock_` while dropping our ChannelBinder reference, in
  // case the ChannelBinder is destroyed. Side-effects of its destruction could
  // involve re-entering this Receiver (e.g. it may case `this` itself to be
  // destroyed.)
  scoped_refptr<ChannelBinder> channel;
  base::AutoLock lock(lock_);
  channel_.swap(channel);
}

base::android::BinderStatusOr<void>
ChannelBinder::Receiver::OnBinderTransaction(
    transaction_code_t code,
    const base::android::ParcelReader& in,
    const base::android::ParcelWriter& out) {
  if (code != kReceive) {
    return base::unexpected(STATUS_UNKNOWN_TRANSACTION);
  }

  scoped_refptr<ChannelBinder> channel;
  {
    base::AutoLock lock(lock_);
    channel = channel_;
  }
  if (!channel) {
    return base::ok();
  }

  ASSIGN_OR_RETURN(const auto payload, ReadMessagePayload(in));
  const auto bytes = absl::visit(
      base::Overloaded{
          [](const PayloadBuffer& payload) {
            return base::span<const uint8_t>(payload.data.get(), payload.size);
          },
          [](const base::ReadOnlySharedMemoryMapping& mapping) {
            return mapping.GetMemoryAsSpan<uint8_t>();
          },
      },
      payload);

  ASSIGN_OR_RETURN(const auto num_handles, in.ReadUint32());
  std::vector<PlatformHandle> handles;
  handles.reserve(num_handles);
  for (uint32_t i = 0; i < num_handles; ++i) {
    ASSIGN_OR_RETURN(auto handle, ReadPlatformHandle(in));
    handles.push_back(std::move(handle));
  }
  channel->Receive(bytes, std::move(handles));
  return base::ok();
}

void ChannelBinder::Receiver::OnBinderDestroyed() {
  scoped_refptr<ChannelBinder> channel;
  {
    base::AutoLock lock(lock_);
    channel = channel_;
  }
  if (channel) {
    channel->OnDisconnect();
  }
}

}  // namespace mojo::core
