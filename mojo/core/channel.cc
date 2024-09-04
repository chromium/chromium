// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/channel.h"

#include <stddef.h>
#include <string.h>

#include <cstdint>
#include <limits>
#include <utility>

#include "base/check_op.h"
#include "base/functional/overloaded.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/process/current_process.h"
#include "base/process/process_handle.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "mojo/core/configuration.h"
#include "mojo/core/embedder/features.h"

#if BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
#include "base/apple/mach_logging.h"
#elif BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "mojo/core/channel_binder.h"
#endif

namespace mojo::core {

namespace {

std::atomic_bool g_use_trivial_messages{false};

// TODO(crbug.com/40824727): Consider asking the memory allocator for a suitable
// size.
constexpr int kGrowthFactor = 2;

static_assert(
    IsAlignedForChannelMessage(sizeof(Channel::Message::LegacyHeader)),
    "Invalid LegacyHeader size.");

static_assert(IsAlignedForChannelMessage(sizeof(Channel::Message::Header)),
              "Invalid Header size.");

static_assert(sizeof(Channel::Message::LegacyHeader) == 8,
              "LegacyHeader must be 8 bytes on ChromeOS and Android");

static_assert(offsetof(Channel::Message::LegacyHeader, num_bytes) ==
                  offsetof(Channel::Message::Header, num_bytes),
              "num_bytes should be at the same offset in both Header structs.");
static_assert(offsetof(Channel::Message::LegacyHeader, message_type) ==
                  offsetof(Channel::Message::Header, message_type),
              "message_type should be at the same offset in both Header "
              "structs.");

const size_t kReadBufferSize = 4096;
const size_t kMaxUnusedReadBufferCapacity = 4096;

#if BUILDFLAG(IS_FUCHSIA)
// Fuchsia: The zx_channel_write() API supports up to 64 handles.
const size_t kMaxAttachedHandles = 64;
#else
// Linux: The platform imposes a limit of 253 handles per sendmsg().
const size_t kMaxAttachedHandles = 253;
#endif  // BUILDFLAG(IS_FUCHSIA)

static_assert(alignof(std::max_align_t) >= kChannelMessageAlignment, "");
Channel::AlignedBuffer MakeAlignedBuffer(size_t size) {
  // Generic allocators (such as malloc) return a pointer that is suitably
  // aligned for storing any type of object with a fundamental alignment
  // requirement. Buffers have no additional alignment requirement beyond that.
  void* ptr = operator new(size);
  // Even though the allocator is configured in such a way that it crashes
  // rather than return nullptr, ASAN and friends don't know about that. This
  // CHECK() prevents Clusterfuzz from complaining. crbug.com/1180576.
  CHECK(ptr);
  return Channel::AlignedBuffer(static_cast<char*>(ptr));
}

struct TrivialMessage;

// The type of message always used by a Channel which backs an ipcz transport.
// Most of the inherited Message interface is unused since it's only called by
// the original Mojo Core implementation.
struct IpczMessage : public Channel::Message {
  IpczMessage(base::span<const uint8_t> data,
              std::vector<PlatformHandle> handles) {
    size_ = sizeof(IpczHeader) + data.size();
    data_.reset(static_cast<char*>(operator new(size_)));

    IpczHeader& header = *reinterpret_cast<IpczHeader*>(data_.get());
    header.size = sizeof(IpczHeader);

    DCHECK_LE(handles.size(), std::numeric_limits<uint16_t>::max());
    DCHECK_LE(size_, std::numeric_limits<uint32_t>::max());
    header.num_handles = static_cast<uint16_t>(handles.size());
    header.num_bytes = static_cast<uint32_t>(size_);
    header.v2.creation_timeticks_us =
        (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds();
    memcpy(&header + 1, data.data(), data.size());

    handles_.reserve(handles.size());
    for (PlatformHandle& handle : handles) {
      handles_.emplace_back(std::move(handle));
    }
  }
  ~IpczMessage() override = default;

  // Channel::Message:
  void SetHandles(std::vector<PlatformHandle>) override { NOTREACHED(); }
  void SetHandles(std::vector<PlatformHandleInTransit>) override {
    NOTREACHED();
  }
  std::vector<PlatformHandleInTransit> TakeHandles() override {
    return std::move(handles_);
  }
  size_t NumHandlesForTransit() const override { return handles_.size(); }

  const void* data() const override { return data_.get(); }
  void* mutable_data() const override { NOTREACHED(); }
  size_t capacity() const override { return size_; }

  bool ExtendPayload(size_t) override { NOTREACHED(); }

 private:
  Channel::AlignedBuffer data_;
  std::vector<PlatformHandleInTransit> handles_;
};

// A complex message can be large or contain file handles.
struct ComplexMessage : public Channel::Message {
  ComplexMessage() = default;
  ComplexMessage(size_t capacity,
                 size_t max_handles,
                 size_t payload_size,
                 MessageType message_type);

  ComplexMessage(const ComplexMessage&) = delete;
  ComplexMessage& operator=(const ComplexMessage&) = delete;

  ~ComplexMessage() override = default;

