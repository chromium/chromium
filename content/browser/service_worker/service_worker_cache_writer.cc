// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/service_worker/service_worker_cache_writer.h"

#include <algorithm>
#include <string>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/completion_once_callback.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace {

const size_t kCopyBufferSize = 16 * 1024;

// Shim class used to turn always-async functions into async-or-result
// functions. See the comments below near ReadResponseHead.
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

// Similar to AsyncOnlyCompletionCallbackAdaptor but specialized for
// ReadResponseHead.
class ServiceWorkerCacheWriter::ReadResponseHeadCallbackAdapter
    : public base::RefCounted<
          ServiceWorkerCacheWriter::ReadResponseHeadCallbackAdapter> {
 public:
  explicit ReadResponseHeadCallbackAdapter(
      base::WeakPtr<ServiceWorkerCacheWriter> owner)
      : owner_(std::move(owner)) {
    DCHECK(owner_);
  }

  void DidReadResponseHead(int result,
                           network::mojom::URLResponseHeadPtr response_head,
                           std::optional<mojo_base::BigBuffer>) {
    result_ = result;
    if (!owner_)
      return;
    owner_->response_head_to_read_ = std::move(response_head);
    if (async_)
      owner_->AsyncDoLoop(result);
  }

  void SetAsync() { async_ = true; }
  int result() { return result_; }

 private:
  friend class base::RefCounted<ReadResponseHeadCallbackAdapter>;
  virtual ~ReadResponseHeadCallbackAdapter() = default;

  base::WeakPtr<ServiceWorkerCacheWriter> owner_;
  bool async_ = false;
  int result_ = net::ERR_IO_PENDING;
};

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
        NOTREACHED_IN_MIGRATION() << "Unknown state in DoLoop";
        state_ = STATE_DONE;
        break;
    }
  } while (status != net::ERR_IO_PENDING && state_ != STATE_DONE);
  io_pending_ = (status == net::ERR_IO_PENDING);
  return status;
}

ServiceWorkerCacheWriter::ServiceWorkerCacheWriter(
    mojo::Remote<storage::mojom::ServiceWorkerResourceReader> compare_reader,
    mojo::Remote<storage::mojom::ServiceWorkerResourceReader> copy_reader,
    mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> writer,
    int64_t writer_resource_id,
    bool pause_when_not_identical,
    ChecksumUpdateTiming checksum_update_timing)
    : state_(STATE_START),
      io_pending_(false),
      comparing_(false),
      pause_when_not_identical_(pause_when_not_identical),
      compare_reader_(std::move(compare_reader)),
      copy_reader_(std::move(copy_reader)),
      writer_(std::move(writer)),
      writer_resource_id_(writer_resource_id),
      checksum_update_timing_(checksum_update_timing),
      checksum_(crypto::SecureHash::Create(crypto::SecureHash::SHA256)) {
  if (compare_reader_) {
    compare_reader_.set_disconnect_handler(
        base::BindOnce(&ServiceWorkerCacheWriter::OnRemoteDisconnected,
                       weak_factory_.GetWeakPtr()));
  }
  if (copy_reader_) {
    copy_reader_.set_disconnect_handler(
        base::BindOnce(&ServiceWorkerCacheWriter::OnRemoteDisconnected,
                       weak_factory_.GetWeakPtr()));
  }
  DCHECK(writer_);
  writer_.set_disconnect_handler(
      base::BindOnce(&ServiceWorkerCacheWriter::OnRemoteDisconnected,
                     weak_factory_.GetWeakPtr()));
}

ServiceWorkerCacheWriter::~ServiceWorkerCacheWriter() {}

std::unique_ptr<ServiceWorkerCacheWriter>
ServiceWorkerCacheWriter::CreateForCopy(
    mojo::Remote<storage::mojom::ServiceWorkerResourceReader> copy_reader,
    mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> writer,
    int64_t writer_resource_id) {
  DCHECK(copy_reader);
  DCHECK(copy_reader.is_connected());
  DCHECK(writer);
  DCHECK(writer.is_connected());
  mojo::Remote<storage::mojom::ServiceWorkerResourceReader> null_remote;
  return base::WrapUnique(new ServiceWorkerCacheWriter(
      std::move(null_remote) /* compare_reader */, std::move(copy_reader),
      std::move(writer), writer_resource_id,
      /*pause_when_not_identical=*/false,
      ChecksumUpdateTiming::kCacheMismatch));
}

