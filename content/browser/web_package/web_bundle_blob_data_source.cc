// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_blob_data_source.h"

#include "base/bit_cast.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/task_traits.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/base/io_buffer.h"
#include "storage/browser/blob/blob_builder_from_stream.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/blob/mojo_blob_reader.h"

namespace content {
namespace {

class MojoBlobReaderDelegate : public storage::MojoBlobReader::Delegate {
 public:
  using CompletionCallback = base::OnceCallback<void(net::Error net_error)>;
  explicit MojoBlobReaderDelegate(CompletionCallback completion_callback)
      : completion_callback_(std::move(completion_callback)) {}

  MojoBlobReaderDelegate(const MojoBlobReaderDelegate&) = delete;
  MojoBlobReaderDelegate& operator=(const MojoBlobReaderDelegate&) = delete;

  ~MojoBlobReaderDelegate() override = default;
  RequestSideData DidCalculateSize(uint64_t total_size,
                                   uint64_t content_size) override {
    return DONT_REQUEST_SIDE_DATA;
  }
  void DidRead(int num_bytes) override {}
  void OnComplete(net::Error result, uint64_t total_written_bytes) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    std::move(completion_callback_).Run(result);
  }

 private:
  CompletionCallback completion_callback_;
};

void OnReadComplete(web_package::mojom::BundleDataSource::ReadCallback callback,
                    std::unique_ptr<storage::BlobReader> blob_reader,
                    scoped_refptr<net::IOBufferWithSize> io_buf,
                    int bytes_read) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (bytes_read != io_buf->size()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  std::vector<uint8_t> vec;
  vec.assign(base::bit_cast<uint8_t*>(io_buf->data()),
             base::bit_cast<uint8_t*>(io_buf->data()) + bytes_read);
  std::move(callback).Run(std::move(vec));
}

void OnCalculateSizeComplete(
    uint64_t offset,
    uint64_t length,
    web_package::mojom::BundleDataSource::ReadCallback callback,
    std::unique_ptr<storage::BlobReader> blob_reader,
    int net_error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (net_error != net::OK) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  if (offset >= blob_reader->total_size()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  uint64_t offset_plus_length;
  if (!base::CheckAdd(offset, length).AssignIfValid(&offset_plus_length)) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  if (offset_plus_length > blob_reader->total_size())
    length = blob_reader->total_size() - offset;

  auto set_read_range_status = blob_reader->SetReadRange(offset, length);
  if (set_read_range_status != storage::BlobReader::Status::DONE) {
    DCHECK_EQ(set_read_range_status, storage::BlobReader::Status::NET_ERROR);
    std::move(callback).Run(absl::nullopt);
    return;
  }
  auto* raw_blob_reader = blob_reader.get();
  auto io_buf =
      base::MakeRefCounted<net::IOBufferWithSize>(static_cast<size_t>(length));
  auto split_callback = base::SplitOnceCallback(base::BindOnce(
      &OnReadComplete, std::move(callback), std::move(blob_reader), io_buf));
  int bytes_read;
  storage::BlobReader::Status read_status =
      raw_blob_reader->Read(io_buf.get(), io_buf->size(), &bytes_read,
                            std::move(split_callback.first));
  if (read_status != storage::BlobReader::Status::IO_PENDING) {
    std::move(split_callback.second).Run(bytes_read);
  }
}

}  // namespace

WebBundleBlobDataSource::WebBundleBlobDataSource(
    uint64_t length_hint,
    mojo::ScopedDataPipeConsumerHandle outer_response_body,
    network::mojom::URLLoaderClientEndpointsPtr endpoints,
    BrowserContext::BlobContextGetter blob_context_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&WebBundleBlobDataSource::CreateCoreOnIO,
                     weak_factory_.GetWeakPtr(), length_hint,
                     std::move(outer_response_body), std::move(endpoints),
                     std::move(blob_context_getter)));
}

WebBundleBlobDataSource::~WebBundleBlobDataSource() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (core_)
    GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE, std::move(core_));

  auto tasks = std::move(pending_get_core_tasks_);
  for (auto& task : tasks) {
    std::move(task).Run();
  }
}

