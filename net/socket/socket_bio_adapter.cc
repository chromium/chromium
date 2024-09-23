// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_bio_adapter.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"
#include "net/socket/stream_socket.h"
#include "net/ssl/openssl_ssl_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/boringssl/src/include/openssl/bio.h"

namespace {

const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("socket_bio_adapter", R"(
      semantics {
        sender: "Socket BIO Adapter"
        description:
          "SocketBIOAdapter is used only internal to //net code as an internal "
          "detail to implement a TLS connection for a Socket class, and is not "
          "being called directly outside of this abstraction."
        trigger:
          "Establishing a TLS connection to a remote endpoint. There are many "
          "different ways in which a TLS connection may be triggered, such as "
          "loading an HTTPS URL."
        data:
          "All data sent or received over a TLS connection. This traffic may "
          "either be the handshake or application data. During the handshake, "
          "the target host name, user's IP, data related to previous "
          "handshake, client certificates, and channel ID, may be sent. When "
          "the connection is used to load an HTTPS URL, the application data "
          "includes cookies, request headers, and the response body."
        destination: OTHER
        destination_other:
          "Any destination the implementing socket is connected to."
      }
      policy {
        cookies_allowed: NO
        setting: "This feature cannot be disabled."
        policy_exception_justification: "Essential for navigation."
      })");

}  // namespace

namespace net {

SocketBIOAdapter::SocketBIOAdapter(StreamSocket* socket,
                                   int read_buffer_capacity,
                                   int write_buffer_capacity,
                                   Delegate* delegate)
    : socket_(socket),
      read_buffer_capacity_(read_buffer_capacity),
      write_buffer_capacity_(write_buffer_capacity),
      delegate_(delegate) {
  bio_.reset(BIO_new(BIOMethod()));
  BIO_set_data(bio_.get(), this);
  BIO_set_init(bio_.get(), 1);

  read_callback_ = base::BindRepeating(&SocketBIOAdapter::OnSocketReadComplete,
                                       weak_factory_.GetWeakPtr());
  write_callback_ = base::BindRepeating(
      &SocketBIOAdapter::OnSocketWriteComplete, weak_factory_.GetWeakPtr());
}

SocketBIOAdapter::~SocketBIOAdapter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // BIOs are reference-counted and may outlive the adapter. Clear the pointer
  // so future operations fail.
  BIO_set_data(bio_.get(), nullptr);
}

bool SocketBIOAdapter::HasPendingReadData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return read_result_ > 0;
}

size_t SocketBIOAdapter::GetAllocationSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t buffer_size = 0;
  if (read_buffer_)
    buffer_size += read_buffer_capacity_;

  if (write_buffer_)
    buffer_size += write_buffer_capacity_;
  return buffer_size;
}

