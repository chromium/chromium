// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_resource_ops.h"

#include "base/numerics/checked_math.h"
#include "base/pickle.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/cpp/net_adapters.h"
#include "third_party/blink/public/common/blob/blob_utils.h"

namespace content {

namespace {

// Disk cache entry data indices.
//
// This enum pertains to data persisted on disk. Do not remove or reuse values.
enum {
  kResponseInfoIndex = 0,
  kResponseContentIndex = 1,
  kResponseMetadataIndex = 2,
};

// Convert an HttpResponseInfo retrieved from disk_cache to URLResponseHead.
network::mojom::URLResponseHeadPtr ConvertHttpResponseInfo(
    const net::HttpResponseInfo& http_info,
    int64_t response_data_size) {
  auto response_head = network::mojom::URLResponseHead::New();

  response_head->request_time = http_info.request_time;
  response_head->response_time = http_info.response_time;
  response_head->headers = http_info.headers;
  response_head->headers->GetMimeType(&response_head->mime_type);
  response_head->headers->GetCharset(&response_head->charset);
  response_head->content_length = response_data_size;
  response_head->was_fetched_via_spdy = http_info.was_fetched_via_spdy;
  response_head->was_alpn_negotiated = http_info.was_alpn_negotiated;
  response_head->connection_info = http_info.connection_info;
  response_head->alpn_negotiated_protocol = http_info.alpn_negotiated_protocol;
  response_head->remote_endpoint = http_info.remote_endpoint;
  response_head->cert_status = http_info.ssl_info.cert_status;
  response_head->ssl_info = http_info.ssl_info;

  return response_head;
}

}  // namespace

// BigBuffer backed IOBuffer.
class BigIOBuffer : public net::IOBufferWithSize {
 public:
  explicit BigIOBuffer(mojo_base::BigBuffer buffer);

  BigIOBuffer(const BigIOBuffer&) = delete;
  BigIOBuffer& operator=(const BigIOBuffer&) = delete;

  mojo_base::BigBuffer TakeBuffer();

 protected:
  ~BigIOBuffer() override;

 private:
  mojo_base::BigBuffer buffer_;
};

BigIOBuffer::BigIOBuffer(mojo_base::BigBuffer buffer)
    : net::IOBufferWithSize(nullptr, buffer.size()),
      buffer_(std::move(buffer)) {
  data_ = reinterpret_cast<char*>(buffer_.data());
}

BigIOBuffer::~BigIOBuffer() {
  // Reset `data_` to avoid double-free. The base class (IOBuffer) tries to
  // delete it.
  data_ = nullptr;
}

mojo_base::BigBuffer BigIOBuffer::TakeBuffer() {
  data_ = nullptr;
  size_ = 0UL;
  return std::move(buffer_);
}

class ServiceWorkerResourceReaderImpl::DataReader {
 public:
  DataReader(
      base::WeakPtr<ServiceWorkerResourceReaderImpl> owner,
      size_t total_bytes_to_read,
      mojo::PendingRemote<storage::mojom::ServiceWorkerDataPipeStateNotifier>
          notifier,
      mojo::ScopedDataPipeProducerHandle producer_handle)
      : owner_(std::move(owner)),
        total_bytes_to_read_(total_bytes_to_read),
        notifier_(std::move(notifier)),
        producer_handle_(std::move(producer_handle)),
        watcher_(FROM_HERE,
                 mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                 base::SequencedTaskRunnerHandle::Get()) {
    DCHECK(owner_);
    DCHECK(notifier_);
  }
  ~DataReader() = default;

  DataReader(const DataReader&) = delete;
  DataReader operator=(const DataReader&) = delete;

  void Start() {
#if DCHECK_IS_ON()
    DCHECK_EQ(state_, State::kInitialized);
    state_ = State::kStarted;
#endif

    owner_->EnsureEntryIsOpen(base::BindOnce(&DataReader::ContinueReadData,
                                             weak_factory_.GetWeakPtr()));
  }

 private:
  void ContinueReadData() {
#if DCHECK_IS_ON()
    DCHECK_EQ(state_, State::kStarted);
    state_ = State::kCacheEntryOpened;
#endif

    if (!owner_) {
      Complete(net::ERR_ABORTED);
      return;
    }

    if (!owner_->entry_) {
      Complete(net::ERR_CACHE_MISS);
      return;
    }

    watcher_.Watch(producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                   base::BindRepeating(&DataReader::OnWritable,
                                       weak_factory_.GetWeakPtr()));
    watcher_.ArmOrNotify();
  }