  // Message impl:
  void SetHandles(std::vector<PlatformHandle> new_handles) override;
  void SetHandles(std::vector<PlatformHandleInTransit> new_handles) override;
  std::vector<PlatformHandleInTransit> TakeHandles() override;
  size_t NumHandlesForTransit() const override;

  const void* data() const override { return data_.get(); }
  void* mutable_data() const override { return data_.get(); }
  size_t capacity() const override;

  bool ExtendPayload(size_t new_payload_size) override;

 private:
  friend struct Channel::Message;
  friend struct TrivialMessage;

  // The message data buffer.
  Channel::AlignedBuffer data_;

  // The capacity of the buffer at |data_|.
  size_t capacity_ = 0;

  // Maximum number of handles which may be attached to this message.
  size_t max_handles_ = 0;

  std::vector<PlatformHandleInTransit> handle_vector_;

#if BUILDFLAG(IS_WIN)
  // On Windows, handles are serialised into the extra header section.
  raw_ptr<HandleEntry, AllowPtrArithmetic> handles_ = nullptr;
#elif BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
  // On OSX, handles are serialised into the extra header section.
  raw_ptr<MachPortsExtraHeader, AllowPtrArithmetic> mach_ports_header_ =
      nullptr;
#endif
};

struct TrivialMessage : public Channel::Message {
  TrivialMessage(const TrivialMessage&) = delete;
  TrivialMessage& operator=(const TrivialMessage&) = delete;

  ~TrivialMessage() override = default;

  // TryConstruct should be used to build a TrivialMessage.
  static Channel::MessagePtr TryConstruct(size_t payload_size,
                                          MessageType message_type);

  // Message impl:
  const void* data() const override { return &data_[0]; }
  void* mutable_data() const override {
    return const_cast<uint8_t*>(&data_[0]);
  }

  size_t capacity() const override;

  bool ExtendPayload(size_t new_payload_size) override;

  // The following interface methods are NOT supported on a Trivial message.
  void SetHandles(std::vector<PlatformHandle> new_handles) override;
  void SetHandles(std::vector<PlatformHandleInTransit> new_handles) override;
  std::vector<PlatformHandleInTransit> TakeHandles() override;
  size_t NumHandlesForTransit() const override;

 private:
  friend struct Channel::Message;
  TrivialMessage() = default;

  alignas(sizeof(void*)) uint8_t data_[256 - sizeof(Channel::Message)];

