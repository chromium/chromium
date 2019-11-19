// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_cache_writer.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "content/browser/appcache/appcache_response.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_storage.h"

namespace {

const size_t kCopyBufferSize = 16 * 1024;

// Shim class used to turn always-async functions into async-or-result
// functions. See the comments below near ReadInfoHelper.
class AsyncOnlyCompletionCallbackAdaptor
    : public base::RefCounted<AsyncOnlyCompletionCallbackAdaptor> {
 public:
  explicit AsyncOnlyCompletionCallbackAdaptor(
      net::CompletionOnceCallback callback)
      : async_(false),
        result_(net::ERR_IO_PENDING),
        callback_(std::move(callback)) {}

  void set_async(bool async) { async_ = async; }
  bool async() { return async_; }
  int result() { return result_; }

  void WrappedCallback(int result) {
    result_ = result;
    if (async_)
      std::move(callback_).Run(result);
  }

 private:
  friend class base::RefCounted<AsyncOnlyCompletionCallbackAdaptor>;
  virtual ~AsyncOnlyCompletionCallbackAdaptor() {}

  bool async_;
  int result_;
  net::CompletionOnceCallback callback_;
};

}  // namespace

namespace content {

int ServiceWorkerCacheWriter::DoLoop(int status) {
  do {
    switch (state_) {
      case STATE_START:
        status = DoStart(status);
        break;
      case STATE_READ_HEADERS_FOR_COMPARE:
        status = DoReadHeadersForCompare(status);
        break;
      case STATE_READ_HEADERS_FOR_COMPARE_DONE:
        status = DoReadHeadersForCompareDone(status);
        break;
      case STATE_READ_DATA_FOR_COMPARE:
        status = DoReadDataForCompare(status);
        break;
      case STATE_READ_DATA_FOR_COMPARE_DONE:
        status = DoReadDataForCompareDone(status);
        break;
      case STATE_READ_HEADERS_FOR_COPY:
        status = DoReadHeadersForCopy(status);
        break;
      case STATE_READ_HEADERS_FOR_COPY_DONE:
        status = DoReadHeadersForCopyDone(status);
        break;
      case STATE_READ_DATA_FOR_COPY:
        status = DoReadDataForCopy(status);
        break;
      case STATE_READ_DATA_FOR_COPY_DONE:
        status = DoReadDataForCopyDone(status);
        break;
      case STATE_WRITE_HEADERS_FOR_PASSTHROUGH:
        status = DoWriteHeadersForPassthrough(status);
        break;
      case STATE_WRITE_HEADERS_FOR_PASSTHROUGH_DONE:
        status = DoWriteHeadersForPassthroughDone(status);
        break;
      case STATE_WRITE_DATA_FOR_PASSTHROUGH:
        status = DoWriteDataForPassthrough(status);
        break;
      case STATE_WRITE_DATA_FOR_PASSTHROUGH_DONE:
        status = DoWriteDataForPassthroughDone(status);
        break;
      case STATE_WRITE_HEADERS_FOR_COPY:
        status = DoWriteHeadersForCopy(status);
        break;
      case STATE_WRITE_HEADERS_FOR_COPY_DONE:
        status = DoWriteHeadersForCopyDone(status);
        break;
      case STATE_WRITE_DATA_FOR_COPY:
        status = DoWriteDataForCopy(status);
        break;
      case STATE_WRITE_DATA_FOR_COPY_DONE:
        status = DoWriteDataForCopyDone(status);
        break;
      case STATE_DONE:
        status = DoDone(status);
        break;
      default:
        NOTREACHED() << "Unknown state in DoLoop";
        state_ = STATE_DONE;
        break;
    }
  } while (status != net::ERR_IO_PENDING && state_ != STATE_DONE);
  io_pending_ = (status == net::ERR_IO_PENDING);
  return status;
}

ServiceWorkerCacheWriter::ServiceWorkerCacheWriter(
    std::unique_ptr<ServiceWorkerResponseReader> compare_reader,
    std::unique_ptr<ServiceWorkerResponseReader> copy_reader,
    std::unique_ptr<ServiceWorkerResponseWriter> writer,
    bool pause_when_not_identical)
    : state_(STATE_START),
      io_pending_(false),
      comparing_(false),
      pause_when_not_identical_(pause_when_not_identical),
      compare_reader_(std::move(compare_reader)),
      copy_reader_(std::move(copy_reader)),
      writer_(std::move(writer)) {}

ServiceWorkerCacheWriter::~ServiceWorkerCacheWriter() {}

std::unique_ptr<ServiceWorkerCacheWriter>
ServiceWorkerCacheWriter::CreateForCopy(
    std::unique_ptr<ServiceWorkerResponseReader> copy_reader,
    std::unique_ptr<ServiceWorkerResponseWriter> writer) {
  DCHECK(copy_reader);
  DCHECK(writer);
  return base::WrapUnique(new ServiceWorkerCacheWriter(
      nullptr /* compare_reader */, std::move(copy_reader), std::move(writer),
      false /* pause_when_not_identical*/));
}

std::unique_ptr<ServiceWorkerCacheWriter>
ServiceWorkerCacheWriter::CreateForWriteBack(
    std::unique_ptr<ServiceWorkerResponseWriter> writer) {
  DCHECK(writer);
  return base::WrapUnique(new ServiceWorkerCacheWriter(
      nullptr /* compare_reader */, nullptr /* copy_reader */,
      std::move(writer), false /* pause_when_not_identical*/));
}

std::unique_ptr<ServiceWorkerCacheWriter>
ServiceWorkerCacheWriter::CreateForComparison(
    std::unique_ptr<ServiceWorkerResponseReader> compare_reader,
    std::unique_ptr<ServiceWorkerResponseReader> copy_reader,
    std::unique_ptr<ServiceWorkerResponseWriter> writer,
    bool pause_when_not_identical) {
  // |compare_reader| reads data for the comparison. |copy_reader| reads
  // data for copy.
  DCHECK(compare_reader);
  DCHECK(copy_reader);
  DCHECK(writer);
  return base::WrapUnique(new ServiceWorkerCacheWriter(
      std::move(compare_reader), std::move(copy_reader), std::move(writer),
      pause_when_not_identical));
}

net::Error ServiceWorkerCacheWriter::MaybeWriteHeaders(
    HttpResponseInfoIOBuffer* headers,
    OnWriteCompleteCallback callback) {
  DCHECK(!io_pending_);
  DCHECK(!IsCopying());

  headers_to_write_ = headers;
  pending_callback_ = std::move(callback);
  DCHECK_EQ(STATE_START, state_);
  int result = DoLoop(net::OK);

  // Synchronous errors and successes always go to STATE_DONE.
  if (result != net::ERR_IO_PENDING)
    DCHECK_EQ(STATE_DONE, state_);

  // ERR_IO_PENDING has to have one of the STATE_*_DONE states as the next state
  // (not STATE_DONE itself).
  if (result == net::ERR_IO_PENDING) {
    DCHECK(state_ == STATE_READ_HEADERS_FOR_COMPARE_DONE ||
           state_ == STATE_WRITE_HEADERS_FOR_COPY_DONE ||
           state_ == STATE_WRITE_HEADERS_FOR_PASSTHROUGH_DONE)
        << "Unexpected state: " << state_;
    io_pending_ = true;
  }

  return result >= 0 ? net::OK : static_cast<net::Error>(result);
}

net::Error ServiceWorkerCacheWriter::MaybeWriteData(
    net::IOBuffer* buf,
    size_t buf_size,
    OnWriteCompleteCallback callback) {
  DCHECK(!io_pending_);
  DCHECK(!IsCopying());

  data_to_write_ = buf;
  len_to_write_ = buf_size;
  pending_callback_ = std::move(callback);

  if (comparing_)
    state_ = STATE_READ_DATA_FOR_COMPARE;
  else
    state_ = STATE_WRITE_DATA_FOR_PASSTHROUGH;

  int result = DoLoop(net::OK);

  // Synchronous completions are always STATE_DONE.
  if (result != net::ERR_IO_PENDING)
    DCHECK_EQ(STATE_DONE, state_);

  // Asynchronous completion means the state machine must be waiting in one of
  // the Done states for an IO operation to complete:
  if (result == net::ERR_IO_PENDING) {
    // Note that STATE_READ_HEADERS_FOR_COMPARE_DONE is excluded because the
    // headers are compared in MaybeWriteHeaders, not here, and
    // STATE_WRITE_HEADERS_FOR_PASSTHROUGH_DONE is excluded because that write
    // is done by MaybeWriteHeaders.
    DCHECK(state_ == STATE_READ_DATA_FOR_COMPARE_DONE ||
           state_ == STATE_PAUSING ||
           state_ == STATE_READ_HEADERS_FOR_COPY_DONE ||
           state_ == STATE_READ_DATA_FOR_COPY_DONE ||
           state_ == STATE_WRITE_HEADERS_FOR_COPY_DONE ||
           state_ == STATE_WRITE_DATA_FOR_COPY_DONE ||
           state_ == STATE_WRITE_DATA_FOR_PASSTHROUGH_DONE)
        << "Unexpected state: " << state_;
  }
  return result >= 0 ? net::OK : static_cast<net::Error>(result);
}

net::Error ServiceWorkerCacheWriter::Resume(OnWriteCompleteCallback callback) {
  DCHECK(pause_when_not_identical_);
  DCHECK_EQ(STATE_PAUSING, state_);
  DCHECK(io_pending_);
  DCHECK(!IsCopying());

  io_pending_ = false;
  pending_callback_ = std::move(callback);
  state_ = STATE_READ_HEADERS_FOR_COPY;

  int result = DoLoop(net::OK);

  // Synchronous completions are always STATE_DONE.
  if (result != net::ERR_IO_PENDING)
    DCHECK_EQ(STATE_DONE, state_);

  // Asynchronous completion means the state machine must be waiting in one of
  // the Done states for an IO operation to complete:
  if (result == net::ERR_IO_PENDING) {
    // Note that STATE_READ_HEADERS_FOR_COMPARE_DONE is excluded because the
    // headers are compared in MaybeWriteHeaders, not here, and
    // STATE_WRITE_HEADERS_FOR_PASSTHROUGH_DONE is excluded because that write
    // is done by MaybeWriteHeaders.
    DCHECK(state_ == STATE_READ_HEADERS_FOR_COPY_DONE ||
           state_ == STATE_READ_DATA_FOR_COPY_DONE ||
           state_ == STATE_WRITE_HEADERS_FOR_COPY_DONE ||
           state_ == STATE_WRITE_DATA_FOR_COPY_DONE ||
           state_ == STATE_WRITE_DATA_FOR_PASSTHROUGH_DONE)
        << "Unexpected state: " << state_;
  }

  return result >= 0 ? net::OK : static_cast<net::Error>(result);
}

net::Error ServiceWorkerCacheWriter::StartCopy(
    OnWriteCompleteCallback callback) {
  DCHECK(IsCopying());
  pending_callback_ = std::move(callback);

  int result = DoLoop(net::OK);

  // Synchronous completions are always STATE_DONE.
  if (result != net::ERR_IO_PENDING)
    DCHECK_EQ(STATE_DONE, state_);

  // Asynchronous completion means the state machine must be waiting in one of
  // the Done states for an IO operation to complete:
  if (result == net::ERR_IO_PENDING) {
    DCHECK(state_ == STATE_READ_HEADERS_FOR_COPY_DONE ||
           state_ == STATE_WRITE_HEADERS_FOR_COPY_DONE ||
           state_ == STATE_READ_DATA_FOR_COPY_DONE ||
           state_ == STATE_WRITE_DATA_FOR_COPY_DONE)
        << "Unexpected state: " << state_;
  }

  return result >= 0 ? net::OK : static_cast<net::Error>(result);
}

bool ServiceWorkerCacheWriter::IsCopying() const {
  return !compare_reader_ && copy_reader_;
}

int64_t ServiceWorkerCacheWriter::WriterResourceId() const {
  DCHECK(writer_);
  return writer_->response_id();
}

int ServiceWorkerCacheWriter::DoStart(int result) {
  bytes_written_ = 0;
  if (compare_reader_) {
    state_ = STATE_READ_HEADERS_FOR_COMPARE;
    comparing_ = true;
  } else if (IsCopying()) {
    state_ = STATE_READ_HEADERS_FOR_COPY;
    comparing_ = false;
  } else {
    // No existing reader, just write the headers back directly.
    state_ = STATE_WRITE_HEADERS_FOR_PASSTHROUGH;
    comparing_ = false;
  }
  return net::OK;
}

int ServiceWorkerCacheWriter::DoReadHeadersForCompare(int result) {
  DCHECK_GE(result, 0);
  DCHECK(headers_to_write_);

  headers_to_read_ = new HttpResponseInfoIOBuffer;
  state_ = STATE_READ_HEADERS_FOR_COMPARE_DONE;
  return ReadInfoHelper(compare_reader_, headers_to_read_.get());
}

int ServiceWorkerCacheWriter::DoReadHeadersForCompareDone(int result) {
  if (result < 0) {
    state_ = STATE_DONE;
    return result;
  }
  cached_length_ = headers_to_read_->response_data_size;
  bytes_compared_ = 0;
  state_ = STATE_DONE;
  return net::OK;
}

int ServiceWorkerCacheWriter::DoReadDataForCompare(int result) {
  DCHECK_GE(result, 0);
  DCHECK(data_to_write_);

  data_to_read_ = base::MakeRefCounted<net::IOBuffer>(len_to_write_);
  len_to_read_ = len_to_write_;
  state_ = STATE_READ_DATA_FOR_COMPARE_DONE;
  compare_offset_ = 0;
  // If this was an EOF, don't issue a read.
  if (len_to_write_ > 0)
    result = ReadDataHelper(compare_reader_, data_to_read_.get(), len_to_read_);
  return result;
}

int ServiceWorkerCacheWriter::DoReadDataForCompareDone(int result) {
  DCHECK(data_to_read_);
  DCHECK(data_to_write_);
  DCHECK_EQ(len_to_read_, len_to_write_);

  if (result < 0) {
    state_ = STATE_DONE;
    return result;
  }

  DCHECK_LE(result + compare_offset_, static_cast<size_t>(len_to_write_));

  // Premature EOF while reading the service worker script cache data to
  // compare. Fail the comparison.
  if (result == 0 && len_to_write_ != 0) {
    comparing_ = false;
    state_ =
        pause_when_not_identical_ ? STATE_PAUSING : STATE_READ_HEADERS_FOR_COPY;
    return pause_when_not_identical_ ? net::ERR_IO_PENDING : net::OK;
  }

  // Compare the data from the ServiceWorker script cache to the data from the
  // network.
  if (memcmp(data_to_read_->data(), data_to_write_->data() + compare_offset_,
             result)) {
    // Data mismatched. This method already validated that all the bytes through
    // |bytes_compared_| were identical, so copy the first |bytes_compared_|
    // over, then start writing network data back after the changed point.
    comparing_ = false;
    state_ =
        pause_when_not_identical_ ? STATE_PAUSING : STATE_READ_HEADERS_FOR_COPY;
    return pause_when_not_identical_ ? net::ERR_IO_PENDING : net::OK;
  }

  compare_offset_ += result;

  // This is a little bit tricky. It is possible that not enough data was read
  // to finish comparing the entire block of data from the network (which is
  // kept in len_to_write_), so this method may need to issue another read and
  // return to this state.
  //
  // Compare isn't complete yet. Issue another read for the remaining data. Note
  // that this reuses the same IOBuffer.
  if (compare_offset_ < static_cast<size_t>(len_to_read_)) {
    state_ = STATE_READ_DATA_FOR_COMPARE_DONE;
    return ReadDataHelper(compare_reader_, data_to_read_.get(),
                          len_to_read_ - compare_offset_);
  }

  // Cached entry is longer than the network entry but the prefix matches. Copy
  // just the prefix.
  if (len_to_read_ == 0 && bytes_compared_ + compare_offset_ < cached_length_) {
    comparing_ = false;
    state_ =
        pause_when_not_identical_ ? STATE_PAUSING : STATE_READ_HEADERS_FOR_COPY;
    return pause_when_not_identical_ ? net::ERR_IO_PENDING : net::OK;
  }

  bytes_compared_ += compare_offset_;
  state_ = STATE_DONE;
  return net::OK;
}

int ServiceWorkerCacheWriter::DoReadHeadersForCopy(int result) {
  DCHECK_GE(result, 0);
  DCHECK(copy_reader_);
  bytes_copied_ = 0;
  headers_to_read_ = new HttpResponseInfoIOBuffer;
  data_to_copy_ = base::MakeRefCounted<net::IOBuffer>(kCopyBufferSize);
  state_ = STATE_READ_HEADERS_FOR_COPY_DONE;
  return ReadInfoHelper(copy_reader_, headers_to_read_.get());
}

int ServiceWorkerCacheWriter::DoReadHeadersForCopyDone(int result) {
  if (result < 0) {
    state_ = STATE_DONE;
    return result;
  }
  state_ = STATE_WRITE_HEADERS_FOR_COPY;
  return net::OK;
}

// Write the just-read headers back to the cache.
// Note that this *discards* the read headers and replaces them with the net
// headers if the cache writer is not for copy, otherwise write the read
// headers.
int ServiceWorkerCacheWriter::DoWriteHeadersForCopy(int result) {
  DCHECK_GE(result, 0);
  DCHECK(writer_);
  state_ = STATE_WRITE_HEADERS_FOR_COPY_DONE;
  return WriteInfo(IsCopying() ? headers_to_read_ : headers_to_write_);
}

int ServiceWorkerCacheWriter::DoWriteHeadersForCopyDone(int result) {
  if (result < 0) {
    state_ = STATE_DONE;
    return result;
  }
  state_ = STATE_READ_DATA_FOR_COPY;
  return net::OK;
}

int ServiceWorkerCacheWriter::DoReadDataForCopy(int result) {
  DCHECK_GE(result, 0);

  // If the cache writer is only for copy, get the total size to read from
  // header data instead of |bytes_compared_| as no comparison is done.
  size_t total_size_to_read =
      IsCopying() ? headers_to_read_->response_data_size : bytes_compared_;
  size_t to_read =
      std::min(kCopyBufferSize, total_size_to_read - bytes_copied_);

  // At this point, all compared bytes have been read. Currently
  // If the cache write is not just for copy, |data_to_write_| and
  // |len_to_write_| hold the chunk of network input that caused the comparison
  // failure, so those need to be written back and this object needs to go into
  // passthrough mode. If the cache writer is just for copy, change state to
  // STATE_DONE as there is no more data to copy.
  if (to_read == 0) {
    state_ = IsCopying() ? STATE_DONE : STATE_WRITE_DATA_FOR_PASSTHROUGH;
    return net::OK;
  }
  state_ = STATE_READ_DATA_FOR_COPY_DONE;
  return ReadDataHelper(copy_reader_, data_to_copy_.get(), to_read);
}

int ServiceWorkerCacheWriter::DoReadDataForCopyDone(int result) {
  if (result < 0) {
    state_ = STATE_DONE;
    return result;
  }
  state_ = STATE_WRITE_DATA_FOR_COPY;
  return result;
}

int ServiceWorkerCacheWriter::DoWriteDataForCopy(int result) {
  state_ = STATE_WRITE_DATA_FOR_COPY_DONE;
  DCHECK_GT(result, 0);
  return WriteData(data_to_copy_, result);
}

int ServiceWorkerCacheWriter::DoWriteDataForCopyDone(int result) {
  if (result < 0) {
    state_ = STATE_DONE;
    return result;
  }
  bytes_written_ += result;
  bytes_copied_ += result;
  state_ = STATE_READ_DATA_FOR_COPY;
  return result;
}

int ServiceWorkerCacheWriter::DoWriteHeadersForPassthrough(int result) {
  DCHECK_GE(result, 0);
  DCHECK(writer_);
  state_ = STATE_WRITE_HEADERS_FOR_PASSTHROUGH_DONE;
  return WriteInfo(headers_to_write_);
}

int ServiceWorkerCacheWriter::DoWriteHeadersForPassthroughDone(int result) {
  state_ = STATE_DONE;
  return result;
}

int ServiceWorkerCacheWriter::DoWriteDataForPassthrough(int result) {
  DCHECK_GE(result, 0);
  state_ = STATE_WRITE_DATA_FOR_PASSTHROUGH_DONE;
  if (len_to_write_ > 0)
    result = WriteData(data_to_write_, len_to_write_);
  return result;
}

int ServiceWorkerCacheWriter::DoWriteDataForPassthroughDone(int result) {
  if (result < 0) {
    state_ = STATE_DONE;
    return result;
  }
  bytes_written_ += result;
  state_ = STATE_DONE;
  return net::OK;
}

int ServiceWorkerCacheWriter::DoDone(int result) {
  state_ = STATE_DONE;
  return result;
}

// These helpers adapt the AppCache "always use the callback" pattern to the
// //net "only use the callback for async" pattern using
// AsyncCompletionCallbackAdaptor.
//
// Specifically, these methods return result codes directly for synchronous
// completions, and only run their callback (which is AsyncDoLoop) for
// asynchronous completions.

int ServiceWorkerCacheWriter::ReadInfoHelper(
    const std::unique_ptr<ServiceWorkerResponseReader>& reader,
    HttpResponseInfoIOBuffer* buf) {
  net::CompletionOnceCallback run_callback = base::BindOnce(
      &ServiceWorkerCacheWriter::AsyncDoLoop, weak_factory_.GetWeakPtr());
  scoped_refptr<AsyncOnlyCompletionCallbackAdaptor> adaptor(
      new AsyncOnlyCompletionCallbackAdaptor(std::move(run_callback)));
  reader->ReadInfo(
      buf, base::BindOnce(&AsyncOnlyCompletionCallbackAdaptor::WrappedCallback,
                          adaptor));
  adaptor->set_async(true);
  return adaptor->result();
}

int ServiceWorkerCacheWriter::ReadDataHelper(
    const std::unique_ptr<ServiceWorkerResponseReader>& reader,
    net::IOBuffer* buf,
    int buf_len) {
  net::CompletionOnceCallback run_callback = base::BindOnce(
      &ServiceWorkerCacheWriter::AsyncDoLoop, weak_factory_.GetWeakPtr());
  scoped_refptr<AsyncOnlyCompletionCallbackAdaptor> adaptor(
      new AsyncOnlyCompletionCallbackAdaptor(std::move(run_callback)));
  reader->ReadData(
      buf, buf_len,
      base::BindOnce(&AsyncOnlyCompletionCallbackAdaptor::WrappedCallback,
                     adaptor));
  adaptor->set_async(true);
  return adaptor->result();
}

int ServiceWorkerCacheWriter::WriteInfoToResponseWriter(
    scoped_refptr<HttpResponseInfoIOBuffer> response_info) {
  did_replace_ = true;
  net::CompletionOnceCallback run_callback = base::BindOnce(
      &ServiceWorkerCacheWriter::AsyncDoLoop, weak_factory_.GetWeakPtr());
  scoped_refptr<AsyncOnlyCompletionCallbackAdaptor> adaptor(
      new AsyncOnlyCompletionCallbackAdaptor(std::move(run_callback)));
  writer_->WriteInfo(
      response_info.get(),
      base::BindOnce(&AsyncOnlyCompletionCallbackAdaptor::WrappedCallback,
                     adaptor));
  adaptor->set_async(true);
  return adaptor->result();
}

int ServiceWorkerCacheWriter::WriteInfo(
    scoped_refptr<HttpResponseInfoIOBuffer> response_info) {
  if (!write_observer_)
    return WriteInfoToResponseWriter(std::move(response_info));

  int result = write_observer_->WillWriteInfo(response_info);
  if (result != net::OK) {
    DCHECK_NE(result, net::ERR_IO_PENDING);
    state_ = STATE_DONE;
    return result;
  }

  return WriteInfoToResponseWriter(std::move(response_info));
}

int ServiceWorkerCacheWriter::WriteDataToResponseWriter(
    scoped_refptr<net::IOBuffer> data,
    int length) {
  net::CompletionOnceCallback run_callback = base::BindOnce(
      &ServiceWorkerCacheWriter::AsyncDoLoop, weak_factory_.GetWeakPtr());
  scoped_refptr<AsyncOnlyCompletionCallbackAdaptor> adaptor(
      new AsyncOnlyCompletionCallbackAdaptor(std::move(run_callback)));
  writer_->WriteData(
      data.get(), length,
      base::BindOnce(&AsyncOnlyCompletionCallbackAdaptor::WrappedCallback,
                     adaptor));
  adaptor->set_async(true);
  return adaptor->result();
}

int ServiceWorkerCacheWriter::WriteData(scoped_refptr<net::IOBuffer> data,
                                        int length) {
  if (!write_observer_)
    return WriteDataToResponseWriter(std::move(data), length);

  auto complete_callback =
      base::BindOnce(&ServiceWorkerCacheWriter::OnWillWriteDataCompleted,
                     weak_factory_.GetWeakPtr(), data, length);

  int result = write_observer_->WillWriteData(data, length,
                                              std::move(complete_callback));

  if (result == net::OK)
    return WriteDataToResponseWriter(std::move(data), length);

  if (result < 0 && result != net::ERR_IO_PENDING)
    state_ = STATE_DONE;

  return result;
}

// AsyncDoLoop() may need to be called to continue the state machine.
void ServiceWorkerCacheWriter::OnWillWriteDataCompleted(
    scoped_refptr<net::IOBuffer> data,
    int length,
    net::Error error) {
  DCHECK_NE(error, net::ERR_IO_PENDING);
  io_pending_ = false;
  if (error != net::OK) {
    state_ = STATE_DONE;
    AsyncDoLoop(error);
    return;
  }

  int result = WriteDataToResponseWriter(std::move(data), length);
  if (result != net::ERR_IO_PENDING)
    AsyncDoLoop(result);
}

void ServiceWorkerCacheWriter::AsyncDoLoop(int result) {
  result = DoLoop(result);
  // If the result is ERR_IO_PENDING, the pending callback will be run by a
  // later invocation of AsyncDoLoop.
  if (result != net::ERR_IO_PENDING) {
    OnWriteCompleteCallback callback = std::move(pending_callback_);
    net::Error error = result >= 0 ? net::OK : static_cast<net::Error>(result);
    io_pending_ = false;
    std::move(callback).Run(error);
    return;
  }
  if (state_ == STATE_PAUSING) {
    DCHECK(pause_when_not_identical_);
    OnWriteCompleteCallback callback = std::move(pending_callback_);
    std::move(callback).Run(net::ERR_IO_PENDING);
  }
}

}  // namespace content
