// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#ifndef NET_BASE_IO_BUFFER_H_
#define NET_BASE_IO_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/memory/free_deleter.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/pickle.h"
#include "net/base/net_export.h"

namespace net {

// IOBuffers are reference counted data buffers used for easier asynchronous
// IO handling.
//
// They are often used as the destination buffers for Read() operations, or as
// the source buffers for Write() operations.
//
// IMPORTANT: Never re-use an IOBuffer after cancelling the IO operation that
//            was using it, since this may lead to memory corruption!
//
// -----------------------
// Ownership of IOBuffers:
// -----------------------
//
// Although IOBuffers are RefCountedThreadSafe, they are not intended to be
// used as a shared buffer, nor should they be used simultaneously across
// threads. The fact that they are reference counted is an implementation
// detail for allowing them to outlive cancellation of asynchronous
// operations.
//
// Instead, think of the underlying |char*| buffer contained by the IOBuffer
// as having exactly one owner at a time.
//
// Whenever you call an asynchronous operation that takes an IOBuffer,
// ownership is implicitly transferred to the called function, until the
// operation has completed (at which point it transfers back to the caller).
//
//     ==> The IOBuffer's data should NOT be manipulated, destroyed, or read
//         until the operation has completed.
//
//     ==> Cancellation does NOT count as completion. If an operation using
//         an IOBuffer is cancelled, the caller should release their
//         reference to this IOBuffer at the time of cancellation since
//         they can no longer use it.
//
// For instance, if you were to call a Read() operation on some class which
// takes an IOBuffer, and then delete that class (which generally will
// trigger cancellation), the IOBuffer which had been passed to Read() should
// never be re-used.
//
// This usage contract is assumed by any API which takes an IOBuffer, even
// though it may not be explicitly mentioned in the function's comments.
//
// -----------------------
// Motivation
// -----------------------
//
// The motivation for transferring ownership during cancellation is
// to make it easier to work with un-cancellable operations.
//
// For instance, let's say under the hood your API called out to the
// operating system's synchronous ReadFile() function on a worker thread.
// When cancelling through our asynchronous interface, we have no way of
// actually aborting the in progress ReadFile(). We must let it keep running,
// and hence the buffer it was reading into must remain alive. Using
// reference counting we can add a reference to the IOBuffer and make sure
// it is not destroyed until after the synchronous operation has completed.

// Base class, never instantiated, does not own the buffer.
class NET_EXPORT IOBuffer : public base::RefCountedThreadSafe<IOBuffer> {
 public:
  int size() const { return size_; }

  char* data() { return data_; }
  const char* data() const { return data_; }

  uint8_t* bytes() { return reinterpret_cast<uint8_t*>(data()); }
  const uint8_t* bytes() const {
    return reinterpret_cast<const uint8_t*>(data());
  }

  base::span<uint8_t> span() {
    return base::make_span(bytes(), static_cast<size_t>(size_));
  }
  base::span<const uint8_t> span() const {
    return base::make_span(bytes(), static_cast<size_t>(size_));
  }

 protected:
  friend class base::RefCountedThreadSafe<IOBuffer>;

  static void AssertValidBufferSize(size_t size);

  IOBuffer();
  explicit IOBuffer(base::span<char> data);
  explicit IOBuffer(base::span<uint8_t> data);

  virtual ~IOBuffer();

  raw_ptr<char, AllowPtrArithmetic> data_ = nullptr;
  int size_ = 0;
};

// Class which owns its buffer and manages its destruction.
class NET_EXPORT IOBufferWithSize : public IOBuffer {
 public:
  IOBufferWithSize();
  explicit IOBufferWithSize(size_t size);

 protected:
  ~IOBufferWithSize() override;

 private:
  base::HeapArray<char> storage_;
};

// This is a read only IOBuffer.  The data is stored in a string and
// the IOBuffer interface does not provide a proper way to modify it.
class NET_EXPORT StringIOBuffer : public IOBuffer {
 public:
  explicit StringIOBuffer(std::string s);

 private:
  ~StringIOBuffer() override;