  static constexpr size_t kInternalCapacity = sizeof(data_);
};

static_assert(sizeof(TrivialMessage) == 256,
              "Expected TrivialMessage to be 256 bytes");

}  // namespace

// static
Channel::MessagePtr Channel::Message::CreateMessage(size_t payload_size,
                                                    size_t max_handles) {
  return CreateMessage(payload_size, payload_size, max_handles);
}

// static
Channel::MessagePtr Channel::Message::CreateMessage(size_t payload_size,
                                                    size_t max_handles,
                                                    MessageType message_type) {
  return CreateMessage(payload_size, payload_size, max_handles, message_type);
}

// static
Channel::MessagePtr Channel::Message::CreateMessage(size_t capacity,
                                                    size_t payload_size,
                                                    size_t max_handles) {
#if defined(MOJO_CORE_LEGACY_PROTOCOL)
  return CreateMessage(capacity, payload_size, max_handles,
                       Message::MessageType::NORMAL_LEGACY);
#else
  return CreateMessage(capacity, payload_size, max_handles,
                       Message::MessageType::NORMAL);
#endif
}

// static
Channel::MessagePtr Channel::Message::CreateMessage(size_t capacity,
                                                    size_t payload_size,
                                                    size_t max_handles,
                                                    MessageType message_type) {
  if (g_use_trivial_messages &&
      (capacity + std::max(sizeof(Header), sizeof(LegacyHeader))) <=
          TrivialMessage::kInternalCapacity &&
      max_handles == 0) {
    // The TrivialMessage has a fixed capacity so if the requested capacity
    // plus a header can fit then we can try to construct a TrivialMessage.
    auto msg = TrivialMessage::TryConstruct(payload_size, message_type);
    if (msg) {
      return msg;
    }
  }

  return base::WrapUnique<Channel::Message>(
      new ComplexMessage(capacity, payload_size, max_handles, message_type));
}

// static
Channel::MessagePtr Channel::Message::CreateIpczMessage(
    base::span<const uint8_t> data,
    std::vector<PlatformHandle> handles) {
  return std::make_unique<IpczMessage>(data, std::move(handles));
}

// static
void Channel::set_use_trivial_messages(bool use_trivial_messages) {
  g_use_trivial_messages = use_trivial_messages;
}

// static
Channel::MessagePtr Channel::Message::CreateRawForFuzzing(
    base::span<const unsigned char> data) {
  auto message = std::make_unique<ComplexMessage>();
  message->size_ = data.size();
  if (data.size()) {
    message->data_ = MakeAlignedBuffer(data.size());
    base::ranges::copy(data, message->data_.get());
  }
  return base::WrapUnique<Channel::Message>(message.release());
}

// static
Channel::MessagePtr Channel::Message::Deserialize(
    const void* data,
    size_t data_num_bytes,
    HandlePolicy handle_policy,
    base::ProcessHandle from_process) {
  if (data_num_bytes < sizeof(LegacyHeader))
    return nullptr;

  const LegacyHeader* legacy_header =
      reinterpret_cast<const LegacyHeader*>(data);
  if (legacy_header->num_bytes != data_num_bytes) {
    DLOG(ERROR) << "Decoding invalid message: " << legacy_header->num_bytes
                << " != " << data_num_bytes;
    return nullptr;
  }

  // If a message isn't explicitly identified as type NORMAL_LEGACY, it is
  // expected to have a full-size header.
  const Header* header = nullptr;
  if (legacy_header->message_type != MessageType::NORMAL_LEGACY)
    header = reinterpret_cast<const Header*>(data);

  uint32_t extra_header_size = 0;
  size_t payload_size = 0;
  const char* payload = nullptr;
  if (!header) {
    payload_size = data_num_bytes - sizeof(LegacyHeader);
    payload = static_cast<const char*>(data) + sizeof(LegacyHeader);
  } else {
    if (header->num_bytes < header->num_header_bytes ||
        header->num_header_bytes < sizeof(Header)) {
      DLOG(ERROR) << "Decoding invalid message: " << header->num_bytes << " < "
                  << header->num_header_bytes;
      return nullptr;
    }
    extra_header_size = header->num_header_bytes - sizeof(Header);
    payload_size = data_num_bytes - header->num_header_bytes;
    payload = static_cast<const char*>(data) + header->num_header_bytes;
  }

  if (!IsAlignedForChannelMessage(extra_header_size)) {
    // Well-formed messages always have any extra header bytes aligned to a
    // |kChannelMessageAlignment| boundary.
    DLOG(ERROR) << "Invalid extra header size";
    return nullptr;
  }

#if BUILDFLAG(IS_WIN)
  uint32_t max_handles = extra_header_size / sizeof(HandleEntry);
#elif BUILDFLAG(IS_FUCHSIA)
  uint32_t max_handles = extra_header_size / sizeof(HandleInfoEntry);
#elif BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
  if (extra_header_size > 0 &&
      extra_header_size < sizeof(MachPortsExtraHeader)) {
    DLOG(ERROR) << "Decoding invalid message: " << extra_header_size << " < "
                << sizeof(MachPortsExtraHeader);
    return nullptr;
  }
  uint32_t max_handles =
      extra_header_size == 0
          ? 0
          : (extra_header_size - sizeof(MachPortsExtraHeader)) /
                sizeof(MachPortsEntry);
#else
  const uint32_t max_handles = 0;
  // No extra header expected. Fail if this is detected.
  if (extra_header_size > 0) {
    DLOG(ERROR) << "Decoding invalid message: unexpected extra_header_size > 0";
    return nullptr;
  }
#endif  // BUILDFLAG(IS_WIN)

  const uint16_t num_handles =
      header ? header->num_handles : legacy_header->num_handles;
  if (num_handles > max_handles || max_handles > kMaxAttachedHandles) {
    DLOG(ERROR) << "Decoding invalid message: " << num_handles << " > "
                << max_handles;
    return nullptr;
  }

  if (num_handles > 0 && handle_policy == HandlePolicy::kRejectHandles) {
    DLOG(ERROR) << "Rejecting message with unexpected handle attachments.";
    return nullptr;
  }

  MessagePtr message =
      CreateMessage(payload_size, max_handles, legacy_header->message_type);
  DCHECK_EQ(message->data_num_bytes(), data_num_bytes);

  // Copy all payload bytes.
  if (payload_size)
    memcpy(message->mutable_payload(), payload, payload_size);

  if (header) {
    DCHECK_EQ(message->extra_header_size(), extra_header_size);
    DCHECK_EQ(message->header()->num_header_bytes, header->num_header_bytes);

    if (message->extra_header_size()) {
      // Copy extra header bytes.
      memcpy(message->mutable_extra_header(),
             static_cast<const char*>(data) + sizeof(Header),
             message->extra_header_size());
    }
    message->header()->num_handles = header->num_handles;
  } else {
    message->legacy_header()->num_handles = legacy_header->num_handles;
  }

#if BUILDFLAG(IS_WIN)
  std::vector<PlatformHandleInTransit> handles(num_handles);
  for (size_t i = 0; i < num_handles; i++) {
    HANDLE handle = base::win::Uint32ToHandle(
        static_cast<ComplexMessage*>(message.get())->handles_[i].handle);
    if (PlatformHandleInTransit::IsPseudoHandle(handle))
      return nullptr;
    if (from_process == base::kNullProcessHandle) {
      handles[i] = PlatformHandleInTransit(
          PlatformHandle(base::win::ScopedHandle(handle)));
    } else {
      handles[i] = PlatformHandleInTransit(
          PlatformHandleInTransit::TakeIncomingRemoteHandle(handle,
                                                            from_process));
    }
  }
  message->SetHandles(std::move(handles));
#endif

  return message;
}

// static
void Channel::Message::ExtendPayload(MessagePtr& message,
                                     size_t new_payload_size) {
  if (message->ExtendPayload(new_payload_size)) {
    return;
  }

  // ComplexMessage will never fail to extend the payload; therefore, if we do
  // fail it's because the message is a TrivialMessage which has run out of
  // space. In which case we will upgrade the message type to a ComplexMessage.
  size_t capacity_without_header = message->capacity();
  auto m = base::WrapUnique<Channel::Message>(
      new ComplexMessage(new_payload_size, new_payload_size, 0,
                         message->legacy_header()->message_type));
  memcpy(m->mutable_payload(), message->payload(), capacity_without_header);
  message.swap(m);
}

const void* Channel::Message::extra_header() const {
  DCHECK(!is_legacy_message());
  return reinterpret_cast<const uint8_t*>(data()) + sizeof(Header);
}

void* Channel::Message::mutable_extra_header() {
  DCHECK(!is_legacy_message());
  return reinterpret_cast<uint8_t*>(mutable_data()) + sizeof(Header);
}

size_t Channel::Message::extra_header_size() const {
  return header()->num_header_bytes - sizeof(Header);
}

void* Channel::Message::mutable_payload() {
  if (is_legacy_message())
    return static_cast<void*>(legacy_header() + 1);
  return reinterpret_cast<uint8_t*>(mutable_data()) +
         header()->num_header_bytes;
}

const void* Channel::Message::payload() const {
  if (is_legacy_message())
    return static_cast<const void*>(legacy_header() + 1);
  return reinterpret_cast<const uint8_t*>(data()) + header()->num_header_bytes;
}

size_t Channel::Message::payload_size() const {
  if (is_legacy_message())
    return legacy_header()->num_bytes - sizeof(LegacyHeader);
  return size_ - header()->num_header_bytes;
}

size_t Channel::Message::num_handles() const {
  return is_legacy_message() ? legacy_header()->num_handles
                             : header()->num_handles;
}

bool Channel::Message::has_handles() const {
  return (is_legacy_message() ? legacy_header()->num_handles
                              : header()->num_handles) > 0;
}

bool Channel::Message::is_legacy_message() const {
  return legacy_header()->message_type == MessageType::NORMAL_LEGACY;
}

Channel::Message::LegacyHeader* Channel::Message::legacy_header() const {
  return reinterpret_cast<LegacyHeader*>(mutable_data());
}

Channel::Message::Header* Channel::Message::header() const {
  DCHECK(!is_legacy_message());
  return reinterpret_cast<Header*>(mutable_data());
}

ComplexMessage::ComplexMessage(size_t capacity,
                               size_t payload_size,
                               size_t max_handles,
                               MessageType message_type)
    : max_handles_(max_handles) {
  DCHECK_GE(capacity, payload_size);
  DCHECK_LE(max_handles_, kMaxAttachedHandles);

  const bool is_legacy_message = (message_type == MessageType::NORMAL_LEGACY);
  size_t extra_header_size = 0;
#if BUILDFLAG(IS_WIN)
  // On Windows we serialize HANDLEs into the extra header space.
  extra_header_size = max_handles_ * sizeof(HandleEntry);
#elif BUILDFLAG(IS_FUCHSIA)
  // On Fuchsia we serialize handle types into the extra header space.
  extra_header_size = max_handles_ * sizeof(HandleInfoEntry);
#elif BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
  // On OSX, some of the platform handles may be mach ports, which are
  // serialised into the message buffer. Since there could be a mix of fds and
  // mach ports, we store the mach ports as an <index, port> pair (of uint32_t),
  // so that the original ordering of handles can be re-created.
  if (max_handles) {
    extra_header_size =
        sizeof(MachPortsExtraHeader) + (max_handles * sizeof(MachPortsEntry));
  }
#endif
  // Pad extra header data to be aliged to |kChannelMessageAlignment| bytes.
  if (!IsAlignedForChannelMessage(extra_header_size)) {
    extra_header_size += kChannelMessageAlignment -
                         (extra_header_size % kChannelMessageAlignment);
  }
  DCHECK(IsAlignedForChannelMessage(extra_header_size));
  const size_t header_size =
      is_legacy_message ? sizeof(LegacyHeader) : sizeof(Header);
  DCHECK(extra_header_size == 0 || !is_legacy_message);

  capacity_ = header_size + extra_header_size + capacity;
  size_ = header_size + extra_header_size + payload_size;
  data_ = MakeAlignedBuffer(capacity_);
  // Only zero out the header and not the payload. Since the payload is going to
  // be memcpy'd, zeroing the payload is unnecessary work and a significant
  // performance issue when dealing with large messages. Any sanitizer errors
  // complaining about an uninitialized read in the payload area should be
  // treated as an error and fixed.
  memset(mutable_data(), 0, header_size + extra_header_size);

  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(size_));
  legacy_header()->num_bytes = static_cast<uint32_t>(size_);

