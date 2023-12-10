// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_impl.h"

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/io_buffer.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/blob_url_loader.h"
#include "storage/browser/blob/mojo_blob_reader.h"

namespace storage {
namespace {

class ReaderDelegate : public MojoBlobReader::Delegate {
 public:
  ReaderDelegate(mojo::PendingRemote<blink::mojom::BlobReaderClient> client)
      : client_(std::move(client)) {}

  ReaderDelegate(const ReaderDelegate&) = delete;
  ReaderDelegate& operator=(const ReaderDelegate&) = delete;

  MojoBlobReader::Delegate::RequestSideData DidCalculateSize(
      uint64_t total_size,
      uint64_t content_size) override {
    if (client_)
      client_->OnCalculatedSize(total_size, content_size);
    return MojoBlobReader::Delegate::DONT_REQUEST_SIDE_DATA;
  }

  void OnComplete(net::Error result, uint64_t total_written_bytes) override {
    if (client_)
      client_->OnComplete(result, total_written_bytes);
  }

 private:
  mojo::Remote<blink::mojom::BlobReaderClient> client_;
};

class DataPipeGetterReaderDelegate : public MojoBlobReader::Delegate {
 public:
  DataPipeGetterReaderDelegate(
      network::mojom::DataPipeGetter::ReadCallback callback)
      : callback_(std::move(callback)) {}

  DataPipeGetterReaderDelegate(const DataPipeGetterReaderDelegate&) = delete;
  DataPipeGetterReaderDelegate& operator=(const DataPipeGetterReaderDelegate&) =
      delete;

  MojoBlobReader::Delegate::RequestSideData DidCalculateSize(
      uint64_t total_size,
      uint64_t content_size) override {
    // Check if null since it's conceivable OnComplete() was already called
    // with error.
    if (!callback_.is_null())
      std::move(callback_).Run(net::OK, content_size);
    return MojoBlobReader::Delegate::DONT_REQUEST_SIDE_DATA;
  }

  void OnComplete(net::Error result, uint64_t total_written_bytes) override {
    // Check if null since DidCalculateSize() may have already been called
    // and an error occurred later.
    if (!callback_.is_null() && result != net::OK) {
      // On error, signal failure immediately. On success, OnCalculatedSize()
      // is guaranteed to be called, and the result will be signaled from
      // there.
      std::move(callback_).Run(result, 0);
    }
  }