std::unique_ptr<ServiceWorkerCacheWriter>
ServiceWorkerCacheWriter::CreateForWriteBack(
    mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> writer,
    int64_t writer_resource_id) {
  DCHECK(writer);
  DCHECK(writer.is_connected());
  return base::WrapUnique(new ServiceWorkerCacheWriter(
      /*compare_reader=*/{}, /*copy_reader=*/{}, std::move(writer),
      writer_resource_id,
      /*pause_when_not_identical=*/false,
      ChecksumUpdateTiming::kCacheMismatch));
}

std::unique_ptr<ServiceWorkerCacheWriter>
ServiceWorkerCacheWriter::CreateForComparison(
    mojo::Remote<storage::mojom::ServiceWorkerResourceReader> compare_reader,
    mojo::Remote<storage::mojom::ServiceWorkerResourceReader> copy_reader,
    mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> writer,
    int64_t writer_resource_id,
    bool pause_when_not_identical,
    ChecksumUpdateTiming checksum_update_timing) {
  // |compare_reader| reads data for the comparison. |copy_reader| reads
  // data for copy.
  DCHECK(compare_reader);
  DCHECK(compare_reader.is_connected());
  DCHECK(copy_reader);
  DCHECK(copy_reader.is_connected());
  DCHECK(writer);
  DCHECK(writer.is_connected());
  return base::WrapUnique(new ServiceWorkerCacheWriter(
      std::move(compare_reader), std::move(copy_reader), std::move(writer),
      writer_resource_id, pause_when_not_identical, checksum_update_timing));
}