  DCHECK(base::IsValueInRangeForNumericType<uint16_t>(header_size +
                                                      extra_header_size));
  legacy_header()->message_type = message_type;

  if (is_legacy_message) {
    legacy_header()->num_handles = static_cast<uint16_t>(max_handles);
  } else {
    header()->num_header_bytes =
        static_cast<uint16_t>(header_size + extra_header_size);
  }

  if (max_handles_ > 0) {
#if BUILDFLAG(IS_WIN)
    handles_ = reinterpret_cast<HandleEntry*>(mutable_extra_header());
    // Initialize all handles to invalid values.
    for (size_t i = 0; i < max_handles_; ++i)
      handles_[i].handle = base::win::HandleToUint32(INVALID_HANDLE_VALUE);
#elif BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
    mach_ports_header_ =
        reinterpret_cast<MachPortsExtraHeader*>(mutable_extra_header());
    mach_ports_header_->num_ports = 0;
    // Initialize all handles to invalid values.
    for (size_t i = 0; i < max_handles_; ++i) {
      mach_ports_header_->entries[i] = {0};
    }
#endif
  }
}

size_t ComplexMessage::capacity() const {
  if (is_legacy_message())
    return capacity_ - sizeof(LegacyHeader);
  return capacity_ - header()->num_header_bytes;
}

bool ComplexMessage::ExtendPayload(size_t new_payload_size) {
  size_t capacity_without_header = capacity();
  size_t header_size = capacity_ - capacity_without_header;
  if (new_payload_size > capacity_without_header) {
    size_t new_capacity =
        std::max(static_cast<size_t>(capacity_without_header * kGrowthFactor),
                 new_payload_size) +
        header_size;
    Channel::AlignedBuffer new_data = MakeAlignedBuffer(new_capacity);
    memcpy(new_data.get(), data_.get(), capacity_);
    data_ = std::move(new_data);
    capacity_ = new_capacity;

    if (max_handles_ > 0) {
// We also need to update the cached extra header addresses in case the
// payload buffer has been relocated.
#if BUILDFLAG(IS_WIN)
      handles_ = reinterpret_cast<HandleEntry*>(mutable_extra_header());
#elif BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
      mach_ports_header_ =
          reinterpret_cast<MachPortsExtraHeader*>(mutable_extra_header());
#endif
    }
  }
  size_ = header_size + new_payload_size;
  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(size_));
  legacy_header()->num_bytes = static_cast<uint32_t>(size_);

