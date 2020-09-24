// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_RESOURCE_OPS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_RESOURCE_OPS_H_

#include "base/memory/weak_ptr.h"
#include "components/services/storage/public/mojom/service_worker_storage_control.mojom.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"

namespace content {

class BigIOBuffer;

// The implementation of storage::mojom::ServiceWorkerResourceReader.
class ServiceWorkerResourceReaderImpl
    : public storage::mojom::ServiceWorkerResourceReader {
 public:
  ServiceWorkerResourceReaderImpl(int64_t resource_id,
                                  base::WeakPtr<AppCacheDiskCache> disk_cache);

  ServiceWorkerResourceReaderImpl(const ServiceWorkerResourceReaderImpl&) =
      delete;
  ServiceWorkerResourceReaderImpl& operator=(
      const ServiceWorkerResourceReaderImpl&) = delete;

  ~ServiceWorkerResourceReaderImpl() override;

  // storage::mojom::ServiceWorkerResourceReader implementations:
  void ReadResponseHead(ReadResponseHeadCallback callback) override;
  void ReadData(
      int64_t size,
      mojo::PendingRemote<storage::mojom::ServiceWorkerDataPipeStateNotifier>
          notifier,
      ReadDataCallback callback) override;

 private:
  class DataReader;

  // Called while executing ReadResponseHead() in the order they are declared.
  void ContinueReadResponseHead();
  void DidReadHttpResponseInfo(scoped_refptr<net::IOBuffer> buffer, int status);
  void DidReadMetadata(int status);
  // Complete the operation started by ReadResponseHead().
  void FailReadResponseHead(int status);
  void CompleteReadResponseHead(int status);

  // Completes ReadData(). Called when `data_reader_` finished reading response
  // data.
  void DidReadDataComplete();

  // Opens a disk cache entry associated with `resource_id_`, if it isn't
  // opened yet.
  void EnsureEntryIsOpen(base::OnceClosure callback);

  static void DidOpenEntry(
      base::WeakPtr<ServiceWorkerResourceReaderImpl> reader,
      AppCacheDiskCacheEntry** entry,
      int rv);

  const int64_t resource_id_;
  base::WeakPtr<AppCacheDiskCache> disk_cache_;
  AppCacheDiskCacheEntry* entry_ = nullptr;

  // Used to read metadata from disk cache.
  scoped_refptr<BigIOBuffer> metadata_buffer_;
  // Holds the return value of ReadResponseHead(). Stored as a member field
  // to handle net style maybe-async methods.
  network::mojom::URLResponseHeadPtr response_head_;
  // Holds the callback of ReadResponseHead(). Stored as a member field to
  // handle //net-style maybe-async methods.
  ReadResponseHeadCallback read_response_head_callback_;

  // Helper for ReadData().
  std::unique_ptr<DataReader> data_reader_;

  // Holds the callback of EnsureEntryIsOpen(). Stored as a data member to
  // handle net style maybe-async methods.
  base::OnceClosure open_entry_callback_;

#if DCHECK_IS_ON()
  enum class State {
    kIdle,
    kReadResponseHeadStarted,
    kReadDataStarted,
    kCacheEntryOpened,
    kResponseInfoRead,
    kMetadataRead,
  };
  State state_ = State::kIdle;
#endif  // DCHECK_IS_ON()

  base::WeakPtrFactory<ServiceWorkerResourceReaderImpl> weak_factory_{this};
};

// The implementation of storage::mojom::ServiceWorkerResourceWriter.
// Currently this class is an adaptor that uses ServiceWorkerResponseWriter
// internally.
// TODO(crbug.com/1055677): Fork the implementation of
// ServiceWorkerResponseWriter and stop using it.
class ServiceWorkerResourceWriterImpl
    : public storage::mojom::ServiceWorkerResourceWriter {
 public:
  explicit ServiceWorkerResourceWriterImpl(
      std::unique_ptr<ServiceWorkerResponseWriter> writer);

  ServiceWorkerResourceWriterImpl(const ServiceWorkerResourceWriterImpl&) =
      delete;
  ServiceWorkerResourceWriterImpl& operator=(
      const ServiceWorkerResourceWriterImpl&) = delete;

  ~ServiceWorkerResourceWriterImpl() override;

 private:
  // storage::mojom::ServiceWorkerResourceWriter implementations:
  void WriteResponseHead(network::mojom::URLResponseHeadPtr response_head,
                         WriteResponseHeadCallback callback) override;
  void WriteData(mojo_base::BigBuffer data,
                 WriteDataCallback callback) override;

  const std::unique_ptr<ServiceWorkerResponseWriter> writer_;
};

// The implementation of storage::mojom::ServiceWorkerResourceMetadataWriter.
// Currently this class is an adaptor that uses
// ServiceWorkerResponseMetadataWriter internally.
// TODO(crbug.com/1055677): Fork the implementation of
// ServiceWorkerResponseMetadataWriter and stop using it.
class ServiceWorkerResourceMetadataWriterImpl
    : public storage::mojom::ServiceWorkerResourceMetadataWriter {
 public:
  explicit ServiceWorkerResourceMetadataWriterImpl(
      std::unique_ptr<ServiceWorkerResponseMetadataWriter> writer);

  ServiceWorkerResourceMetadataWriterImpl(
      const ServiceWorkerResourceMetadataWriterImpl&) = delete;
  ServiceWorkerResourceMetadataWriterImpl& operator=(
      const ServiceWorkerResourceMetadataWriterImpl&) = delete;

  ~ServiceWorkerResourceMetadataWriterImpl() override;

 private:
  // storage::mojom::ServiceWorkerResourceMetadataWriter implementations:
  void WriteMetadata(mojo_base::BigBuffer data,
                     WriteMetadataCallback callback) override;

  const std::unique_ptr<ServiceWorkerResponseMetadataWriter> writer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_RESOURCE_OPS_H_
