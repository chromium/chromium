// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_BIG_BUFFER_H_
#define MOJO_PUBLIC_CPP_BASE_BIG_BUFFER_H_

#include <cstdint>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/optional.h"
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

  DISALLOW_COPY_AND_ASSIGN(BigBufferSharedMemoryRegion);
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
class COMPONENT_EXPORT(MOJO_BASE) BigBuffer {
 public:
  static constexpr size_t kMaxInlineBytes = 64 * 1024;

  enum class StorageType {
    kBytes,
    kSharedMemory,
    kInvalidBuffer,
  };

  // Defaults to empty kBytes storage.
  BigBuffer();
  BigBuffer(BigBuffer&& other);

  // Constructs a BigBuffer over an existing span of bytes. Intentionally
  // implicit for convenience. Always copies the contents of |data| into some
  // internal storage.
  BigBuffer(base::span<const uint8_t> data);

  // Helper for implicit conversion from byte vectors.
  BigBuffer(const std::vector<uint8_t>& data);

  // Constructs a BigBuffer from an existing shared memory region. Not intended
  // for general-purpose use.
  explicit BigBuffer(internal::BigBufferSharedMemoryRegion shared_memory);

  // Constructs a BigBuffer with the given size. The contents of buffer memory
  // are uninitialized. Buffers constructed this way must be filled completely
  // before transfer to avoid leaking information to less privileged processes.
  explicit BigBuffer(size_t size);

  ~BigBuffer();

  BigBuffer& operator=(BigBuffer&& other);

  // Returns a pointer to the data stored by this BigBuffer, regardless of
  // backing storage type.
  uint8_t* data();
  const uint8_t* data() const;

  // Returns the size of the data stored by this BigBuffer, regardless of
  // backing storage type.
  size_t size() const;

  StorageType storage_type() const { return storage_type_; }

  base::span<const uint8_t> byte_span() const {
    DCHECK_EQ(storage_type_, StorageType::kBytes);
    return base::make_span(bytes_.get(), bytes_size_);
  }

  internal::BigBufferSharedMemoryRegion& shared_memory() {
    DCHECK_EQ(storage_type_, StorageType::kSharedMemory);
    return shared_memory_.value();
  }

 private:
  friend class BigBufferView;

  StorageType storage_type_;
  std::unique_ptr<uint8_t[]> bytes_;
  size_t bytes_size_;
  base::Optional<internal::BigBufferSharedMemoryRegion> shared_memory_;

  DISALLOW_COPY_AND_ASSIGN(BigBuffer);
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
  static BigBuffer ToBigBuffer(BigBufferView view) WARN_UNUSED_RESULT;

  BigBuffer::StorageType storage_type() const { return storage_type_; }

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
  BigBuffer::StorageType storage_type_;
  base::span<const uint8_t> bytes_;
  base::Optional<internal::BigBufferSharedMemoryRegion> shared_memory_;

  DISALLOW_COPY_AND_ASSIGN(BigBufferView);
};

}  // namespace mojo_base

#endif  // MOJO_PUBLIC_CPP_BASE_BIG_BUFFER_H_