  return true;
}

void ComplexMessage::SetHandles(std::vector<PlatformHandle> new_handles) {
  std::vector<PlatformHandleInTransit> handles;
  handles.reserve(new_handles.size());
  for (auto& h : new_handles) {
    handles.emplace_back(PlatformHandleInTransit(std::move(h)));
  }
  SetHandles(std::move(handles));
}

void ComplexMessage::SetHandles(
    std::vector<PlatformHandleInTransit> new_handles) {
  if (is_legacy_message()) {
    // Old semantics for ChromeOS and Android
    if (legacy_header()->num_handles == 0) {
      CHECK(new_handles.empty());
      return;
    }
    CHECK_EQ(new_handles.size(), legacy_header()->num_handles);
    std::swap(handle_vector_, new_handles);
    return;
  }

  if (max_handles_ == 0) {
    CHECK(new_handles.empty());
    return;
  }

  CHECK_LE(new_handles.size(), max_handles_);
  header()->num_handles = static_cast<uint16_t>(new_handles.size());
  std::swap(handle_vector_, new_handles);
#if BUILDFLAG(IS_WIN)
  memset(handles_, 0, extra_header_size());
  for (size_t i = 0; i < handle_vector_.size(); i++) {
    HANDLE handle = handle_vector_[i].remote_handle();
    if (handle == INVALID_HANDLE_VALUE)
      handle = handle_vector_[i].handle().GetHandle().Get();
    handles_[i].handle = base::win::HandleToUint32(handle);
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
  if (mach_ports_header_) {
    for (size_t i = 0; i < max_handles_; ++i) {
      mach_ports_header_->entries[i] = {0};
    }
    for (size_t i = 0; i < handle_vector_.size(); i++) {
      mach_ports_header_->entries[i].type =
          static_cast<uint8_t>(handle_vector_[i].handle().type());
    }
    mach_ports_header_->num_ports = handle_vector_.size();
  }
#endif
}

std::vector<PlatformHandleInTransit> ComplexMessage::TakeHandles() {
  return std::move(handle_vector_);
}

size_t ComplexMessage::NumHandlesForTransit() const {
  return handle_vector_.size();
}

// static
Channel::MessagePtr TrivialMessage::TryConstruct(size_t payload_size,
                                                 MessageType message_type) {
  const bool is_legacy_message = (message_type == MessageType::NORMAL_LEGACY);
  const size_t header_size =
      is_legacy_message ? sizeof(LegacyHeader) : sizeof(Header);

  size_t size = header_size + payload_size;
  if (size > kInternalCapacity) {
    return nullptr;
  }

  auto message = base::WrapUnique(new TrivialMessage);
  memset(message->mutable_data(), 0, sizeof(TrivialMessage::data_));

  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(size));
  message->size_ = size;
  message->legacy_header()->num_bytes = static_cast<uint32_t>(size);
  message->legacy_header()->message_type = message_type;

  if (!is_legacy_message) {
    DCHECK(base::IsValueInRangeForNumericType<uint16_t>(header_size));
    message->header()->num_header_bytes = static_cast<uint16_t>(header_size);
  }

  return base::WrapUnique<Channel::Message>(message.release());
}

size_t TrivialMessage::capacity() const {
  if (is_legacy_message())
    return kInternalCapacity - sizeof(LegacyHeader);
  return kInternalCapacity - header()->num_header_bytes;
}