 private:
  network::mojom::DataPipeGetter::ReadCallback callback_;
};

}  // namespace

// static
base::WeakPtr<BlobImpl> BlobImpl::Create(
    std::unique_ptr<BlobDataHandle> handle,
    mojo::PendingReceiver<blink::mojom::Blob> receiver) {
  return (new BlobImpl(std::move(handle), std::move(receiver)))
      ->weak_ptr_factory_.GetWeakPtr();
}

void BlobImpl::UpdateHandle(std::unique_ptr<BlobDataHandle> new_handle) {
  DCHECK_EQ(handle_->uuid(), new_handle->uuid());
  handle_ = std::move(new_handle);
}

void BlobImpl::Clone(mojo::PendingReceiver<blink::mojom::Blob> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void BlobImpl::AsDataPipeGetter(
    mojo::PendingReceiver<network::mojom::DataPipeGetter> receiver) {
  data_pipe_getter_receivers_.Add(this, std::move(receiver));
}

void BlobImpl::ReadRange(
    uint64_t offset,
    uint64_t length,
    mojo::ScopedDataPipeProducerHandle handle,
    mojo::PendingRemote<blink::mojom::BlobReaderClient> client) {
  MojoBlobReader::Create(
      handle_.get(),
      (length == std::numeric_limits<uint64_t>::max())
          ? net::HttpByteRange::RightUnbounded(offset)
          : net::HttpByteRange::Bounded(offset, offset + length - 1),
      std::make_unique<ReaderDelegate>(std::move(client)), std::move(handle));
}

void BlobImpl::ReadAll(
    mojo::ScopedDataPipeProducerHandle handle,
    mojo::PendingRemote<blink::mojom::BlobReaderClient> client) {
  MojoBlobReader::Create(handle_.get(), net::HttpByteRange(),
                         std::make_unique<ReaderDelegate>(std::move(client)),
                         std::move(handle));
}

void BlobImpl::Load(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  BlobURLLoader::CreateAndStart(std::move(loader), method, headers,
                                std::move(client),
                                std::make_unique<BlobDataHandle>(*handle_));
}

void BlobImpl::ReadSideData(ReadSideDataCallback callback) {
  handle_->RunOnConstructionComplete(base::BindOnce(
      [](BlobDataHandle handle, ReadSideDataCallback callback,
         BlobStatus status) {
        if (status != BlobStatus::DONE) {
          DCHECK(BlobStatusIsError(status));
          std::move(callback).Run(std::nullopt);
          return;
        }

        auto snapshot = handle.CreateSnapshot();
        // Currently side data is supported only for blobs with a single entry.
        const auto& items = snapshot->items();
        if (items.size() != 1) {
          std::move(callback).Run(std::nullopt);
          return;
        }

        const auto& item = items[0];
        if (item->type() != BlobDataItem::Type::kReadableDataHandle) {
          std::move(callback).Run(std::nullopt);
          return;
        }

        int32_t body_size = item->data_handle()->GetSideDataSize();
        if (body_size == 0) {
          std::move(callback).Run(std::nullopt);
          return;
        }
        item->data_handle()->ReadSideData(base::BindOnce(
            [](ReadSideDataCallback callback, int result,
               mojo_base::BigBuffer buffer) {
              if (result < 0) {
                std::move(callback).Run(std::nullopt);
                return;
              }
              std::move(callback).Run(std::move(buffer));
            },
            std::move(callback)));
      },
      *handle_, std::move(callback)));
}

void BlobImpl::CaptureSnapshot(CaptureSnapshotCallback callback) {
  handle_->RunOnConstructionComplete(base::BindOnce(
      [](base::WeakPtr<BlobImpl> blob_impl, CaptureSnapshotCallback callback,
         BlobStatus status) {
        if (!blob_impl) {
          // No need to call callback, since blob_impl is only destroyed if the
          // mojo pipe is disconnected.
          return;
        }

        auto* handle = blob_impl->handle_.get();

        if (status != BlobStatus::DONE) {
          DCHECK(BlobStatusIsError(status));
          std::move(callback).Run(0, std::nullopt);
          return;
        }

        auto snapshot = handle->CreateSnapshot();
        // Only blobs consisting of a single file can have a modification
        // time.
        const auto& items = snapshot->items();
        if (items.size() != 1) {
          std::move(callback).Run(handle->size(), std::nullopt);
          return;
        }

        const auto& item = items[0];
        if (item->type() != BlobDataItem::Type::kFile) {
          std::move(callback).Run(handle->size(), std::nullopt);
          return;
        }

        base::Time modification_time = item->expected_modification_time();
        if (!modification_time.is_null() &&
            handle->size() != BlobDataHandle::kUnknownSize) {
          std::move(callback).Run(handle->size(), modification_time);
          return;
        }

        struct SizeAndTime {
          uint64_t size;
          std::optional<base::Time> time;
        };
        base::ThreadPool::PostTaskAndReplyWithResult(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
            base::BindOnce(
                [](const base::FilePath& path) {
                  base::File::Info info;
                  if (!base::GetFileInfo(path, &info))
                    return SizeAndTime{0, std::nullopt};
                  return SizeAndTime{static_cast<uint64_t>(info.size),
                                     info.last_modified};
                },
                item->path()),
            base::BindOnce(
                [](CaptureSnapshotCallback callback,
                   const SizeAndTime& result) {
                  std::move(callback).Run(result.size, result.time);
                },
                std::move(callback)));
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BlobImpl::GetInternalUUID(GetInternalUUIDCallback callback) {
  std::move(callback).Run(handle_->uuid());
}

void BlobImpl::Clone(
    mojo::PendingReceiver<network::mojom::DataPipeGetter> receiver) {
  data_pipe_getter_receivers_.Add(this, std::move(receiver));
}

void BlobImpl::Read(mojo::ScopedDataPipeProducerHandle handle,
                    ReadCallback callback) {
  MojoBlobReader::Create(
      handle_.get(), net::HttpByteRange(),
      std::make_unique<DataPipeGetterReaderDelegate>(std::move(callback)),
      std::move(handle));
}

void BlobImpl::FlushForTesting() {
  auto weak_self = weak_ptr_factory_.GetWeakPtr();
  receivers_.FlushForTesting();
  if (!weak_self)
    return;
  data_pipe_getter_receivers_.FlushForTesting();
  if (!weak_self)
    return;
  if (receivers_.empty() && data_pipe_getter_receivers_.empty())
    delete this;
}

BlobImpl::BlobImpl(std::unique_ptr<BlobDataHandle> handle,
                   mojo::PendingReceiver<blink::mojom::Blob> receiver)
    : handle_(std::move(handle)) {
  DCHECK(handle_);
  receivers_.Add(this, std::move(receiver));
  receivers_.set_disconnect_handler(
      base::BindRepeating(&BlobImpl::OnMojoDisconnect, base::Unretained(this)));
  data_pipe_getter_receivers_.set_disconnect_handler(
      base::BindRepeating(&BlobImpl::OnMojoDisconnect, base::Unretained(this)));
}

BlobImpl::~BlobImpl() = default;

void BlobImpl::OnMojoDisconnect() {
  if (!receivers_.empty())
    return;
  if (!data_pipe_getter_receivers_.empty())
    return;
  delete this;
}

}  // namespace storage
