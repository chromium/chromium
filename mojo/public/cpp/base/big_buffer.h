// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_BIG_BUFFER_H_
#define MOJO_PUBLIC_CPP_BASE_BIG_BUFFER_H_

#include <stdint.h>

#include <optional>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/containers/checked_iterators.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/system/buffer.h"

namespace mojo_base {

class BigBuffer;
class BigBufferView;

namespace internal {

// Internal helper used by BigBuffer when backed by shared memory.
class COMPONENT_EXPORT(MOJO_BASE) BigBufferSharedMemoryRegion {
 public:
  BigBufferSharedMemoryRegion();
  BigBufferSharedMemoryRegion(mojo::ScopedSharedBufferHandle buffer_handle,
                              size_t size);
  BigBufferSharedMemoryRegion(BigBufferSharedMemoryRegion&& other);

  BigBufferSharedMemoryRegion(const BigBufferSharedMemoryRegion&) = delete;
  BigBufferSharedMemoryRegion& operator=(const BigBufferSharedMemoryRegion&) =
      delete;

  ~BigBufferSharedMemoryRegion();

  BigBufferSharedMemoryRegion& operator=(BigBufferSharedMemoryRegion&& other);

  void* memory() const { return buffer_mapping_.get(); }

  size_t size() const { return size_; }
  mojo::ScopedSharedBufferHandle TakeBufferHandle();

 private:
  friend class mojo_base::BigBuffer;
  friend class mojo_base::BigBufferView;

  size_t size_;
  mojo::ScopedSharedBufferHandle buffer_handle_;
  mojo::ScopedSharedBufferMapping buffer_mapping_;
};

}  // namespace internal

// BigBuffer represents a potentially large sequence of bytes. When passed over
// mojom (as a mojo_base::mojom::BigBuffer union), it may serialize as either an
// inlined array of bytes or as a shared buffer handle with the payload copied
// into the corresponding shared memory region. This makes it easier to send
// payloads of varying and unbounded size over IPC without fear of hitting
// message size limits.
//
// A BigBuffer may be (implicitly) constructed over any span of bytes, and it
// exposes simple |data()| and |size()| accessors akin to what common container
// types provide. Users do not need to be concerned with the actual backing
// storage used to implement this interface.
//
// SECURITY NOTE: When shmem is backing the message, it may be writable in the
// sending process while being read in the receiving process. If a BigBuffer is
// received from an untrustworthy process, you should make a copy of the data
// before processing it to avoid time-of-check time-of-use (TOCTOU) bugs.
// The |size()| of the data cannot be manipulated.
class COMPONENT_EXPORT(MOJO_BASE) BigBuffer {
 public:
  using value_type = uint8_t;
  using iterator = base::CheckedContiguousIterator<uint8_t>;
  using const_iterator = base::CheckedContiguousIterator<const uint8_t>;

  static constexpr size_t kMaxInlineBytes = 64 * 1024;

  enum class StorageType {
    kBytes,
    kSharedMemory,
    kInvalidBuffer,
  };

  // Defaults to empty kBytes storage.
  BigBuffer();

  // Constructs a BigBuffer over an existing span of bytes. Intentionally
  // implicit for convenience. Always copies the contents of |data| into some
  // internal storage.
  BigBuffer(base::span<const uint8_t> data);

  // Constructs a BigBuffer from an existing shared memory region. Not intended
  // for general-purpose use.
  explicit BigBuffer(internal::BigBufferSharedMemoryRegion shared_memory);

  // Constructs a BigBuffer with the given size. The contents of buffer memory
  // are uninitialized. Buffers constructed this way must be filled completely
  // before transfer to avoid leaking information to less privileged processes.
  explicit BigBuffer(size_t size);

  BigBuffer(BigBuffer&& other);
  BigBuffer& operator=(BigBuffer&& other);

  ~BigBuffer();

  // Returns a new BigBuffer containing a copy of this BigBuffer's contents.
  // Note that the new BigBuffer may not necessarily have the same backing
  // storage type as the original one, only the same contents.
  BigBuffer Clone() const;

  // Returns a pointer to the data stored by this BigBuffer, regardless of
  // backing storage type. Prefer to use `base::span(big_buffer)` instead, or
  // the implicit conversion to `base::span`.
  uint8_t* data() { return const_cast<uint8_t*>(std::as_const(*this).data()); }
  const uint8_t* data() const;

  // Returns the size of the data stored by this BigBuffer, regardless of
  // backing storage type.
  size_t size() const;