bool TrivialMessage::ExtendPayload(size_t new_payload_size) {
  size_t capacity_without_header = capacity();
  size_t header_size = kInternalCapacity - capacity_without_header;
  size_t required_size = new_payload_size + header_size;
  if (required_size > kInternalCapacity) {
    return false;
  }

  // We can just bump up the internal size as it's less than the capacity.
  size_ = required_size;
  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(size_));
  legacy_header()->num_bytes = static_cast<uint32_t>(size_);
  return true;
}

void TrivialMessage::SetHandles(std::vector<PlatformHandle> new_handles) {
  CHECK(new_handles.empty());
}

void TrivialMessage::SetHandles(
    std::vector<PlatformHandleInTransit> new_handles) {
  CHECK(new_handles.empty());
}

std::vector<PlatformHandleInTransit> TrivialMessage::TakeHandles() {
  return std::vector<PlatformHandleInTransit>();
}

size_t TrivialMessage::NumHandlesForTransit() const {
  return 0;
}

// Helper class for managing a Channel's read buffer allocations. This maintains
// a single contiguous buffer with the layout:
//
//   [discarded bytes][occupied bytes][unoccupied bytes]
//
// The Reserve() method ensures that a certain capacity of unoccupied bytes are
// available. It does not claim that capacity and only allocates new capacity
// when strictly necessary.
//
// Claim() marks unoccupied bytes as occupied.
//
// Discard() marks occupied bytes as discarded, signifying that their contents
// can be forgotten or overwritten.
//
// Realign() moves occupied bytes to the front of the buffer so that those
// occupied bytes are properly aligned.
//
// The most common Channel behavior in practice should result in very few
// allocations and copies, as memory is claimed and discarded shortly after
// being reserved, and future reservations will immediately reuse discarded
// memory.
class Channel::ReadBuffer {
 public:
  ReadBuffer() {
    size_ = kReadBufferSize;
    data_ = MakeAlignedBuffer(size_);
  }

  ReadBuffer(const ReadBuffer&) = delete;
  ReadBuffer& operator=(const ReadBuffer&) = delete;

  ~ReadBuffer() { DCHECK(data_); }

  const char* occupied_bytes() const {
    return data_.get() + num_discarded_bytes_;
  }

  size_t num_occupied_bytes() const {
    return num_occupied_bytes_ - num_discarded_bytes_;
  }

  // Ensures the ReadBuffer has enough contiguous space allocated to hold
  // |num_bytes| more bytes; returns the address of the first available byte.
  char* Reserve(size_t num_bytes) {
    if (num_occupied_bytes_ + num_bytes > size_) {
      size_ = std::max(static_cast<size_t>(size_ * kGrowthFactor),
                       num_occupied_bytes_ + num_bytes);
      AlignedBuffer new_data = MakeAlignedBuffer(size_);
      memcpy(new_data.get(), data_.get(), num_occupied_bytes_);
      data_ = std::move(new_data);
    }

    return data_.get() + num_occupied_bytes_;
  }

  // Marks the first |num_bytes| unoccupied bytes as occupied.
  void Claim(size_t num_bytes) {
    DCHECK_LE(num_occupied_bytes_ + num_bytes, size_);
    num_occupied_bytes_ += num_bytes;
  }

  // Marks the first |num_bytes| occupied bytes as discarded. This may result in
  // shrinkage of the internal buffer, and it is not safe to assume the result
  // of a previous Reserve() call is still valid after this.
  void Discard(size_t num_bytes) {
    DCHECK_LE(num_discarded_bytes_ + num_bytes, num_occupied_bytes_);
    num_discarded_bytes_ += num_bytes;

    if (num_discarded_bytes_ == num_occupied_bytes_) {
      // We can just reuse the buffer from the beginning in this common case.
      num_discarded_bytes_ = 0;
      num_occupied_bytes_ = 0;
    }

    if (num_discarded_bytes_ > kMaxUnusedReadBufferCapacity) {
      // In the uncommon case that we have a lot of discarded data at the
      // front of the buffer, simply move remaining data to a smaller buffer.
      size_t num_preserved_bytes = num_occupied_bytes_ - num_discarded_bytes_;
      size_ = std::max(num_preserved_bytes, kReadBufferSize);
      AlignedBuffer new_data = MakeAlignedBuffer(size_);
      memcpy(new_data.get(), data_.get() + num_discarded_bytes_,
             num_preserved_bytes);
      data_ = std::move(new_data);
      num_discarded_bytes_ = 0;
      num_occupied_bytes_ = num_preserved_bytes;
    }

    if (num_occupied_bytes_ == 0 && size_ > kMaxUnusedReadBufferCapacity) {
      // Opportunistically shrink the read buffer back down to a small size if
      // it's grown very large. We only do this if there are no remaining
      // unconsumed bytes in the buffer to avoid copies in most the common
      // cases.
      size_ = kMaxUnusedReadBufferCapacity;
      data_ = MakeAlignedBuffer(size_);
    }
  }

  void Realign() {
    size_t num_bytes = num_occupied_bytes();
    memmove(data_.get(), occupied_bytes(), num_bytes);
    num_discarded_bytes_ = 0;
    num_occupied_bytes_ = num_bytes;
  }

 private:
  AlignedBuffer data_;

  // The total size of the allocated buffer.
  size_t size_ = 0;