int SocketBIOAdapter::BIORead(base::span<uint8_t> out) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (out.empty()) {
    return 0;
  }

  // If there is no result available synchronously, report any Write() errors
  // that were observed. Otherwise the application may have encountered a socket
  // error while writing that would otherwise not be reported until the
  // application attempted to write again - which it may never do. See
  // https://crbug.com/249848.
  if (write_error_ != OK && write_error_ != ERR_IO_PENDING &&
      (read_result_ == 0 || read_result_ == ERR_IO_PENDING)) {
    OpenSSLPutNetError(FROM_HERE, write_error_);
    return -1;
  }

  if (read_result_ == 0) {
    // Instantiate the read buffer and read from the socket. Although only |len|
    // bytes were requested, intentionally read to the full buffer size. The SSL
    // layer reads the record header and body in separate reads to avoid
    // overreading, but issuing one is more efficient. SSL sockets are not
    // reused after shutdown for non-SSL traffic, so overreading is fine.
    CHECK(!read_buffer_);
    CHECK_EQ(0, read_offset_);
    read_buffer_ =
        base::MakeRefCounted<IOBufferWithSize>(read_buffer_capacity_);
    read_result_ = ERR_IO_PENDING;
    int result = socket_->ReadIfReady(
        read_buffer_.get(), read_buffer_capacity_,
        base::BindOnce(&SocketBIOAdapter::OnSocketReadIfReadyComplete,
                       weak_factory_.GetWeakPtr()));
    if (result == ERR_IO_PENDING)
      read_buffer_ = nullptr;
    if (result == ERR_READ_IF_READY_NOT_IMPLEMENTED) {
      result = socket_->Read(read_buffer_.get(), read_buffer_capacity_,
                             read_callback_);
    }
    if (result != ERR_IO_PENDING) {
      // `HandleSocketReadResult` will update `read_result_` based on `result`.
      HandleSocketReadResult(result);
    }
  }

  // There is a pending Read(). Inform the caller to retry when it completes.
  if (read_result_ == ERR_IO_PENDING) {
    BIO_set_retry_read(bio());
    return -1;
  }

  // If the last Read() failed, report the error.
  if (read_result_ < 0) {
    OpenSSLPutNetError(FROM_HERE, read_result_);
    return -1;
  }

  // Report the result of the last Read() if non-empty.
  CHECK_LT(read_offset_, read_result_);
  base::span<const uint8_t> read_data = read_buffer_->span().subspan(
      read_offset_, std::min(out.size(), base::checked_cast<size_t>(
                                             read_result_ - read_offset_)));
  out.copy_prefix_from(read_data);
  read_offset_ += read_data.size();

  // Release the buffer when empty.
  if (read_offset_ == read_result_) {
    read_buffer_ = nullptr;
    read_offset_ = 0;
    read_result_ = 0;
  }

  return read_data.size();
}

void SocketBIOAdapter::HandleSocketReadResult(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_NE(ERR_IO_PENDING, result);
  CHECK_EQ(ERR_IO_PENDING, read_result_);

  // If an EOF, canonicalize to ERR_CONNECTION_CLOSED here, so that higher
  // levels don't report success.
  if (result == 0)
    result = ERR_CONNECTION_CLOSED;

  read_result_ = result;

  // The read buffer is no longer needed.
  if (read_result_ <= 0)
    read_buffer_ = nullptr;
}

void SocketBIOAdapter::OnSocketReadComplete(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(ERR_IO_PENDING, read_result_);

  HandleSocketReadResult(result);
  delegate_->OnReadReady();
}

void SocketBIOAdapter::OnSocketReadIfReadyComplete(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(ERR_IO_PENDING, read_result_);
  CHECK_GE(OK, result);

  // Do not use HandleSocketReadResult() because result == OK doesn't mean EOF.
  read_result_ = result;

  delegate_->OnReadReady();
}

