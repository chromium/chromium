// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKET_BIO_ADAPTER_H_
#define NET_SOCKET_SOCKET_BIO_ADAPTER_H_

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace net {

class GrowableIOBuffer;
class IOBuffer;
class StreamSocket;

// An adapter to convert between StreamSocket and OpenSSL BIO I/O models.
//
// BIO exposes a UNIX-like interface where BIO_read and BIO_write may either
// succeed synchronously or be retried (with no memory between calls).
// StreamSocket exposes an asynchronous interface where an asynchronous
// operation continues running and completes with a callback.
//
// For reading, SocketBIOAdapter maintains a buffer to pass to
// StreamSocket::Read. Once that Read completes, BIO_read synchronously drains
// the buffer and signals BIO_should_read once empty.
//
// For writing, SocketBIOAdapter maintains a ring buffer of data to be written
// to the StreamSocket. BIO_write synchronously copies data into the buffer or
// signals BIO_should_write if the buffer is full. The ring buffer is drained
// asynchronously into the socket. Note this means write errors are reported at
// a later BIO_write.
//
// To work around this delay, write errors are also surfaced out of
// BIO_read. Otherwise a failure in the final BIO_write of an application may go
// unnoticed. If this occurs, OnReadReady will be signaled as if it were a read
// error. See https://crbug.com/249848.
class NET_EXPORT_PRIVATE SocketBIOAdapter {
 public:
  // A delegate interface for when the sockets are ready. BIO assumes external
  // knowledge of when to retry operations (such as a select() loop for UNIX),
  // which is signaled out of StreamSocket's callbacks here.
  //
  // Callers should implement these methods and, when signaled, retry the
  // BIO_read or BIO_write. This usually is done by retrying a higher-level
  // operation, such as SSL_read or SSL_write.
  //
  // Callers may assume that OnReadReady and OnWriteReady will only be called
  // from a PostTask or StreamSocket callback.
  class Delegate {
   public:
    // Called when the BIO is ready to handle BIO_read, after having previously
    // been blocked.
    virtual void OnReadReady() = 0;

    // Called when the BIO is ready to handle BIO_write, after having previously
    // been blocked.
    virtual void OnWriteReady() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Creates a new SocketBIOAdapter for the specified socket. |socket| and
  // |delegate| must remain valid for the lifetime of the SocketBIOAdapter.
  SocketBIOAdapter(StreamSocket* socket,
                   int read_buffer_capacity,
                   int write_buffer_capacity,
                   Delegate* delegate);

  SocketBIOAdapter(const SocketBIOAdapter&) = delete;
  SocketBIOAdapter& operator=(const SocketBIOAdapter&) = delete;

  ~SocketBIOAdapter();

  BIO* bio() { return bio_.get(); }

  // Returns true if any data has been read from the underlying StreamSocket,
  // but not yet consumed by the BIO.
  bool HasPendingReadData();

  // Returns the allocation size estimate in bytes.
  size_t GetAllocationSize() const;

 private:
  int BIORead(base::span<uint8_t> out);
  void HandleSocketReadResult(int result);
  void OnSocketReadComplete(int result);
  void OnSocketReadIfReadyComplete(int result);

  int BIOWrite(base::span<const uint8_t> in);
  void SocketWrite();
  void HandleSocketWriteResult(int result);
  void OnSocketWriteComplete(int result);
  void CallOnReadReady();

  static SocketBIOAdapter* GetAdapter(BIO* bio);
  static int BIOReadWrapper(BIO* bio, char* out, int len);
  static int BIOWriteWrapper(BIO* bio, const char* in, int len);
  static long BIOCtrlWrapper(BIO* bio, int cmd, long larg, void* parg);

  static const BIO_METHOD* BIOMethod();

  bssl::UniquePtr<BIO> bio_;

  // The pointer is non-owning so this class may be used with both
  // ClientSocketHandles and raw StreamSockets.
  raw_ptr<StreamSocket> socket_;

  CompletionRepeatingCallback read_callback_;
  CompletionRepeatingCallback write_callback_;

  // The capacity of the read buffer.
  int read_buffer_capacity_;
  // A buffer containing data from the most recent socket Read(). The buffer is
  // deallocated when unused.
  scoped_refptr<IOBuffer> read_buffer_;
  // The number of bytes of read_buffer_ consumed.
  int read_offset_ = 0;
  // The result of the most recent socket Read(). If ERR_IO_PENDING, there is a
  // socket Read() in progress. If another error, Read() has failed. Otherwise,
  // it is the number of bytes in the buffer (zero if empty).
  int read_result_ = 0;

  // The capacity of the write buffer.
  int write_buffer_capacity_;
  // A ring buffer of data to be written to the transport. The offset of the
  // buffer is the start of the ring buffer and is advanced on successful
  // Write(). The buffer is deallocated when unused.
  scoped_refptr<GrowableIOBuffer> write_buffer_;
  // The number of bytes of data in write_buffer_.
  int write_buffer_used_ = 0;
  // The most recent socket Write() error. If ERR_IO_PENDING, there is a socket
  // Write() in progress. If OK, there is no socket Write() in progress and none
  // have failed.
  int write_error_ = OK;

  raw_ptr<Delegate> delegate_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<SocketBIOAdapter> weak_factory_{this};
};

}  // namespace net

#endif  // NET_SOCKET_SOCKET_BIO_ADAPTER_H_