  // The number of discarded bytes at the beginning of the allocated buffer.
  size_t num_discarded_bytes_ = 0;

  // The total number of occupied bytes, including discarded bytes.
  size_t num_occupied_bytes_ = 0;
};

bool Channel::Delegate::IsIpczTransport() const {
  return false;
}

void Channel::Delegate::OnChannelDestroyed() {}

Channel::Channel(Delegate* delegate,
                 HandlePolicy handle_policy,
                 DispatchBufferPolicy buffer_policy)
    : is_for_ipcz_(delegate ? delegate->IsIpczTransport() : false),
      delegate_(delegate),
      handle_policy_(handle_policy),
      read_buffer_(buffer_policy == DispatchBufferPolicy::kManaged
                       ? new ReadBuffer
                       : nullptr) {}

Channel::~Channel() {
  if (is_for_ipcz()) {
    DCHECK(delegate_);
    delegate_->OnChannelDestroyed();
  }
}

// static
scoped_refptr<Channel> Channel::CreateForIpczDriver(
    Delegate* delegate,
    PlatformChannelEndpoint endpoint,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
#if BUILDFLAG(IS_NACL)
  return nullptr;
#else
  return Create(delegate, ConnectionParams{std::move(endpoint)},
                HandlePolicy::kAcceptHandles, std::move(io_task_runner));
#endif
}

void Channel::ShutDown() {
  ShutDownImpl();
  if (!is_for_ipcz()) {
    // When Channel is used for an ipcz transport, we leave `delegate_` intact
    // so the Channel can notify once it's finally being destroyed.
    delegate_ = nullptr;
  }
}

char* Channel::GetReadBuffer(size_t* buffer_capacity) {
  DCHECK(read_buffer_);
  size_t required_capacity = *buffer_capacity;
  if (!required_capacity)
    required_capacity = kReadBufferSize;

  *buffer_capacity = required_capacity;
  return read_buffer_->Reserve(required_capacity);
}

bool Channel::OnReadComplete(size_t bytes_read, size_t* next_read_size_hint) {
  DCHECK(read_buffer_);
  *next_read_size_hint = kReadBufferSize;
  read_buffer_->Claim(bytes_read);

  const size_t header_size = is_for_ipcz_ ? sizeof(Message::IpczHeader)
                                          : sizeof(Message::LegacyHeader);
  while (read_buffer_->num_occupied_bytes() >= header_size) {
    // Ensure the occupied data is properly aligned. If it isn't, a SIGBUS could
    // happen on architectures that don't allow misaligned words access (i.e.
    // anything other than x86). Only re-align when necessary to avoid copies.
    if (!IsAlignedForChannelMessage(
            reinterpret_cast<uintptr_t>(read_buffer_->occupied_bytes()))) {
      read_buffer_->Realign();
    }

    DispatchResult result =
        TryDispatchMessage(base::make_span(read_buffer_->occupied_bytes(),
                                           read_buffer_->num_occupied_bytes()),
                           next_read_size_hint);
    if (result == DispatchResult::kOK) {
      if (ShouldRecordSubsampledHistograms()) {
        LogHistogramForIPCMetrics(MessageType::kReceive);
      }
      read_buffer_->Discard(*next_read_size_hint);
      *next_read_size_hint = 0;
    } else if (result == DispatchResult::kNotEnoughData) {
      return true;
    } else if (result == DispatchResult::kMissingHandles) {
      break;
    } else if (result == DispatchResult::kError) {
      return false;
    }
  }
  return true;
}

Channel::DispatchResult Channel::TryDispatchMessage(
    base::span<const char> buffer,
    size_t* size_hint) {
  return TryDispatchMessage(buffer, std::nullopt, size_hint);
}