int SocketBIOAdapter::BIOWrite(base::span<const uint8_t> in) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (in.empty()) {
    return 0;
  }

  // If the write buffer is not empty, there must be a pending Write() to flush
  // it.
  CHECK(write_buffer_used_ == 0 || write_error_ == ERR_IO_PENDING);

  // If a previous Write() failed, report the error.
  if (write_error_ != OK && write_error_ != ERR_IO_PENDING) {
    OpenSSLPutNetError(FROM_HERE, write_error_);
    return -1;
  }

  // Instantiate the write buffer if needed.
  if (!write_buffer_) {
    CHECK_EQ(0, write_buffer_used_);
    write_buffer_ = base::MakeRefCounted<GrowableIOBuffer>();
    write_buffer_->SetCapacity(write_buffer_capacity_);
  }

  // If the ring buffer is full, inform the caller to try again later.
  if (write_buffer_used_ == write_buffer_->capacity()) {
    BIO_set_retry_write(bio());
    return -1;
  }

  int bytes_copied = 0;

  // If there is space after the offset, fill it.
  if (write_buffer_used_ < write_buffer_->RemainingCapacity()) {
    base::span<const uint8_t> chunk = in.first(
        std::min(base::checked_cast<size_t>(write_buffer_->RemainingCapacity() -
                                            write_buffer_used_),
                 in.size()));
    write_buffer_->span().subspan(write_buffer_used_).copy_prefix_from(chunk);
    in = in.subspan(chunk.size());
    bytes_copied += chunk.size();
    write_buffer_used_ += chunk.size();
  }

  // If there is still space for remaining data, try to wrap around.
  if (in.size() > 0 && write_buffer_used_ < write_buffer_->capacity()) {
    // If there were any room after the offset, the previous branch would have
    // filled it.
    CHECK_LE(write_buffer_->RemainingCapacity(), write_buffer_used_);
    int write_offset = write_buffer_used_ - write_buffer_->RemainingCapacity();
    base::span<const uint8_t> chunk = in.first(std::min(
        in.size(), base::checked_cast<size_t>(write_buffer_->capacity() -
                                              write_buffer_used_)));
    write_buffer_->everything().subspan(write_offset).copy_prefix_from(chunk);
    in = in.subspan(chunk.size());
    bytes_copied += chunk.size();
    write_buffer_used_ += chunk.size();
  }

  // Either the buffer is now full or there is no more input.
  CHECK(in.empty() || write_buffer_used_ == write_buffer_->capacity());

  // Schedule a socket Write() if necessary. (The ring buffer may previously
  // have been empty.)
  SocketWrite();

  // If a read-interrupting write error was synchronously discovered,
  // asynchronously notify OnReadReady. See https://crbug.com/249848. Avoid
  // reentrancy by deferring it to a later event loop iteration.
  if (write_error_ != OK && write_error_ != ERR_IO_PENDING &&
      read_result_ == ERR_IO_PENDING) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&SocketBIOAdapter::CallOnReadReady,
                                  weak_factory_.GetWeakPtr()));
  }

  return bytes_copied;
}

void SocketBIOAdapter::SocketWrite() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (write_error_ == OK && write_buffer_used_ > 0) {
    int write_buffer_used_old = write_buffer_used_;
    int write_size =
        std::min(write_buffer_used_, write_buffer_->RemainingCapacity());

    // TODO(crbug.com/40064248): Remove this once the crash is resolved.
    char debug[128];
    snprintf(debug, sizeof(debug),
             "offset=%d;remaining=%d;used=%d;write_size=%d",
             write_buffer_->offset(), write_buffer_->RemainingCapacity(),
             write_buffer_used_, write_size);
    base::debug::Alias(debug);

    write_error_ = ERR_IO_PENDING;
    int result = socket_->Write(write_buffer_.get(), write_size,
                                write_callback_, kTrafficAnnotation);

    // TODO(crbug.com/40064248): Remove this once the crash is resolved.
    char debug2[32];
    snprintf(debug2, sizeof(debug2), "result=%d", result);
    base::debug::Alias(debug2);

    // If `write_buffer_used_` changed across a call to the underlying socket,
    // something went very wrong.
    //
    // TODO(crbug.com/40064248): Remove this once the crash is resolved.
    CHECK_EQ(write_buffer_used_old, write_buffer_used_);
    if (result != ERR_IO_PENDING) {
      // `HandleSocketWriteResult` will update `write_error_` based on `result.
      HandleSocketWriteResult(result);
    }
  }
}

void SocketBIOAdapter::HandleSocketWriteResult(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_NE(ERR_IO_PENDING, result);
  CHECK_EQ(ERR_IO_PENDING, write_error_);

  if (result < 0) {
    write_error_ = result;

    // The write buffer is no longer needed.
    write_buffer_ = nullptr;
    write_buffer_used_ = 0;
    return;
  }

  // Advance the ring buffer.
  CHECK_LE(result, write_buffer_used_);
  CHECK_LE(result, write_buffer_->RemainingCapacity());
  write_buffer_->set_offset(write_buffer_->offset() + result);
  write_buffer_used_ -= result;
  if (write_buffer_->RemainingCapacity() == 0)
    write_buffer_->set_offset(0);
  write_error_ = OK;

  // Release the write buffer if empty.
  if (write_buffer_used_ == 0)
    write_buffer_ = nullptr;
}