net::Error ServiceWorkerCacheWriter::MaybeWriteHeaders(
    network::mojom::URLResponseHeadPtr response_head,
    OnWriteCompleteCallback callback) {
  DCHECK(!io_pending_);
  DCHECK(!IsCopying());

  if (!writer_.is_connected())
    return net::ERR_FAILED;

  response_head_to_write_ = std::move(response_head);
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

  if (!writer_.is_connected())
    return net::ERR_FAILED;

  data_to_write_ = buf;
  len_to_write_ = buf_size;
  pending_callback_ = std::move(callback);

  if (checksum_update_timing_ == ChecksumUpdateTiming::kAlways &&
      len_to_write_ > 0) {
    checksum_->Update(data_to_write_->data(), len_to_write_);
  }

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

  if (!copy_reader_.is_connected())
    return net::ERR_FAILED;

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

  if (!copy_reader_.is_connected())
    return net::ERR_FAILED;

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

void ServiceWorkerCacheWriter::FlushRemotesForTesting() {
  if (copy_reader_)
    copy_reader_.FlushForTesting();  // IN-TEST
  if (compare_reader_)
    compare_reader_.FlushForTesting();  // IN-TEST
  DCHECK(writer_);
  writer_.FlushForTesting();  // IN-TEST
}

int64_t ServiceWorkerCacheWriter::writer_resource_id() const {
  DCHECK_NE(writer_resource_id_, blink::mojom::kInvalidServiceWorkerResourceId);
  return writer_resource_id_;
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
  DCHECK(response_head_to_write_);
  DCHECK(compare_reader_);

  if (!compare_reader_.is_connected()) {
    state_ = STATE_DONE;
    return net::ERR_FAILED;
  }

  state_ = STATE_READ_HEADERS_FOR_COMPARE_DONE;
  return ReadResponseHead(compare_reader_.get());
}

int ServiceWorkerCacheWriter::DoReadHeadersForCompareDone(int result) {
  if (result < 0) {
    state_ = STATE_DONE;
    return result;
  }
  DCHECK(response_head_to_read_);
  cached_length_ = response_head_to_read_->content_length;
  bytes_compared_ = 0;
  state_ = STATE_DONE;
  return net::OK;
}

int ServiceWorkerCacheWriter::DoReadDataForCompare(int result) {
  DCHECK_GE(result, 0);
  DCHECK(data_to_write_);

  data_to_read_ = base::MakeRefCounted<net::IOBufferWithSize>(len_to_write_);
  len_to_read_ = len_to_write_;
  state_ = STATE_READ_DATA_FOR_COMPARE_DONE;
  compare_offset_ = 0;
  // If this was an EOF, don't issue a read.
  if (len_to_write_ > 0) {
    DCHECK(compare_reader_);
    if (!compare_reader_.is_connected()) {
      state_ = STATE_DONE;
      return net::ERR_FAILED;
    }
    result = ReadDataHelper(compare_reader_.get(), compare_data_pipe_reader_,
                            data_to_read_, len_to_read_);
  }
  return result;
}

int ServiceWorkerCacheWriter::DoReadDataForCompareDone(int result) {
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

  DCHECK(data_to_read_);
  DCHECK(data_to_write_);

  // Compare the data from the ServiceWorker script cache to the data from the
  // network.
  if (!std::equal(data_to_read_->data(), data_to_read_->data() + result,
                  data_to_write_->data() + compare_offset_)) {
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
    DCHECK(compare_reader_);
    if (!compare_reader_.is_connected()) {
      state_ = STATE_DONE;
      return net::ERR_FAILED;
    }
    state_ = STATE_READ_DATA_FOR_COMPARE_DONE;
    return ReadDataHelper(compare_reader_.get(), compare_data_pipe_reader_,
                          data_to_read_.get(), len_to_read_ - compare_offset_);
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

  if (!copy_reader_.is_connected()) {
    state_ = STATE_DONE;
    return net::ERR_FAILED;
  }

  bytes_copied_ = 0;
  data_to_copy_ = base::MakeRefCounted<net::IOBufferWithSize>(kCopyBufferSize);
  state_ = STATE_READ_HEADERS_FOR_COPY_DONE;
  return ReadResponseHead(copy_reader_.get());
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
  if (IsCopying()) {
    DCHECK(response_head_to_read_);
    bytes_to_copy_ = response_head_to_read_->content_length;
    return WriteResponseHead(std::move(response_head_to_read_));
  } else {
    DCHECK(response_head_to_write_);
    return WriteResponseHead(std::move(response_head_to_write_));
  }
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
  DCHECK(copy_reader_);

  if (!copy_reader_.is_connected()) {
    state_ = STATE_DONE;
    return net::ERR_FAILED;
  }

  // If the cache writer is only for copy, get the total size to read from
  // header data instead of |bytes_compared_| as no comparison is done.
  size_t total_size_to_read = IsCopying() ? bytes_to_copy_ : bytes_compared_;
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
  return ReadDataHelper(copy_reader_.get(), copy_data_pipe_reader_,
                        data_to_copy_.get(), to_read);
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
  CHECK_GT(result, 0);
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
  DCHECK(response_head_to_write_);
  state_ = STATE_WRITE_HEADERS_FOR_PASSTHROUGH_DONE;
  return WriteResponseHead(std::move(response_head_to_write_));
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

// These methods adapt the AppCache "always use the callback" pattern to the
// //net "only use the callback for async" pattern using
// AsyncCompletionCallbackAdaptor and ReadResponseHeadCallbackAdaptor.
//
// Specifically, these methods return result codes directly for synchronous
// completions, and only run their callback (which is AsyncDoLoop) for
// asynchronous completions.

int ServiceWorkerCacheWriter::ReadResponseHead(
    storage::mojom::ServiceWorkerResourceReader* reader) {
  auto adapter = base::MakeRefCounted<ReadResponseHeadCallbackAdapter>(
      weak_factory_.GetWeakPtr());
  reader->ReadResponseHead(base::BindOnce(
      &ReadResponseHeadCallbackAdapter::DidReadResponseHead, adapter));
  adapter->SetAsync();
  return adapter->result();
}

class ServiceWorkerCacheWriter::DataPipeReader {
 public:
  DataPipeReader(storage::mojom::ServiceWorkerResourceReader* reader,
                 ServiceWorkerCacheWriter* owner,
                 scoped_refptr<base::SequencedTaskRunner> runner)
      : reader_(reader),
        owner_(owner),
        watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL, runner),
        task_runner_(runner) {}

  // Reads the body up to |num_bytes| bytes. |callback| is always called
  // asynchronously.
  using ReadCallback = base::OnceCallback<void(int /* result */)>;
  void Read(scoped_refptr<net::IOBuffer> buffer,
            int num_bytes,
            ReadCallback callback) {
    DCHECK(buffer);
    buffer_ = std::move(buffer);
    num_bytes_to_read_ = base::checked_cast<size_t>(num_bytes);
    callback_ = std::move(callback);

    if (!data_.is_valid()) {
      // This is the initial call of Read(). Call PrepareReadData() to get a
      // data pipe to read the body.
      reader_->PrepareReadData(
          -1, base::BindOnce(
                  &ServiceWorkerCacheWriter::DataPipeReader::OnReadDataPrepared,
                  weak_factory_.GetWeakPtr()));
      return;
    }
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceWorkerCacheWriter::DataPipeReader::ReadInternal,
                       weak_factory_.GetWeakPtr(), MOJO_RESULT_OK));
  }

 private:
  void ReadInternal(MojoResult) {
    MojoResult result = data_->ReadData(
        MOJO_READ_DATA_FLAG_NONE, buffer_->span().first(num_bytes_to_read_),
        num_bytes_to_read_);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      watcher_.ArmOrNotify();
      return;
    }
    if (result != MOJO_RESULT_OK) {
      // Disconnected means it's the end of the body or an error occurs during
      // reading the body.
      // TODO(crbug.com/40120038): notify of errors.
      num_bytes_to_read_ = 0;
    }
    owner_->AsyncDoLoop(base::checked_cast<int>(num_bytes_to_read_));
  }

  void OnReadDataPrepared(mojo::ScopedDataPipeConsumerHandle data) {
    // An invalid handle can be returned when creating a data pipe fails on the
    // other side of the endpoint.
    if (!data) {
      owner_->AsyncDoLoop(net::ERR_FAILED);
      return;
    }

    data_ = std::move(data);
    watcher_.Watch(data_.get(),
                   MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                   base::BindRepeating(
                       &ServiceWorkerCacheWriter::DataPipeReader::ReadInternal,
                       weak_factory_.GetWeakPtr()));
    ReadInternal(MOJO_RESULT_OK);

    // TODO(crbug.com/40120038): provide a callback to notify of errors
    // if any.
    reader_->ReadData({});
  }

  // Parameters set on Read().
  scoped_refptr<net::IOBuffer> buffer_;
  size_t num_bytes_to_read_ = 0;
  ReadCallback callback_;

  // |reader_| is safe to be kept as a rawptr because |owner_| owns |this| and
  // |reader_|, and |owner_| keeps |reader_| until it's destroyed.
  const raw_ptr<storage::mojom::ServiceWorkerResourceReader> reader_;
  const raw_ptr<ServiceWorkerCacheWriter> owner_;

  // Mojo data pipe and the watcher is set up when Read() is called for the
  // first time.
  mojo::ScopedDataPipeConsumerHandle data_;
  mojo::SimpleWatcher watcher_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<DataPipeReader> weak_factory_{this};
};