Channel::DispatchResult Channel::TryDispatchMessage(
    base::span<const char> buffer,
    std::optional<std::vector<PlatformHandle>> received_handles,
    size_t* size_hint) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("toplevel.ipc"),
              "Mojo dispatch message");
  if (is_for_ipcz_) {
    // This has already been validated.
    DCHECK_GE(buffer.size(), Message::kMinIpczHeaderSize);

    const auto& header =
        *reinterpret_cast<const Message::IpczHeader*>(buffer.data());
    const size_t header_size = header.size;
    const size_t num_bytes = header.num_bytes;
    const size_t num_handles = header.num_handles;
    if (header_size < Message::kMinIpczHeaderSize || num_bytes < header_size) {
      return DispatchResult::kError;
    }

    if (buffer.size() < num_bytes) {
      *size_hint = num_bytes - buffer.size();
      return DispatchResult::kNotEnoughData;
    }

    std::vector<PlatformHandle> handles;
    if (num_handles > 0) {
      if (handle_policy_ == HandlePolicy::kRejectHandles) {
        return DispatchResult::kError;
      }

      if (received_handles) {
        handles = std::move(*received_handles);
      } else if (!GetReadPlatformHandlesForIpcz(num_handles, handles)) {
        return DispatchResult::kError;
      }

      if (handles.size() < num_handles) {
        return DispatchResult::kMissingHandles;
      }
    }

    if (ShouldRecordSubsampledHistograms() && Message::IsAtLeastV2(header)) {
      base::TimeTicks creation_time =
          base::TimeTicks() +
          base::Microseconds(header.v2.creation_timeticks_us);
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Mojo.Channel.WriteToReadLatencyUs",
          base::TimeTicks::Now() - creation_time, base::Microseconds(1),
          base::Seconds(1), 100);
    }

    auto data = buffer.first(num_bytes).subspan(header_size);
    delegate_->OnChannelMessage(data.data(), data.size(), std::move(handles));
    *size_hint = num_bytes;
    return DispatchResult::kOK;
  }

  // We have at least enough data available for a LegacyHeader.
  const Message::LegacyHeader* legacy_header =
      reinterpret_cast<const Message::LegacyHeader*>(buffer.data());

  if (legacy_header->num_bytes < sizeof(Message::LegacyHeader)) {
    LOG(ERROR) << "Invalid message size: " << legacy_header->num_bytes;
    return DispatchResult::kError;
  }

  if (buffer.size() < legacy_header->num_bytes) {
    // Not enough data available to read the full message. Hint to the
    // implementation that it should try reading the full size of the message.
    *size_hint = legacy_header->num_bytes - buffer.size();
    return DispatchResult::kNotEnoughData;
  }

  const Message::Header* header = nullptr;
  if (legacy_header->message_type != Message::MessageType::NORMAL_LEGACY) {
    header = reinterpret_cast<const Message::Header*>(legacy_header);
  }

  size_t extra_header_size = 0;
  const void* extra_header = nullptr;
  size_t payload_size = 0;
  void* payload = nullptr;
  if (header) {
    if (header->num_header_bytes < sizeof(Message::Header) ||
        header->num_header_bytes > header->num_bytes) {
      LOG(ERROR) << "Invalid message header size: " << header->num_header_bytes;
      return DispatchResult::kError;
    }
    extra_header_size = header->num_header_bytes - sizeof(Message::Header);
    extra_header = extra_header_size ? header + 1 : nullptr;
    payload_size = header->num_bytes - header->num_header_bytes;
    payload =
        payload_size
            ? reinterpret_cast<Message::Header*>(
                  const_cast<char*>(buffer.data()) + header->num_header_bytes)
            : nullptr;
  } else {
    payload_size = legacy_header->num_bytes - sizeof(Message::LegacyHeader);
    payload = payload_size
                  ? const_cast<Message::LegacyHeader*>(&legacy_header[1])
                  : nullptr;
  }

  const uint16_t num_handles =
      header ? header->num_handles : legacy_header->num_handles;
  std::vector<PlatformHandle> handles;
  bool deferred = false;
  if (num_handles > 0) {
    if (handle_policy_ == HandlePolicy::kRejectHandles) {
      return DispatchResult::kError;
    }

    if (received_handles) {
      handles = std::move(*received_handles);
    } else if (!GetReadPlatformHandles(payload, payload_size, num_handles,
                                       extra_header, extra_header_size,
                                       &handles, &deferred)) {
      return DispatchResult::kError;
    }

    if (handles.size() < num_handles) {
      // Not enough handles available for this message.
      return DispatchResult::kMissingHandles;
    }
  }

  // We've got a complete message! Dispatch it and try another.
  if (legacy_header->message_type != Message::MessageType::NORMAL_LEGACY &&
      legacy_header->message_type != Message::MessageType::NORMAL) {
    DCHECK(!deferred);
    if (!OnControlMessage(legacy_header->message_type, payload, payload_size,
                          std::move(handles))) {
      return DispatchResult::kError;
    }
  } else if (!deferred && delegate_) {
    delegate_->OnChannelMessage(payload, payload_size, std::move(handles));
  }

  *size_hint = legacy_header->num_bytes;
  return DispatchResult::kOK;
}

void Channel::OnError(Error error) {
  if (delegate_)
    delegate_->OnChannelError(error);
}

bool Channel::OnControlMessage(Message::MessageType message_type,
                               const void* payload,
                               size_t payload_size,
                               std::vector<PlatformHandle> handles) {
  return false;
}

// static
void Channel::LogHistogramForIPCMetrics(MessageType type) {
  if (type == MessageType::kSent) {
    UMA_HISTOGRAM_ENUMERATION(
        "Mojo.Channel.WriteSendMessageProcessType",
        base::CurrentProcess::GetInstance().GetShortType({}));
  }
  if (type == MessageType::kReceive) {
    UMA_HISTOGRAM_ENUMERATION(
        "Mojo.Channel.WriteReceiveMessageProcessType",
        base::CurrentProcess::GetInstance().GetShortType({}));
  }
}

// Currently only Non-nacl CrOs, Linux, and Android support upgrades.
#if BUILDFLAG(IS_NACL) || (!(BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || \
                             BUILDFLAG(IS_ANDROID)))
// static
MOJO_SYSTEM_IMPL_EXPORT bool Channel::SupportsChannelUpgrade() {
  return false;
}

MOJO_SYSTEM_IMPL_EXPORT void Channel::OfferChannelUpgrade() {
  NOTREACHED();
}
#endif

bool Channel::ShouldRecordSubsampledHistograms() {
  base::AutoLock hold(lock_);
  return sub_sampler_.ShouldSample(0.001);
}

}  // namespace mojo::core