  void OnWritable(MojoResult) {
#if DCHECK_IS_ON()
    DCHECK(state_ == State::kCacheEntryOpened || state_ == State::kDataRead);
    state_ = State::kProducerWritable;
#endif

    DCHECK(producer_handle_.is_valid());
    DCHECK(!pending_buffer_);

    if (!owner_ || !owner_->entry_) {
      Complete(net::ERR_ABORTED);
      return;
    }

    uint32_t num_bytes = 0;
    MojoResult rv = network::NetToMojoPendingBuffer::BeginWrite(
        &producer_handle_, &pending_buffer_, &num_bytes);
    switch (rv) {
      case MOJO_RESULT_INVALID_ARGUMENT:
      case MOJO_RESULT_BUSY:
        NOTREACHED();
        return;
      case MOJO_RESULT_FAILED_PRECONDITION:
        Complete(net::ERR_ABORTED);
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        watcher_.ArmOrNotify();
        return;
      case MOJO_RESULT_OK:
        // |producer__handle_| must have been taken by |pending_buffer_|.
        DCHECK(pending_buffer_);
        DCHECK(!producer_handle_.is_valid());
        break;
    }

    num_bytes = std::min(num_bytes, blink::BlobUtils::GetDataPipeChunkSize());
    scoped_refptr<network::NetToMojoIOBuffer> buffer =
        base::MakeRefCounted<network::NetToMojoIOBuffer>(pending_buffer_.get());

    net::IOBuffer* raw_buffer = buffer.get();
    int read_bytes = owner_->entry_->Read(
        kResponseContentIndex, current_bytes_read_, raw_buffer, num_bytes,
        base::BindOnce(&DataReader::DidReadData, weak_factory_.GetWeakPtr(),
                       buffer));
    if (read_bytes != net::ERR_IO_PENDING) {
      DidReadData(std::move(buffer), read_bytes);
    }
  }

  void DidReadData(scoped_refptr<network::NetToMojoIOBuffer> buffer,
                   int read_bytes) {
#if DCHECK_IS_ON()
    DCHECK_EQ(state_, State::kProducerWritable);
    state_ = State::kDataRead;
#endif

    if (read_bytes < 0) {
      Complete(read_bytes);
      return;
    }

    producer_handle_ = pending_buffer_->Complete(read_bytes);
    DCHECK(producer_handle_.is_valid());
    pending_buffer_.reset();
    current_bytes_read_ += read_bytes;

    if (read_bytes == 0 || current_bytes_read_ == total_bytes_to_read_) {
      // All data has been read.
      Complete(current_bytes_read_);
      return;
    }
    watcher_.ArmOrNotify();
  }

  void Complete(int status) {
#if DCHECK_IS_ON()
    DCHECK_NE(state_, State::kComplete);
    state_ = State::kComplete;
#endif

    watcher_.Cancel();
    producer_handle_.reset();

    if (notifier_.is_connected()) {
      notifier_->OnComplete(status);
    }

    if (owner_) {
      owner_->DidReadDataComplete();
    }
  }

  base::WeakPtr<ServiceWorkerResourceReaderImpl> owner_;
  const size_t total_bytes_to_read_;
  size_t current_bytes_read_ = 0;
  mojo::Remote<storage::mojom::ServiceWorkerDataPipeStateNotifier> notifier_;
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  mojo::SimpleWatcher watcher_;
  scoped_refptr<network::NetToMojoPendingBuffer> pending_buffer_;

#if DCHECK_IS_ON()
  enum class State {
    kInitialized,
    kStarted,
    kCacheEntryOpened,
    kProducerWritable,
    kDataRead,
    kComplete,
  };
  State state_ = State::kInitialized;
#endif  // DCHECK_IS_ON()

  base::WeakPtrFactory<DataReader> weak_factory_{this};
};

ServiceWorkerResourceReaderImpl::ServiceWorkerResourceReaderImpl(
    int64_t resource_id,
    base::WeakPtr<AppCacheDiskCache> disk_cache)
    : resource_id_(resource_id), disk_cache_(std::move(disk_cache)) {
  DCHECK_NE(resource_id_, blink::mojom::kInvalidServiceWorkerResourceId);
  DCHECK(disk_cache_);
}

ServiceWorkerResourceReaderImpl::~ServiceWorkerResourceReaderImpl() {
  if (entry_) {
    entry_->Close();
  }
}

void ServiceWorkerResourceReaderImpl::ReadResponseHead(
    ReadResponseHeadCallback callback) {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kIdle);
  state_ = State::kReadResponseHeadStarted;
#endif
  DCHECK(!read_response_head_callback_) << __func__ << " already called";
  DCHECK(!response_head_) << " another ReadResponseHead() in progress";
  DCHECK(!metadata_buffer_);
  DCHECK(!data_reader_);