  std::string string_data_;
};

// This version wraps an existing IOBuffer and provides convenient functions
// to progressively read all the data.
//
// DrainableIOBuffer is useful when you have an IOBuffer that contains data
// to be written progressively, and Write() function takes an IOBuffer rather
// than char*. DrainableIOBuffer can be used as follows:
//
// // payload is the IOBuffer containing the data to be written.
// buf = base::MakeRefCounted<DrainableIOBuffer>(payload, payload_size);
//
// while (buf->BytesRemaining() > 0) {
//   // Write() takes an IOBuffer. If it takes char*, we could
//   // simply use the regular IOBuffer like payload->data() + offset.
//   int bytes_written = Write(buf, buf->BytesRemaining());
//   buf->DidConsume(bytes_written);
// }
//
class NET_EXPORT DrainableIOBuffer : public IOBuffer {
 public:
  DrainableIOBuffer(scoped_refptr<IOBuffer> base, size_t size);

  // DidConsume() changes the |data_| pointer so that |data_| always points
  // to the first unconsumed byte.
  void DidConsume(int bytes);

  // Returns the number of unconsumed bytes.
  int BytesRemaining() const;

  // Returns the number of consumed bytes.
  int BytesConsumed() const;

  // Seeks to an arbitrary point in the buffer. The notion of bytes consumed
  // and remaining are updated appropriately.
  void SetOffset(int bytes);

 private:
  ~DrainableIOBuffer() override;

  scoped_refptr<IOBuffer> base_;
  int used_ = 0;
};

// This version provides a resizable buffer and a changeable offset.
//
// GrowableIOBuffer is useful when you read data progressively without
// knowing the total size in advance. GrowableIOBuffer can be used as
// follows:
//
// buf = base::MakeRefCounted<GrowableIOBuffer>();
// buf->SetCapacity(1024);  // Initial capacity.
//
// while (!some_stream->IsEOF()) {
//   // Double the capacity if the remaining capacity is empty.
//   if (buf->RemainingCapacity() == 0)
//     buf->SetCapacity(buf->capacity() * 2);
//   int bytes_read = some_stream->Read(buf, buf->RemainingCapacity());
//   buf->set_offset(buf->offset() + bytes_read);
// }
//
class NET_EXPORT GrowableIOBuffer : public IOBuffer {
 public:
  GrowableIOBuffer();

  // realloc memory to the specified capacity.
  void SetCapacity(int capacity);
  int capacity() { return capacity_; }

  // |offset| moves the |data_| pointer, allowing "seeking" in the data.
  void set_offset(int offset);
  int offset() { return offset_; }

  int RemainingCapacity();

  // Returns the entire buffer, including the bytes before the `offset()`.
  //
  // The `span()` method in the base class only gives the part of the buffer
  // after `offset()`.
  base::span<uint8_t> everything();
  base::span<const uint8_t> everything() const;

  // Return a span before the `offset()`.
  base::span<uint8_t> span_before_offset();
  base::span<const uint8_t> span_before_offset() const;

 private:
  ~GrowableIOBuffer() override;

  // TODO(329476354): Convert to std::vector, use reserve()+resize() to make
  // exact reallocs, and remove `capacity_`. Possibly with an allocator the
  // default-initializes, if it's important to not initialize the new memory?
  std::unique_ptr<char, base::FreeDeleter> real_data_;
  int capacity_ = 0;
  int offset_ = 0;
};

// This versions allows a pickle to be used as the storage for a write-style
// operation, avoiding an extra data copy.
class NET_EXPORT PickledIOBuffer : public IOBuffer {
 public:
  PickledIOBuffer();

  base::Pickle* pickle() { return &pickle_; }

  // Signals that we are done writing to the pickle and we can use it for a
  // write-style IO operation.
  void Done();

 private:
  ~PickledIOBuffer() override;

  base::Pickle pickle_;
};

// This class allows the creation of a temporary IOBuffer that doesn't really
// own the underlying buffer. Please use this class only as a last resort.
// A good example is the buffer for a synchronous operation, where we can be
// sure that nobody is keeping an extra reference to this object so the lifetime
// of the buffer can be completely managed by its intended owner.
// This is now nearly the same as the base IOBuffer class, except that it
// accepts const data as constructor arguments.
class NET_EXPORT WrappedIOBuffer : public IOBuffer {
 public:
  explicit WrappedIOBuffer(base::span<const char> data);
  explicit WrappedIOBuffer(base::span<const uint8_t> data);

 protected:
  ~WrappedIOBuffer() override;
};

}  // namespace net

#endif  // NET_BASE_IO_BUFFER_H_