void SocketBIOAdapter::OnSocketWriteComplete(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(ERR_IO_PENDING, write_error_);

  bool was_full = write_buffer_used_ == write_buffer_->capacity();

  HandleSocketWriteResult(result);
  SocketWrite();

  // If transitioning from being unable to accept data to being able to, signal
  // OnWriteReady.
  if (was_full) {
    base::WeakPtr<SocketBIOAdapter> guard(weak_factory_.GetWeakPtr());
    delegate_->OnWriteReady();
    // OnWriteReady may delete the adapter.
    if (!guard)
      return;
  }

  // Write errors are fed back into BIO_read once the read buffer is empty. If
  // BIO_read is currently blocked, signal early that a read result is ready.
  if (result < 0 && read_result_ == ERR_IO_PENDING)
    delegate_->OnReadReady();
}

void SocketBIOAdapter::CallOnReadReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (read_result_ == ERR_IO_PENDING)
    delegate_->OnReadReady();
}

SocketBIOAdapter* SocketBIOAdapter::GetAdapter(BIO* bio) {
  SocketBIOAdapter* adapter =
      reinterpret_cast<SocketBIOAdapter*>(BIO_get_data(bio));
  if (adapter) {
    CHECK_EQ(bio, adapter->bio());
  }
  return adapter;
}

// TODO(tsepez): should be declared UNSAFE_BUFFER_USAGE in header.
int SocketBIOAdapter::BIOWriteWrapper(BIO* bio, const char* in, int len) {
  BIO_clear_retry_flags(bio);

  SocketBIOAdapter* adapter = GetAdapter(bio);
  if (!adapter) {
    OpenSSLPutNetError(FROM_HERE, ERR_UNEXPECTED);
    return -1;
  }

  return adapter->BIOWrite(base::as_bytes(
      // SAFETY: The caller must ensure `in` points to `len` bytes.
      // TODO(crbug.com/354307327): Spanify this method.
      UNSAFE_TODO(base::span(in, base::checked_cast<size_t>(len)))));
}

// TODO(tsepez): should be declared UNSAFE_BUFFER_USAGE in header.
int SocketBIOAdapter::BIOReadWrapper(BIO* bio, char* out, int len) {
  BIO_clear_retry_flags(bio);

  SocketBIOAdapter* adapter = GetAdapter(bio);
  if (!adapter) {
    OpenSSLPutNetError(FROM_HERE, ERR_UNEXPECTED);
    return -1;
  }

  return adapter->BIORead(base::as_writable_bytes(
      // SAFETY: The caller must ensure `out` points to `len` bytes.
      // TODO(crbug.com/354307327): Spanify this method.
      UNSAFE_TODO(base::span(out, base::checked_cast<size_t>(len)))));
}

long SocketBIOAdapter::BIOCtrlWrapper(BIO* bio,
                                      int cmd,
                                      long larg,
                                      void* parg) {
  switch (cmd) {
    case BIO_CTRL_FLUSH:
      // The SSL stack requires BIOs handle BIO_flush.
      return 1;
  }

  NOTIMPLEMENTED();
  return 0;
}

const BIO_METHOD* SocketBIOAdapter::BIOMethod() {
  static const BIO_METHOD* kMethod = []() {
    BIO_METHOD* method = BIO_meth_new(0, nullptr);
    CHECK(method);
    CHECK(BIO_meth_set_write(method, SocketBIOAdapter::BIOWriteWrapper));
    CHECK(BIO_meth_set_read(method, SocketBIOAdapter::BIOReadWrapper));
    CHECK(BIO_meth_set_ctrl(method, SocketBIOAdapter::BIOCtrlWrapper));
    return method;
  }();
  return kMethod;
}

}  // namespace net
