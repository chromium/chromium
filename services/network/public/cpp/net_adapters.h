// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NET_ADAPTERS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NET_ADAPTERS_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/macros.h"
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
  // Begins a two-phase write to the data pipe.
  //
  // On success, MOJO_RESULT_OK will be returned. The ownership of the given
  // producer handle will be transferred to the new NetToMojoPendingBuffer that
  // will be placed into *pending, and the size of the buffer will be in
  // *num_bytes.
  //
  // On failure or MOJO_RESULT_SHOULD_WAIT, there will be no change to the
  // handle, and *pending and *num_bytes will be unused.
  static MojoResult BeginWrite(mojo::ScopedDataPipeProducerHandle* handle,
                               scoped_refptr<NetToMojoPendingBuffer>* pending,
                               uint32_t* num_bytes);
  // Called to indicate the buffer is done being written to. Passes ownership
  // of the pipe back to the caller.
  mojo::ScopedDataPipeProducerHandle Complete(uint32_t num_bytes);
  char* buffer() { return static_cast<char*>(buffer_); }

 private:
  friend class base::RefCountedThreadSafe<NetToMojoPendingBuffer>;
  // Takes ownership of the handle.
  NetToMojoPendingBuffer(mojo::ScopedDataPipeProducerHandle handle,
                         void* buffer);
  ~NetToMojoPendingBuffer();
  mojo::ScopedDataPipeProducerHandle handle_;
  void* buffer_;
  DISALLOW_COPY_AND_ASSIGN(NetToMojoPendingBuffer);
};

// Net side of a Net -> Mojo copy. The data will be read from the network and
// copied into the buffer associated with the pending mojo write.
class COMPONENT_EXPORT(NETWORK_CPP) NetToMojoIOBuffer
    : public net::WrappedIOBuffer {
 public:
  // If |offset| is specified then the memory buffer passed to the Net layer
  // will be offset by that many bytes.
  NetToMojoIOBuffer(NetToMojoPendingBuffer* pending_buffer, int offset = 0);

 private:
  ~NetToMojoIOBuffer() override;
  scoped_refptr<NetToMojoPendingBuffer> pending_buffer_;

  DISALLOW_COPY_AND_ASSIGN(NetToMojoIOBuffer);
};

class COMPONENT_EXPORT(NETWORK_CPP) MojoToNetPendingBuffer
    : public base::RefCountedThreadSafe<MojoToNetPendingBuffer> {
 public:
  // Starts reading from Mojo.
  //
  // On success, MOJO_RESULT_OK will be returned. The ownership of the given
  // consumer handle will be transferred to the new MojoToNetPendingBuffer that
  // will be placed into *pending, and the size of the buffer will be in
  // *num_bytes.
  //
  // On failure or MOJO_RESULT_SHOULD_WAIT, there will be no change to the
  // handle, and *pending and *num_bytes will be unused.
  static MojoResult BeginRead(mojo::ScopedDataPipeConsumerHandle* handle,
                              scoped_refptr<MojoToNetPendingBuffer>* pending,
                              uint32_t* num_bytes);

  // Indicates the buffer is done being read from. The argument is the number
  // of bytes actually read, since net may do partial writes, which will result
  // in partial reads from the Mojo pipe's perspective.
  void CompleteRead(uint32_t num_bytes);

  // Releases ownership of the pipe handle and returns it.
  mojo::ScopedDataPipeConsumerHandle ReleaseHandle();

  // Returns true if the data was successfully read from the Mojo pipe. We
  // assume that if the buffer_ is null, data was read from the pipe.
  bool IsComplete() const;

  const char* buffer() { return static_cast<const char*>(buffer_); }

 private:
  friend class base::RefCountedThreadSafe<MojoToNetPendingBuffer>;

  // Takes ownership of the handle.
  explicit MojoToNetPendingBuffer(mojo::ScopedDataPipeConsumerHandle handle,
                                  const void* buffer);
  ~MojoToNetPendingBuffer();

  mojo::ScopedDataPipeConsumerHandle handle_;
  const void* buffer_;

  DISALLOW_COPY_AND_ASSIGN(MojoToNetPendingBuffer);
};

// Net side of a Mojo -> Net copy. The data will already be in the
// MojoToNetPendingBuffer's buffer.
class COMPONENT_EXPORT(NETWORK_CPP) MojoToNetIOBuffer
    : public net::WrappedIOBuffer {
 public:
  // |bytes_to_be_read| contains the number of bytes expected to be read by
  // the consumer.
  MojoToNetIOBuffer(MojoToNetPendingBuffer* pending_buffer,
                    int bytes_to_be_read);

 private:
  ~MojoToNetIOBuffer() override;

  scoped_refptr<MojoToNetPendingBuffer> pending_buffer_;
  int bytes_to_be_read_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NET_ADAPTERS_H_