int ServiceWorkerCacheWriter::ReadDataHelper(
    storage::mojom::ServiceWorkerResourceReader* reader,
    std::unique_ptr<DataPipeReader>& data_pipe_reader,
    scoped_refptr<net::IOBuffer> buf,
    int buf_len) {
  if (!data_pipe_reader) {
    data_pipe_reader = std::make_unique<DataPipeReader>(
        reader, this, base::SequencedTaskRunner::GetCurrentDefault());
  }
  data_pipe_reader->Read(buf, buf_len,
                         base::BindOnce(&ServiceWorkerCacheWriter::AsyncDoLoop,
                                        weak_factory_.GetWeakPtr()));
  return net::ERR_IO_PENDING;
}

int ServiceWorkerCacheWriter::WriteResponseHeadToResponseWriter(
    network::mojom::URLResponseHeadPtr response_head) {
  if (!writer_.is_connected()) {
    state_ = STATE_DONE;
    return net::ERR_FAILED;
  }

  DCHECK(response_head);
  did_replace_ = true;
  net::CompletionOnceCallback run_callback = base::BindOnce(
      &ServiceWorkerCacheWriter::AsyncDoLoop, weak_factory_.GetWeakPtr());
  scoped_refptr<AsyncOnlyCompletionCallbackAdaptor> adaptor(
      new AsyncOnlyCompletionCallbackAdaptor(std::move(run_callback)));
  writer_->WriteResponseHead(
      std::move(response_head),
      base::BindOnce(&AsyncOnlyCompletionCallbackAdaptor::WrappedCallback,
                     adaptor));
  adaptor->set_async(true);
  return adaptor->result();
}