void WebBundleBlobDataSource::AddReceiver(
    mojo::PendingReceiver<web_package::mojom::BundleDataSource>
        pending_receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WaitForCore(base::BindOnce(&WebBundleBlobDataSource::AddReceiverImpl,
                             base::Unretained(this),
                             std::move(pending_receiver)));
}

void WebBundleBlobDataSource::AddReceiverImpl(
    mojo::PendingReceiver<web_package::mojom::BundleDataSource>
        pending_receiver) {
  if (!core_)
    return;
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BlobDataSourceCore::AddReceiver, weak_core_,
                                std::move(pending_receiver)));
}

// static
void WebBundleBlobDataSource::CreateCoreOnIO(
    base::WeakPtr<WebBundleBlobDataSource> weak_ptr,
    uint64_t length_hint,
    mojo::ScopedDataPipeConsumerHandle outer_response_body,
    network::mojom::URLLoaderClientEndpointsPtr endpoints,
    BrowserContext::BlobContextGetter blob_context_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto core = std::make_unique<BlobDataSourceCore>(
      length_hint, std::move(endpoints), std::move(blob_context_getter));
  core->Start(std::move(outer_response_body));
  auto weak_core = core->GetWeakPtr();
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&WebBundleBlobDataSource::SetCoreOnUI, std::move(weak_ptr),
                     std::move(weak_core), std::move(core)));
}

// static
void WebBundleBlobDataSource::SetCoreOnUI(
    base::WeakPtr<WebBundleBlobDataSource> weak_ptr,
    base::WeakPtr<BlobDataSourceCore> weak_core,
    std::unique_ptr<BlobDataSourceCore> core) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!weak_ptr) {
    // This happens when the WebBundleBlobDataSource was deleted before
    // SetCoreOnUI() is called.
    GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE, std::move(core));
    return;
  }
  weak_ptr->SetCoreOnUIImpl(std::move(weak_core), std::move(core));
}

void WebBundleBlobDataSource::ReadToDataPipe(
    uint64_t offset,
    uint64_t length,
    mojo::ScopedDataPipeProducerHandle producer_handle,
    CompletionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WaitForCore(base::BindOnce(&WebBundleBlobDataSource::ReadToDataPipeImpl,
                             base::Unretained(this), offset, length,
                             std::move(producer_handle), std::move(callback)));
}

void WebBundleBlobDataSource::WaitForCore(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (core_) {
    std::move(callback).Run();
    return;
  }
  pending_get_core_tasks_.push_back(std::move(callback));
}

void WebBundleBlobDataSource::SetCoreOnUIImpl(
    base::WeakPtr<BlobDataSourceCore> weak_core,
    std::unique_ptr<BlobDataSourceCore> core) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  weak_core_ = std::move(weak_core);
  core_ = std::move(core);

  auto tasks = std::move(pending_get_core_tasks_);
  for (auto& task : tasks) {
    std::move(task).Run();
  }
}

void WebBundleBlobDataSource::ReadToDataPipeImpl(
    uint64_t offset,
    uint64_t length,
    mojo::ScopedDataPipeProducerHandle producer_handle,
    CompletionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!core_) {
    // This happens when |this| was deleted before SetCoreOnUI() is called.
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }
  CompletionCallback wrapped_callback = base::BindOnce(
      [](CompletionCallback callback, net::Error net_error) {
        DCHECK_CURRENTLY_ON(BrowserThread::IO);
        GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), net_error));
      },
      std::move(callback));
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BlobDataSourceCore::ReadToDataPipe, weak_core_,
                                offset, length, std::move(producer_handle),
                                std::move(wrapped_callback)));
}

WebBundleBlobDataSource::BlobDataSourceCore::BlobDataSourceCore(
    uint64_t length_hint,
    network::mojom::URLLoaderClientEndpointsPtr endpoints,
    BrowserContext::BlobContextGetter blob_context_getter)
    : length_hint_(length_hint),
      endpoints_(std::move(endpoints)),
      blob_builder_from_stream_(std::make_unique<
                                storage::BlobBuilderFromStream>(
          std::move(blob_context_getter).Run(),
          "" /* content_type */,
          "" /* content_disposition */,
          base::BindOnce(
              &WebBundleBlobDataSource::BlobDataSourceCore::StreamingBlobDone,
              base::Unretained(this)))) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