  read_response_head_callback_ = std::move(callback);
  EnsureEntryIsOpen(
      base::BindOnce(&ServiceWorkerResourceReaderImpl::ContinueReadResponseHead,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerResourceReaderImpl::ReadData(
    int64_t size,
    mojo::PendingRemote<storage::mojom::ServiceWorkerDataPipeStateNotifier>
        notifier,
    ReadDataCallback callback) {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kIdle);
  state_ = State::kReadDataStarted;
#endif
  DCHECK(!read_response_head_callback_) << "ReadResponseHead() being operating";
  DCHECK(!response_head_);
  DCHECK(!metadata_buffer_);
  DCHECK(!data_reader_);

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = blink::BlobUtils::GetDataPipeCapacity(size);

  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  MojoResult rv =
      mojo::CreateDataPipe(&options, &producer_handle, &consumer_handle);
  if (rv != MOJO_RESULT_OK) {
    std::move(callback).Run(mojo::ScopedDataPipeConsumerHandle());
    return;
  }

  data_reader_ = std::make_unique<DataReader>(weak_factory_.GetWeakPtr(), size,
                                              std::move(notifier),
                                              std::move(producer_handle));
  data_reader_->Start();
  std::move(callback).Run(std::move(consumer_handle));
}

void ServiceWorkerResourceReaderImpl::ContinueReadResponseHead() {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kReadResponseHeadStarted);
  state_ = State::kCacheEntryOpened;
#endif
  DCHECK(read_response_head_callback_);

  if (!entry_) {
    FailReadResponseHead(net::ERR_CACHE_MISS);
    return;
  }

  int64_t size = entry_->GetSize(kResponseInfoIndex);
  if (size <= 0) {
    FailReadResponseHead(net::ERR_CACHE_MISS);
    return;
  }

  auto buffer =
      base::MakeRefCounted<net::IOBuffer>(base::checked_cast<size_t>(size));
  int rv = entry_->Read(
      kResponseInfoIndex, /*offset=*/0, buffer.get(), size,
      base::BindOnce(&ServiceWorkerResourceReaderImpl::DidReadHttpResponseInfo,
                     weak_factory_.GetWeakPtr(), buffer));
  if (rv != net::ERR_IO_PENDING) {
    DidReadHttpResponseInfo(std::move(buffer), rv);
  }
}

void ServiceWorkerResourceReaderImpl::DidReadHttpResponseInfo(
    scoped_refptr<net::IOBuffer> buffer,
    int status) {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kCacheEntryOpened);
  state_ = State::kResponseInfoRead;
#endif
  DCHECK(read_response_head_callback_);
  DCHECK(entry_);

  if (status < 0) {
    FailReadResponseHead(status);
    return;
  }

  // Deserialize the http info structure, ensuring we got headers.
  base::Pickle pickle(buffer->data(), status);
  auto http_info = std::make_unique<net::HttpResponseInfo>();
  bool response_truncated = false;
  if (!http_info->InitFromPickle(pickle, &response_truncated) ||
      !http_info->headers.get()) {
    FailReadResponseHead(net::ERR_FAILED);
    return;
  }
  DCHECK(!response_truncated);

  int64_t response_data_size = entry_->GetSize(kResponseContentIndex);

  response_head_ = ConvertHttpResponseInfo(*http_info, response_data_size);

  int64_t metadata_size = entry_->GetSize(kResponseMetadataIndex);
  DCHECK_GE(metadata_size, 0);
  if (metadata_size <= 0) {
    CompleteReadResponseHead(status);
    return;
  }

  // Read metadata.
  metadata_buffer_ = base::MakeRefCounted<BigIOBuffer>(
      mojo_base::BigBuffer(base::checked_cast<size_t>(metadata_size)));
  int rv = entry_->Read(
      kResponseMetadataIndex, /*offset=*/0, metadata_buffer_.get(),
      metadata_size,
      base::BindOnce(&ServiceWorkerResourceReaderImpl::DidReadMetadata,
                     weak_factory_.GetWeakPtr()));
  if (rv != net::ERR_IO_PENDING) {
    DidReadMetadata(rv);
  }
}

void ServiceWorkerResourceReaderImpl::DidReadMetadata(int status) {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kResponseInfoRead);
  state_ = State::kMetadataRead;
#endif
  DCHECK(read_response_head_callback_);
  DCHECK(metadata_buffer_);

  if (status < 0) {
    FailReadResponseHead(status);
    return;
  }

  CompleteReadResponseHead(status);
}

void ServiceWorkerResourceReaderImpl::FailReadResponseHead(int status) {
  DCHECK_NE(net::OK, status);
  response_head_ = nullptr;
  metadata_buffer_ = nullptr;
  CompleteReadResponseHead(status);
}

