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

// Manages a service worker disk cache entry. Creates and owns an entry.
// TODO(bashi): Used by resource writers. Use in readers as well.
class DiskEntryManager {
 public:
  DiskEntryManager(int64_t resource_id,
                   base::WeakPtr<AppCacheDiskCache> disk_cache);
  ~DiskEntryManager();

  DiskEntryManager(const DiskEntryManager&) = delete;
  DiskEntryManager& operator=(const DiskEntryManager&) = delete;

  // Can be nullptr when a disk cache error occurs.
  AppCacheDiskCacheEntry* entry() {
    DCHECK_EQ(creation_phase_, CreationPhase::kDone);
    return entry_;
  }

  // Calls the callback when entry() is created and can be used.
  //
  // Overlapping calls are not allowed. Specifically, once the method is called,
  // it must not be called again until it calls the callback.
  //
  // If necessary, kicks off the creation of a disk cache entry for the
  // `resource_id` passed to the constructor. After the callback is called,
  // `entry()` can be safely called to obtain the created entry.
  //
  // Has a retry mechanism. If the first attempt fails, dooms the existing
  // entry, then tries to create an entry again.
  void EnsureEntryIsCreated(base::OnceClosure callback);

 private:
  // State of creating a disk_cache entry.
  enum class CreationPhase {
    kNoAttempt,
    kInitialAttempt,
    kDoomExisting,
    kSecondAttempt,
    kDone,
  };

  // Callbacks of EnsureEntryIsCreated(). These are static to manage the
  // ownership of AppCacheDiskCacheEntry correctly.
  // TODO(crbug.com/586174): Refactor service worker's disk cache to use
  // disk_cache::EntryResult to make these callbacks non-static.
  static void DidCreateEntryForFirstAttempt(
      base::WeakPtr<DiskEntryManager> entry_manager,
      AppCacheDiskCacheEntry** entry,
      int rv);
  static void DidDoomExistingEntry(
      base::WeakPtr<DiskEntryManager> entry_manager,
      int rv);
  static void DidCreateEntryForSecondAttempt(
      base::WeakPtr<DiskEntryManager> entry_manager,
      AppCacheDiskCacheEntry** entry,
      int rv);

  void RunEnsureEntryIsCreatedCallback();

  const int64_t resource_id_;
  base::WeakPtr<AppCacheDiskCache> disk_cache_;
  AppCacheDiskCacheEntry* entry_ = nullptr;

  CreationPhase creation_phase_ = CreationPhase::kNoAttempt;

  // Stored as a data member to handle //net-style maybe-async methods.
  base::OnceClosure ensure_entry_is_created_callback_;

  base::WeakPtrFactory<DiskEntryManager> weak_factory_{this};
};

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
  // to handle //net-style maybe-async methods.
  network::mojom::URLResponseHeadPtr response_head_;
  // Holds the callback of ReadResponseHead(). Stored as a member field to
  // handle //net-style maybe-async methods.
  ReadResponseHeadCallback read_response_head_callback_;

  // Helper for ReadData().
  std::unique_ptr<DataReader> data_reader_;

  // Holds the callback of EnsureEntryIsOpen(). Stored as a data member to
  // handle //net-style maybe-async methods.
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
class ServiceWorkerResourceWriterImpl
    : public storage::mojom::ServiceWorkerResourceWriter {
 public:
  ServiceWorkerResourceWriterImpl(int64_t resource_id,
                                  base::WeakPtr<AppCacheDiskCache> disk_cache);

  ServiceWorkerResourceWriterImpl(const ServiceWorkerResourceWriterImpl&) =
      delete;
  ServiceWorkerResourceWriterImpl& operator=(
      const ServiceWorkerResourceWriterImpl&) = delete;

  ~ServiceWorkerResourceWriterImpl() override;

  // storage::mojom::ServiceWorkerResourceWriter implementations:
  void WriteResponseHead(network::mojom::URLResponseHeadPtr response_head,
                         WriteResponseHeadCallback callback) override;
  void WriteData(mojo_base::BigBuffer data,
                 WriteDataCallback callback) override;

 private:
  // Called while executing WriteResponseHead().
  void WriteResponseHeadToEntry(
      network::mojom::URLResponseHeadPtr response_head,
      WriteResponseHeadCallback callback);
  void DidWriteResponseHead(scoped_refptr<net::IOBuffer> buffer,
                            size_t write_amount,
                            int rv);

  // Called while executing WriteData().
  void WriteDataToEntry(mojo_base::BigBuffer data, WriteDataCallback callback);
  void DidWriteData(scoped_refptr<net::IOBuffer> buffer,
                    size_t write_amount,
                    int rv);

  DiskEntryManager entry_manager_;

  // Points the current write position of WriteData().
  size_t write_position_ = 0;

  // Holds the callback of WriteResponseHead() or WriteData(). Stored as a data
  // member to handle //net-style maybe-async methods.
  net::CompletionOnceCallback write_callback_;

#if DCHECK_IS_ON()
  enum class State {
    kIdle,
    kWriteResponseHeadStarted,
    kWriteResponseHeadHasEntry,
    kWriteDataStarted,
    kWriteDataHasEntry,
  };
  State state_ = State::kIdle;
#endif  // DCHECK_IS_ON()

  base::WeakPtrFactory<ServiceWorkerResourceWriterImpl> weak_factory_{this};
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