WebBundleBlobDataSource::BlobDataSourceCore::~BlobDataSourceCore() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (blob_builder_from_stream_)
    std::move(blob_builder_from_stream_)->Abort();
}

void WebBundleBlobDataSource::BlobDataSourceCore::Start(
    mojo::ScopedDataPipeConsumerHandle outer_response_body) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // If |length_hint_| is zero (the stream length is unknown), this will create
  // a disk-backed blob instead of memory-backed.
  // TODO(crbug.com/1033404): Consider deferring creating a blob until the
  // stream length can be calculated from webbundle header.
  blob_builder_from_stream_->Start(
      length_hint_, std::move(outer_response_body),
      mojo::NullAssociatedRemote() /*  progress_client */);
}

void WebBundleBlobDataSource::BlobDataSourceCore::AddReceiver(
    mojo::PendingReceiver<web_package::mojom::BundleDataSource>
        pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void WebBundleBlobDataSource::BlobDataSourceCore::ReadToDataPipe(
    uint64_t offset,
    uint64_t length,
    mojo::ScopedDataPipeProducerHandle producer_handle,
    CompletionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  WaitForBlob(base::BindOnce(&WebBundleBlobDataSource::BlobDataSourceCore::
                                 OnBlobReadyForReadToDataPipe,
                             base::Unretained(this), offset, length,
                             std::move(producer_handle), std::move(callback)));
}

base::WeakPtr<WebBundleBlobDataSource::BlobDataSourceCore>
WebBundleBlobDataSource::BlobDataSourceCore::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return weak_factory_.GetWeakPtr();
}

void WebBundleBlobDataSource::BlobDataSourceCore::Read(uint64_t offset,
                                                       uint64_t length,
                                                       ReadCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  WaitForBlob(base::BindOnce(
      &WebBundleBlobDataSource::BlobDataSourceCore::OnBlobReadyForRead,
      base::Unretained(this), offset, length, std::move(callback)));
}

void WebBundleBlobDataSource::BlobDataSourceCore::Length(
    LengthCallback callback) {
  std::move(callback).Run(-1);
}

void WebBundleBlobDataSource::BlobDataSourceCore::IsRandomAccessContext(
    IsRandomAccessContextCallback callback) {
  std::move(callback).Run(false);
}

void WebBundleBlobDataSource::BlobDataSourceCore::StreamingBlobDone(
    storage::BlobBuilderFromStream* builder,
    std::unique_ptr<storage::BlobDataHandle> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (result)
    blob_ = std::move(result);
  blob_builder_from_stream_.reset();

  auto tasks = std::move(pending_get_blob_tasks_);
  for (auto& task : tasks) {
    std::move(task).Run();
  }
}

void WebBundleBlobDataSource::BlobDataSourceCore::WaitForBlob(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!blob_builder_from_stream_) {
    std::move(callback).Run();
    return;
  }
  pending_get_blob_tasks_.push_back(std::move(callback));
}

void WebBundleBlobDataSource::BlobDataSourceCore::OnBlobReadyForRead(
    uint64_t offset,
    uint64_t length,
    ReadCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!blob_) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  auto blob_reader = blob_->CreateReader();
  auto* raw_blob_reader = blob_reader.get();
  auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&OnCalculateSizeComplete, offset, length,
                     std::move(callback), std::move(blob_reader)));
  auto status = raw_blob_reader->CalculateSize(std::move(split_callback.first));
  if (status != storage::BlobReader::Status::IO_PENDING) {
    std::move(split_callback.second)
        .Run(status == storage::BlobReader::Status::NET_ERROR
                 ? raw_blob_reader->net_error()
                 : net::OK);
  }
}

void WebBundleBlobDataSource::BlobDataSourceCore::OnBlobReadyForReadToDataPipe(
    uint64_t offset,
    uint64_t length,
    mojo::ScopedDataPipeProducerHandle producer_handle,
    CompletionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!blob_) {
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }
  storage::MojoBlobReader::Create(
      blob_.get(), net::HttpByteRange::Bounded(offset, offset + length - 1),
      std::make_unique<MojoBlobReaderDelegate>(std::move(callback)),
      std::move(producer_handle));
}

}  // namespace content