  StorageType storage_type() const { return storage_type_; }

  // WARNING: This method does not work for buffers backed by shared memory. To
  // get a span independent of the storage type, write `base::span(big_buffer)`,
  // or rely on the implicit conversion.
  base::span<const uint8_t> byte_span() const {
    CHECK_EQ(storage_type_, StorageType::kBytes);
    return bytes_.as_span();
  }

  internal::BigBufferSharedMemoryRegion& shared_memory() {
    CHECK_EQ(storage_type_, StorageType::kSharedMemory);
    return shared_memory_.value();
  }

  iterator begin() {
    uint8_t* const ptr = data();
    // SAFETY: If this is an invalid buffer, `ptr` is null and `size()` is zero,
    // which results in a well-defined (null) result. Otherwise, the underlying
    // storage (`bytes_` or `shared_memory_`) guarantees that `ptr` points to at
    // least `size()` bytes.
    return UNSAFE_BUFFERS(iterator(ptr, ptr + size()));
  }

  const_iterator begin() const {
    const uint8_t* const ptr = data();
    // SAFETY: As in the non-const version above.
    return UNSAFE_BUFFERS(const_iterator(ptr, ptr + size()));
  }

  const_iterator cbegin() const { return begin(); }

  iterator end() {
    uint8_t* const ptr = data();
    const size_t len = size();
    // SAFETY: As in `begin()` above.
    return UNSAFE_BUFFERS(iterator(ptr, ptr + len, ptr + len));
  }

  const_iterator end() const {
    const uint8_t* const ptr = data();
    const size_t len = size();
    // SAFETY: As in the non-const version above.
    return UNSAFE_BUFFERS(const_iterator(ptr, ptr + len, ptr + len));
  }

  const_iterator cend() const { return end(); }

 private:
  friend class BigBufferView;

  StorageType storage_type_ = StorageType::kBytes;
  base::HeapArray<uint8_t> bytes_;
  std::optional<internal::BigBufferSharedMemoryRegion> shared_memory_;
};

// Similar to BigBuffer, but doesn't *necessarily* own the buffer storage.
// Namely, if constructed over a small enough span of memory, it will simply
// retain a reference to that memory. This is generally only safe to use for
// serialization and deserialization.
class COMPONENT_EXPORT(MOJO_BASE) BigBufferView {
 public:
  BigBufferView();
  BigBufferView(BigBufferView&& other);

  // Constructs a BigBufferView over |bytes|. If |bytes| is large enough, this
  // will allocate shared memory and copy the contents there. Otherwise this
  // will retain an unsafe reference to |bytes| and must therefore not outlive
  // |bytes|.
  explicit BigBufferView(base::span<const uint8_t> bytes);

  BigBufferView(const BigBufferView&) = delete;
  BigBufferView& operator=(const BigBufferView&) = delete;

  ~BigBufferView();

  BigBufferView& operator=(BigBufferView&& other);

  base::span<const uint8_t> data() const;

  // Explicitly retains a reference to |bytes| as the backing storage for this
  // view. Does not copy and therefore |bytes| must remain valid throughout the
  // view's lifetime. Used for deserialization.
  void SetBytes(base::span<const uint8_t> bytes);

  // Explictly adopts |shared_memory| as the backing storage for this view. Used
  // for deserialization.
  void SetSharedMemory(internal::BigBufferSharedMemoryRegion shared_memory);

  // Converts to a BigBuffer which owns the viewed data. May have to copy data.
  [[nodiscard]] static BigBuffer ToBigBuffer(BigBufferView view);

  BigBuffer::StorageType storage_type() const { return storage_type_; }

  // WARNING: This method does not work for buffers backed by shared memory. To
  // get a span independent of the storage type, use `data()`.
  base::span<const uint8_t> bytes() const {
    DCHECK_EQ(storage_type_, BigBuffer::StorageType::kBytes);
    return bytes_;
  }

  internal::BigBufferSharedMemoryRegion& shared_memory() {
    DCHECK_EQ(storage_type_, BigBuffer::StorageType::kSharedMemory);
    return shared_memory_.value();
  }

  static BigBufferView CreateInvalidForTest();

 private:
  BigBuffer::StorageType storage_type_ = BigBuffer::StorageType::kBytes;
  base::raw_span<const uint8_t> bytes_;
  std::optional<internal::BigBufferSharedMemoryRegion> shared_memory_;
};

}  // namespace mojo_base

#endif  // MOJO_PUBLIC_CPP_BASE_BIG_BUFFER_H_