int ServiceWorkerCacheWriter::WriteResponseHead(
    network::mojom::URLResponseHeadPtr response_head) {
  DCHECK(response_head);
  if (write_observer_) {
    int result = write_observer_->WillWriteResponseHead(*response_head);
    if (result != net::OK) {
      DCHECK_NE(result, net::ERR_IO_PENDING);
      state_ = STATE_DONE;
      return result;
    }
  }
  return WriteResponseHeadToResponseWriter(std::move(response_head));
}

int ServiceWorkerCacheWriter::WriteDataToResponseWriter(
    scoped_refptr<net::IOBuffer> data,
    size_t length) {
  if (!writer_.is_connected()) {
    state_ = STATE_DONE;
    return net::ERR_FAILED;
  }

  net::CompletionOnceCallback run_callback = base::BindOnce(
      &ServiceWorkerCacheWriter::AsyncDoLoop, weak_factory_.GetWeakPtr());
  scoped_refptr<AsyncOnlyCompletionCallbackAdaptor> adaptor(
      new AsyncOnlyCompletionCallbackAdaptor(std::move(run_callback)));

  // If |checksum_update_timing_| is kAlways, the checksum update should be
  // handled in MaybeWriteData().
  if (checksum_update_timing_ == ChecksumUpdateTiming::kCacheMismatch) {
    checksum_->Update(data->data(), length);
  }

  mojo_base::BigBuffer big_buffer(
      base::as_bytes(base::make_span(data->data(), length)));
  writer_->WriteData(
      std::move(big_buffer),
      base::BindOnce(&AsyncOnlyCompletionCallbackAdaptor::WrappedCallback,
                     adaptor));
  adaptor->set_async(true);
  return adaptor->result();
}

int ServiceWorkerCacheWriter::WriteData(scoped_refptr<net::IOBuffer> data,
                                        size_t length) {
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
    size_t length,
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

void ServiceWorkerCacheWriter::OnRemoteDisconnected() {
  if (state_ == STATE_DONE)
    return;

  state_ = STATE_DONE;
  AsyncDoLoop(net::ERR_FAILED);
}

void ServiceWorkerCacheWriter::AsyncDoLoop(int result) {
  result = DoLoop(result);
  // If the result is ERR_IO_PENDING, the pending callback will be run by a
  // later invocation of AsyncDoLoop.
  if (result != net::ERR_IO_PENDING) {
    io_pending_ = false;
    // `pending_callback_` might be already consumed when mojo remotes are
    // disconnected.
    if (pending_callback_) {
      OnWriteCompleteCallback callback = std::move(pending_callback_);
      net::Error error =
          result >= 0 ? net::OK : static_cast<net::Error>(result);
      std::move(callback).Run(error);
    }
    return;
  }
  if (state_ == STATE_PAUSING) {
    DCHECK(pause_when_not_identical_);
    OnWriteCompleteCallback callback = std::move(pending_callback_);
    std::move(callback).Run(net::ERR_IO_PENDING);
  }
}

std::string ServiceWorkerCacheWriter::GetSha256Checksum() {
  DCHECK_EQ(STATE_DONE, state_);
  DCHECK(checksum_);
  uint8_t result[crypto::kSHA256Length];
  checksum_->Finish(result, crypto::kSHA256Length);
  checksum_ = nullptr;
  return base::HexEncode(result);
}

}  // namespace content
