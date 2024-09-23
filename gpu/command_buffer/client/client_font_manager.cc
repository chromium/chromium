// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/client_font_manager.h"

#include <bit>
#include <type_traits>

#include "base/bits.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"

namespace gpu {
namespace raster {

namespace {

class Serializer {
 public:
  Serializer(char* memory, uint32_t memory_size)
      : memory_(memory), memory_size_(memory_size) {}
  ~Serializer() = default;

  template <typename T>
  void Write(const T* val) {
    static_assert(std::is_trivially_copyable_v<T>);
    WriteData(val, sizeof(T), alignof(T));
  }

  void WriteData(const void* input, uint32_t bytes, size_t alignment) {
    AlignMemory(bytes, alignment);
    if (bytes == 0)
      return;

    memcpy(memory_, input, bytes);
    memory_ += bytes;
    bytes_written_ += bytes;
  }

 private:
  void AlignMemory(uint32_t size, size_t alignment) {
    // Due to the math below, alignment must be a power of two.
    DCHECK(std::has_single_bit(alignment));

    size_t memory = reinterpret_cast<size_t>(memory_.get());
    size_t padding = base::bits::AlignUp(memory, alignment) - memory;
    DCHECK_LE(bytes_written_ + size + padding, memory_size_);

    memory_ += padding;
    bytes_written_ += padding;
  }

  raw_ptr<char, AllowPtrArithmetic> memory_ = nullptr;
  uint32_t memory_size_ = 0u;
  uint32_t bytes_written_ = 0u;
};

}  // namespace

ClientFontManager::ClientFontManager(Client* client,
                                     CommandBuffer* command_buffer)
    : client_(client), command_buffer_(command_buffer), strike_server_(this) {}

ClientFontManager::~ClientFontManager() = default;

SkDiscardableHandleId ClientFontManager::createHandle() {
  auto client_handle =
      client_discardable_manager_.CreateHandle(command_buffer_);
  if (client_handle.is_null())
    return kInvalidSkDiscardableHandleId;

  SkDiscardableHandleId handle_id = ++last_allocated_handle_id_;
  discardable_handle_map_[handle_id] = client_handle;
  // Handles start with a ref-count.
  locked_handles_.insert(handle_id);
  return handle_id;
}

bool ClientFontManager::lockHandle(SkDiscardableHandleId handle_id) {
  // Already locked.
  if (locked_handles_.find(handle_id) != locked_handles_.end())
    return true;

  auto it = discardable_handle_map_.find(handle_id);
  if (it == discardable_handle_map_.end())
    return false;

  bool locked = client_discardable_manager_.LockHandle(it->second);
  if (locked) {
    locked_handles_.insert(handle_id);
    return true;
  }

  discardable_handle_map_.erase(it);
  return false;
}

bool ClientFontManager::isHandleDeleted(SkDiscardableHandleId handle_id) {
  auto it = discardable_handle_map_.find(handle_id);
  if (it == discardable_handle_map_.end())
    return true;

  if (client_discardable_manager_.HandleIsDeleted(it->second)) {
    discardable_handle_map_.erase(it);
    return true;
  }

  return false;
}

void ClientFontManager::Serialize() {
  // TODO(khushalsagar): May be skia can track the size required so we avoid
  // this copy.
  std::vector<uint8_t> strike_data;
  strike_server_.writeStrikeData(&strike_data);

  const uint32_t num_handles_created =
      last_allocated_handle_id_ - last_serialized_handle_id_;
  if (strike_data.size() == 0u && num_handles_created == 0u &&
      locked_handles_.size() == 0u) {
    // No font data to serialize.
    return;
  }

  // Size required for serialization.
  base::CheckedNumeric<uint32_t> checked_bytes_required = 0;
  // Skia data size.
  checked_bytes_required += sizeof(uint32_t) + alignof(uint32_t) + 16;
  checked_bytes_required += strike_data.size();

  // num of handles created + SerializableHandles.
  checked_bytes_required +=
      sizeof(uint32_t) + alignof(uint32_t) + alignof(SerializableSkiaHandle);
  checked_bytes_required +=
      base::CheckMul(num_handles_created, sizeof(SerializableSkiaHandle));

  // num of handles locked + DiscardableHandleIds.
  checked_bytes_required +=
      sizeof(uint32_t) + alignof(uint32_t) + alignof(SkDiscardableHandleId);
  checked_bytes_required +=
      base::CheckMul(locked_handles_.size(), sizeof(SkDiscardableHandleId));

  uint32_t bytes_required = 0;
  if (!checked_bytes_required.AssignIfValid(&bytes_required)) {
    DLOG(FATAL) << "ClientFontManager::Serialize: font buffer overflow";
    return;
  }

  // Allocate memory.
  void* memory = client_->MapFontBuffer(bytes_required);
  if (!memory) {
    // We are likely in a context loss situation if mapped memory allocation
    // for font buffer failed.
    return;
  }
  Serializer serializer(reinterpret_cast<char*>(memory), bytes_required);

  // Serialize all new handles.
  serializer.Write<uint32_t>(&num_handles_created);
  for (SkDiscardableHandleId handle_id = last_serialized_handle_id_ + 1;
       handle_id <= last_allocated_handle_id_; handle_id++) {
    auto it = discardable_handle_map_.find(handle_id);
    CHECK(it != discardable_handle_map_.end(), base::NotFatalUntil::M130);

    // We must have a valid |client_handle| here since all new handles are
    // currently in locked state.
    auto client_handle = client_discardable_manager_.GetHandle(it->second);
    DCHECK(client_handle.IsValid());
    SerializableSkiaHandle handle(handle_id, client_handle.shm_id(),
                                  client_handle.byte_offset());
    serializer.Write<SerializableSkiaHandle>(&handle);
  }

  // Serialize all locked handle ids, so the raster unlocks them when done.
  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(locked_handles_.size()));
  const uint32_t num_locked_handles = locked_handles_.size();
  serializer.Write<uint32_t>(&num_locked_handles);
  for (auto handle_id : locked_handles_)
    serializer.Write<SkDiscardableHandleId>(&handle_id);

  // Serialize skia data.
  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(strike_data.size()));
  const uint32_t skia_data_size = strike_data.size();
  serializer.Write<uint32_t>(&skia_data_size);
  serializer.WriteData(strike_data.data(), strike_data.size(), 16);

  // Reset all state for what has been serialized.
  last_serialized_handle_id_ = last_allocated_handle_id_;
  locked_handles_.clear();
  return;
}

}  // namespace raster
}  // namespace gpu
