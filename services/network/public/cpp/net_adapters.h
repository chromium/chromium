// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NET_ADAPTERS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NET_ADAPTERS_H_

#include <stddef.h>
#include <stdint.h>

#include "base/component_export.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/io_buffer.h"

namespace network {

// These adapters are used to transfer data between a Mojo pipe and the net
// library.
//
//   Mojo pipe              Data flow    Network library
//   ----------------------------------------------------------
//   MojoToNetPendingBuffer    --->      MojoToNetIOBuffer
//   NetToMojoPendingBuffer    <---      NetToMojoIOBuffer
//
// While the operation is in progress, the Mojo-side objects keep ownership
// of the Mojo pipe, which in turn is kept alive by the IOBuffer. This allows
// the request to potentially outlive the object managing the translation.
// Mojo side of a Net -> Mojo copy. By default (`kStageNetToMojoBuffer`),
// writes are staged in a heap-allocated `StagingBuffer` and copied to the Mojo
// data pipe inside `Complete()`.
class COMPONENT_EXPORT(NETWORK_CPP) NetToMojoPendingBuffer
    : public base::RefCountedThreadSafe<NetToMojoPendingBuffer> {
  class StagingBuffer;

 public:
  // Single-slot cache that recycles a uniquely-owned `StagingBuffer` across
  // sequential BeginWrite() / Complete() cycles on the same stream.
  //
  // Refcount lifecycle for recycling:
  // - While a read is active, both `NetToMojoPendingBuffer` and any live
  //   `NetToMojoIOBuffer` hold references to `StagingBuffer` (refcount >= 2).
  // - For `Complete()` to return the buffer to this pool via
  //   `PutIfUniquelyOwned()`, callers and synchronous readers must release
  //   their `NetToMojoIOBuffer` references before `Complete()` runs so that
  //   `StagingBuffer`'s refcount drops back to 1 (`HasOneRef()`).
  // - If an async writer (e.g. `disk_cache`) still holds a `NetToMojoIOBuffer`
  //   when `Complete()` runs, `HasOneRef()` is false, recycling is skipped,
  //   and the buffer remains alive exclusively for that writer until its
  //   refcount reaches 0.
  class COMPONENT_EXPORT(NETWORK_CPP) StagingBufferPool {
   public:
    StagingBufferPool();
    StagingBufferPool(const StagingBufferPool&) = delete;
    StagingBufferPool& operator=(const StagingBufferPool&) = delete;
    ~StagingBufferPool();

    void Reset();
    bool HasBufferForTesting() const { return buffer_ != nullptr; }

   private:
    friend class NetToMojoPendingBuffer;

    scoped_refptr<StagingBuffer> Take(size_t min_size);
    void PutIfUniquelyOwned(scoped_refptr<StagingBuffer> buffer);
    base::WeakPtr<StagingBufferPool> GetWeakPtr() {
      return weak_factory_.GetWeakPtr();
    }

    scoped_refptr<StagingBuffer> buffer_;
    base::WeakPtrFactory<StagingBufferPool> weak_factory_{this};
  };

  NetToMojoPendingBuffer(const NetToMojoPendingBuffer&) = delete;
  NetToMojoPendingBuffer& operator=(const NetToMojoPendingBuffer&) = delete;

  // Begins a two-phase write to the data pipe.
  //
  // On success, MOJO_RESULT_OK will be returned. The ownership of the given
  // producer handle will be transferred to the new NetToMojoPendingBuffer that
  // will be placed into *pending. If `buffer_pool` is non-null, a cached
  // staging buffer of sufficient size will be reused instead of allocating, and
  // `Complete()` will return the staging buffer to `*buffer_pool` if uniquely
  // owned.
  //
  // On failure or MOJO_RESULT_SHOULD_WAIT, there will be no change to the
  // handle (or `*buffer_pool`), and *pending will be nulled out.
  static MojoResult BeginWrite(mojo::ScopedDataPipeProducerHandle* handle,
                               scoped_refptr<NetToMojoPendingBuffer>* pending,
                               StagingBufferPool* buffer_pool = nullptr);

  // Called to indicate the buffer is done being written to. Copies `num_bytes`
  // from the staging buffer (if staged) into the Mojo data pipe, completes the
  // two-phase write, returns `staging_buffer_` to its `StagingBufferPool` (if
  // any and uniquely owned), and passes ownership of the pipe back to the
  // caller.
  mojo::ScopedDataPipeProducerHandle Complete(uint32_t num_bytes);

  char* buffer() { return buffer_.data(); }
  uint32_t size() const { return static_cast<uint32_t>(buffer_.size()); }

  // Satisfy the requirements of `std::ranges::contiguous_range`. This allows
  // implicit conversion to a `base::span`.
  char* data() { return buffer_.data(); }
  auto begin() { return buffer_.begin(); }
  auto end() { return buffer_.end(); }

 private:
  friend class base::RefCountedThreadSafe<NetToMojoPendingBuffer>;
  friend class NetToMojoIOBuffer;

  // Ref-counted heap buffer used to stage writes before copying into the Mojo
  // data pipe ring buffer. Reference-counted separately from
  // `NetToMojoPendingBuffer` so that:
  // 1. `NetToMojoPendingBuffer`'s refcount can drop to 0 immediately on error
  //    or destruction (running `~NetToMojoPendingBuffer()` to end the two-phase
  //    Mojo write with 0 bytes and close the producer handle) even while a
  //    background thread (e.g. `disk_cache`) still holds a `NetToMojoIOBuffer`.
  // 2. Uniquely-owned buffers (refcount == 1 at `Complete()`) can be recycled
  //    via `StagingBufferPool`, whereas buffers still referenced by an async
  //    `NetToMojoIOBuffer` (refcount >= 2) are left untouched until the last
  //    holder drops its reference to 0.
  class COMPONENT_EXPORT(NETWORK_CPP) StagingBuffer
      : public base::RefCountedThreadSafe<StagingBuffer> {
   public:
    explicit StagingBuffer(size_t size);
    StagingBuffer(const StagingBuffer&) = delete;
    StagingBuffer& operator=(const StagingBuffer&) = delete;

    base::span<char> span() { return buffer_.as_span(); }

   private:
    friend class base::RefCountedThreadSafe<StagingBuffer>;
    ~StagingBuffer();

    base::HeapArray<char> buffer_;
  };

  // Takes ownership of the handle.
  NetToMojoPendingBuffer(mojo::ScopedDataPipeProducerHandle handle,
                         base::span<uint8_t> mojo_buffer,
                         scoped_refptr<StagingBuffer> staging_buffer,
                         base::WeakPtr<StagingBufferPool> buffer_pool);
  ~NetToMojoPendingBuffer();

  const scoped_refptr<StagingBuffer>& staging_buffer() const {
    return staging_buffer_;
  }

  mojo::ScopedDataPipeProducerHandle handle_;
  // Underlying Mojo two-phase write buffer.
  RAW_PTR_EXCLUSION base::span<uint8_t> mojo_buffer_;
  scoped_refptr<StagingBuffer> staging_buffer_;
  base::WeakPtr<StagingBufferPool> buffer_pool_;
  // `buffer_` points to `staging_buffer_` when staged, or directly to
  // `mojo_buffer_` otherwise. Not a `raw_span<...>` for performance reasons
  // and because direct Mojo mappings come from `mmap`/`MapViewOfFile`.
  // TODO(367764863) Rewrite to base::raw_span.
  RAW_PTR_EXCLUSION base::span<char> buffer_;
};

// Net side of a Net -> Mojo copy. The data will be read from the network and
// copied into the buffer associated with the pending mojo write.
class COMPONENT_EXPORT(NETWORK_CPP) NetToMojoIOBuffer
    : public net::WrappedIOBuffer {
 public:
  // If |offset| is specified then the memory buffer passed to the Net layer
  // will be offset by that many bytes.
  explicit NetToMojoIOBuffer(
      scoped_refptr<NetToMojoPendingBuffer> pending_buffer,
      size_t offset = 0);

  NetToMojoIOBuffer(const NetToMojoIOBuffer&) = delete;
  NetToMojoIOBuffer& operator=(const NetToMojoIOBuffer&) = delete;

 private:
  ~NetToMojoIOBuffer() override;
  // Declared before `pending_buffer_` so `pending_buffer_` can be left null
  // when `staging_buffer_` is non-null.
  //
  // Refcount coordination:
  // - When staged (`staging_buffer_ != nullptr`), this IOBuffer retains only
  //   `staging_buffer_` and leaves `pending_buffer_` null. As a result, the
  //   stream owner (e.g. `URLLoader::pending_write_`) holds the sole reference
  //   to `NetToMojoPendingBuffer`: dropping it on an error or destructor path
  //   immediately drives `NetToMojoPendingBuffer`'s refcount to 0 so
  //   `~NetToMojoPendingBuffer()` calls `EndWriteData(0)` and closes the
  //   producer handle, while any background holder of this IOBuffer keeps the
  //   backing heap memory valid via `staging_buffer_`. Conversely, callers that
  //   want `StagingBufferPool` recycling must ensure all `NetToMojoIOBuffer`
  //   references are released before calling `Complete()` so `staging_buffer_`
  //   has refcount 1 (`HasOneRef()`).
  // - When unstaged (`staging_buffer_ == nullptr`), this IOBuffer retains
  //   `pending_buffer_` so `NetToMojoPendingBuffer`'s refcount stays > 0
  //   (deferring `EndWriteData(0)` and keeping the Mojo mapping valid) until
  //   the last `NetToMojoIOBuffer` is destroyed.
  scoped_refptr<NetToMojoPendingBuffer::StagingBuffer> staging_buffer_;
  scoped_refptr<NetToMojoPendingBuffer> pending_buffer_;
};

class COMPONENT_EXPORT(NETWORK_CPP) MojoToNetPendingBuffer
    : public base::RefCountedThreadSafe<MojoToNetPendingBuffer> {
 public:
  MojoToNetPendingBuffer(const MojoToNetPendingBuffer&) = delete;
  MojoToNetPendingBuffer& operator=(const MojoToNetPendingBuffer&) = delete;

  // Starts reading from Mojo.
  //
  // On success, MOJO_RESULT_OK will be returned. The ownership of the given
  // consumer handle will be transferred to the new MojoToNetPendingBuffer that
  // will be placed into *pending.
  //
  // On failure or MOJO_RESULT_SHOULD_WAIT, there will be no change to the
  // handle, and *pending will be nulled out.
  static MojoResult BeginRead(mojo::ScopedDataPipeConsumerHandle* handle,
                              scoped_refptr<MojoToNetPendingBuffer>* pending);

  // Indicates the buffer is done being read from. The argument is the number
  // of bytes actually read, since net may do partial writes, which will result
  // in partial reads from the Mojo pipe's perspective.
  void CompleteRead(uint32_t num_bytes);

  // Releases ownership of the pipe handle and returns it.
  mojo::ScopedDataPipeConsumerHandle ReleaseHandle();

  // Returns true if the data was successfully read from the Mojo pipe. We
  // assume that if the buffer_ is null, data was read from the pipe.
  bool IsComplete() const;

  const char* buffer() const { return buffer_.data(); }
  uint32_t size() const { return static_cast<uint32_t>(buffer_.size()); }

  // Satisfy the requirements of `std::ranges::contiguous_range`. This allows
  // implicit conversion to a `base::span`.
  const char* data() const { return buffer_.data(); }
  auto begin() const { return buffer_.begin(); }
  auto end() const { return buffer_.end(); }

 private:
  friend class base::RefCountedThreadSafe<MojoToNetPendingBuffer>;

  // Takes ownership of the handle.
  MojoToNetPendingBuffer(mojo::ScopedDataPipeConsumerHandle handle,
                         base::span<const char> buffer);
  ~MojoToNetPendingBuffer();

  mojo::ScopedDataPipeConsumerHandle handle_;

  // `buffer_` is not a raw_span<...> for performance reasons (also, pointee
  // would never be protected under BackupRefPtr, because the pointer comes
  // either from using `mmap`, MapViewOfFile or base::AllocPages directly).
  base::span<const char> buffer_;
};

// Net side of a Mojo -> Net copy. The data will already be in the
// MojoToNetPendingBuffer's buffer.
class COMPONENT_EXPORT(NETWORK_CPP) MojoToNetIOBuffer
    : public net::WrappedIOBuffer {
 public:
  // |bytes_to_be_read| contains the number of bytes expected to be read by
  // the consumer.
  MojoToNetIOBuffer(scoped_refptr<MojoToNetPendingBuffer> pending_buffer,
                    int bytes_to_be_read);

 private:
  ~MojoToNetIOBuffer() override;

  scoped_refptr<MojoToNetPendingBuffer> pending_buffer_;
  int bytes_to_be_read_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NET_ADAPTERS_H_