void ServiceWorkerResourceReaderImpl::CompleteReadResponseHead(int status) {
#if DCHECK_IS_ON()
  DCHECK_NE(state_, State::kIdle);
  state_ = State::kIdle;
#endif
  DCHECK(read_response_head_callback_);

  base::Optional<mojo_base::BigBuffer> metadata =
      metadata_buffer_
          ? base::Optional<mojo_base::BigBuffer>(metadata_buffer_->TakeBuffer())
          : base::nullopt;

  metadata_buffer_ = nullptr;

  std::move(read_response_head_callback_)
      .Run(status, std::move(response_head_), std::move(metadata));
}

void ServiceWorkerResourceReaderImpl::DidReadDataComplete() {
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, State::kReadDataStarted);
  state_ = State::kIdle;
#endif
  DCHECK(data_reader_);
  data_reader_.reset();
}

void ServiceWorkerResourceReaderImpl::EnsureEntryIsOpen(
    base::OnceClosure callback) {
  DCHECK(!open_entry_callback_);
  open_entry_callback_ = std::move(callback);

  int rv;
  AppCacheDiskCacheEntry** entry_ptr = nullptr;
  if (entry_) {
    rv = net::OK;
  } else if (!disk_cache_) {
    rv = net::ERR_FAILED;
  } else {
    entry_ptr = new AppCacheDiskCacheEntry*;
    rv = disk_cache_->OpenEntry(
        resource_id_, entry_ptr,
        base::BindOnce(&DidOpenEntry, weak_factory_.GetWeakPtr(), entry_ptr));
  }

  if (rv != net::ERR_IO_PENDING) {
    DidOpenEntry(weak_factory_.GetWeakPtr(), entry_ptr, rv);
  }
}

// static
void ServiceWorkerResourceReaderImpl::DidOpenEntry(
    base::WeakPtr<ServiceWorkerResourceReaderImpl> reader,
    AppCacheDiskCacheEntry** entry,
    int rv) {
  if (!reader) {
    delete entry;
    return;
  }

  if (!reader->entry_ && rv == net::OK) {
    DCHECK(entry);
    reader->entry_ = *entry;
  }
  delete entry;

  DCHECK(reader->open_entry_callback_);
  std::move(reader->open_entry_callback_).Run();
}

ServiceWorkerResourceWriterImpl::ServiceWorkerResourceWriterImpl(
    std::unique_ptr<ServiceWorkerResponseWriter> writer)
    : writer_(std::move(writer)) {
  DCHECK(writer_);
}

ServiceWorkerResourceWriterImpl::~ServiceWorkerResourceWriterImpl() = default;

void ServiceWorkerResourceWriterImpl::WriteResponseHead(
    network::mojom::URLResponseHeadPtr response_head,
    WriteResponseHeadCallback callback) {
  // Convert URLResponseHead to HttpResponseInfo.
  auto response_info = std::make_unique<net::HttpResponseInfo>();
  response_info->headers = response_head->headers;
  if (response_head->ssl_info.has_value())
    response_info->ssl_info = *response_head->ssl_info;
  response_info->was_fetched_via_spdy = response_head->was_fetched_via_spdy;
  response_info->was_alpn_negotiated = response_head->was_alpn_negotiated;
  response_info->alpn_negotiated_protocol =
      response_head->alpn_negotiated_protocol;
  response_info->connection_info = response_head->connection_info;
  response_info->remote_endpoint = response_head->remote_endpoint;
  response_info->response_time = response_head->response_time;

  auto info_buffer =
      base::MakeRefCounted<HttpResponseInfoIOBuffer>(std::move(response_info));
  writer_->WriteInfo(info_buffer.get(), std::move(callback));
}

void ServiceWorkerResourceWriterImpl::WriteData(mojo_base::BigBuffer data,
                                                WriteDataCallback callback) {
  int buf_len = data.size();
  auto buffer = base::MakeRefCounted<BigIOBuffer>(std::move(data));
  writer_->WriteData(buffer.get(), buf_len, std::move(callback));
}

ServiceWorkerResourceMetadataWriterImpl::
    ServiceWorkerResourceMetadataWriterImpl(
        std::unique_ptr<ServiceWorkerResponseMetadataWriter> writer)
    : writer_(std::move(writer)) {
  DCHECK(writer_);
}

ServiceWorkerResourceMetadataWriterImpl::
    ~ServiceWorkerResourceMetadataWriterImpl() = default;

void ServiceWorkerResourceMetadataWriterImpl::WriteMetadata(
    mojo_base::BigBuffer data,
    WriteMetadataCallback callback) {
  int buf_len = data.size();
  auto buffer = base::MakeRefCounted<BigIOBuffer>(std::move(data));
  writer_->WriteMetadata(buffer.get(), buf_len, std::move(callback));
}

}  // namespace content
