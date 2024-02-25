// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NET_ADAPTERS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NET_ADAPTERS_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/containers/span.h"
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
// Mojo side of a Net -> Mojo copy. The buffer is allocated by Mojo.
class COMPONENT_EXPORT(NETWORK_CPP) NetToMojoPendingBuffer
    : public base::RefCountedThreadSafe<NetToMojoPendingBuffer> {
 public:
  NetToMojoPendingBuffer(const NetToMojoPendingBuffer&) = delete;
  NetToMojoPendingBuffer& operator=(const NetToMojoPendingBuffer&) = delete;

  // Begins a two-phase write to the data pipe.
  //
  // On success, MOJO_RESULT_OK will be returned. The ownership of the given
  // producer handle will be transferred to the new NetToMojoPendingBuffer that
  // will be placed into *pending, and the size of the buffer will be in
  // *num_bytes.
  //
  // On failure or MOJO_RESULT_SHOULD_WAIT, there will be no change to the
  // handle, and *pending will be nulled out.
  static MojoResult BeginWrite(mojo::ScopedDataPipeProducerHandle* handle,
                               scoped_refptr<NetToMojoPendingBuffer>* pending);

  // Called to indicate the buffer is done being written to. Passes ownership
  // of the pipe back to the caller.
  mojo::ScopedDataPipeProducerHandle Complete(uint32_t num_bytes);

  char* buffer() { return buffer_.data(); }
  uint32_t size() const { return static_cast<uint32_t>(buffer_.size()); }

  // Equivalent of buffer(), but allows the class to satisfy the requirements
  // of std::ranges::contiguous_range, and hence allows a span, for example,
  // to be implicitly constructed from a it.
  char* data() { return buffer_.data(); }

 private:
  friend class base::RefCountedThreadSafe<NetToMojoPendingBuffer>;
  // Takes ownership of the handle.
  NetToMojoPendingBuffer(mojo::ScopedDataPipeProducerHandle handle,
                         base::span<char> buffer);
  ~NetToMojoPendingBuffer();

  mojo::ScopedDataPipeProducerHandle handle_;
  // `buffer_` is not a raw_span<...> for performance reasons (also, pointee
  // would never be protected under BackupRefPtr, because the pointer comes
  // either from using `mmap`, MapViewOfFile or base::AllocPages directly).
  base::span<char> buffer_;
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
      int offset = 0);

  NetToMojoIOBuffer(const NetToMojoIOBuffer&) = delete;
  NetToMojoIOBuffer& operator=(const NetToMojoIOBuffer&) = delete;

 private:
  ~NetToMojoIOBuffer() override;
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

  // Equivalent of buffer(), allows conversion to span.
  const char* data() { return buffer_.data(); }

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
