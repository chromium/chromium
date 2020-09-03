// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/chrome_blob_storage_context.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/supports_user_data.h"
#include "base/task/thread_pool.h"
#include "base/task_runner.h"
#include "content/browser/blob_storage/blob_registry_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_memory_controller.h"
#include "storage/browser/blob/blob_registry_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_loader_factory.h"
#include "storage/browser/blob/blob_url_registry.h"

using base::FilePath;
using base::UserDataAdapter;
using storage::BlobStorageContext;

namespace content {

namespace {
const FilePath::CharType kBlobStorageParentDirectory[] =
    FILE_PATH_LITERAL("blob_storage");

// Removes all folders in the parent directory except for the
// |current_run_dir| folder. If this path is empty, then we delete all folders.
void RemoveOldBlobStorageDirectories(FilePath blob_storage_parent,
                                     const FilePath& current_run_dir) {
  if (!base::DirectoryExists(blob_storage_parent)) {
    return;
  }
  base::FileEnumerator enumerator(blob_storage_parent, false /* recursive */,
                                  base::FileEnumerator::DIRECTORIES);
  bool success = true;
  bool cleanup_needed = false;
  for (FilePath name = enumerator.Next(); !name.empty();
       name = enumerator.Next()) {
    cleanup_needed = true;
    if (current_run_dir.empty() || name != current_run_dir)
      success &= base::DeletePathRecursively(name);
  }
  if (cleanup_needed)
    UMA_HISTOGRAM_BOOLEAN("Storage.Blob.CleanupSuccess", success);
}

class BlobHandleImpl : public BlobHandle {
 public:
  explicit BlobHandleImpl(std::unique_ptr<storage::BlobDataHandle> handle)
      : handle_(std::move(handle)) {}

  ~BlobHandleImpl() override {}

  std::string GetUUID() override { return handle_->uuid(); }

  mojo::PendingRemote<blink::mojom::Blob> PassBlob() override {
    mojo::PendingRemote<blink::mojom::Blob> result;
    storage::BlobImpl::Create(
        std::make_unique<storage::BlobDataHandle>(*handle_),
        result.InitWithNewPipeAndPassReceiver());
    return result;
  }

 private:
  std::unique_ptr<storage::BlobDataHandle> handle_;
};

}  // namespace

ChromeBlobStorageContext::ChromeBlobStorageContext() {}

// static
ChromeBlobStorageContext* ChromeBlobStorageContext::GetFor(
    BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context->GetUserData(kBlobStorageContextKeyName)) {
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context =
        new ChromeBlobStorageContext();
    context->SetUserData(
        kBlobStorageContextKeyName,
        std::make_unique<UserDataAdapter<ChromeBlobStorageContext>>(
            blob_storage_context.get()));

    // Check first to avoid memory leak in unittests.
    bool io_thread_valid =
        BrowserThread::IsThreadInitialized(BrowserThread::IO);

    // Resolve our storage directories.
    FilePath blob_storage_parent =
        context->GetPath().Append(kBlobStorageParentDirectory);
    FilePath blob_storage_dir = blob_storage_parent.Append(
        FilePath::FromUTF8Unsafe(base::GenerateGUID()));

    // Only populate the task runner if we're not off the record. This enables
    // paging/saving blob data to disk.
    scoped_refptr<base::TaskRunner> file_task_runner;

    // If we're not incognito mode, schedule all of our file tasks to enable
    // disk on the storage context.
    if (!context->IsOffTheRecord() && io_thread_valid) {
      file_task_runner = base::ThreadPool::CreateTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
      // Removes our old blob directories if they exist.
      BrowserThread::PostBestEffortTask(
          FROM_HERE, file_task_runner,
          base::BindOnce(&RemoveOldBlobStorageDirectories,
                         std::move(blob_storage_parent), blob_storage_dir));
    }

    if (io_thread_valid) {
      GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&ChromeBlobStorageContext::InitializeOnIOThread,
                         blob_storage_context, context->GetPath(),
                         std::move(blob_storage_dir),
                         std::move(file_task_runner)));
    }
  }

  return UserDataAdapter<ChromeBlobStorageContext>::Get(
      context, kBlobStorageContextKeyName);
}

// static
mojo::PendingRemote<storage::mojom::BlobStorageContext>
ChromeBlobStorageContext::GetRemoteFor(BrowserContext* browser_context) {
  DCHECK(browser_context);
  mojo::PendingRemote<storage::mojom::BlobStorageContext> remote;
  auto receiver = remote.InitWithNewPipeAndPassReceiver();
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<ChromeBlobStorageContext> blob_storage_context,
             mojo::PendingReceiver<storage::mojom::BlobStorageContext>
                 receiver) {
            blob_storage_context->BindMojoContext(std::move(receiver));
          },
          base::RetainedRef(ChromeBlobStorageContext::GetFor(browser_context)),
          std::move(receiver)));
  return remote;
}

void ChromeBlobStorageContext::InitializeOnIOThread(
    const FilePath& profile_dir,
    const FilePath& blob_storage_dir,
    scoped_refptr<base::TaskRunner> file_task_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  context_ = std::make_unique<BlobStorageContext>(profile_dir, blob_storage_dir,
                                                  std::move(file_task_runner));
  // Signal the BlobMemoryController when it's appropriate to calculate its
  // storage limits.
  content::GetIOThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     &storage::BlobMemoryController::CalculateBlobStorageLimits,
                     context_->mutable_memory_controller()->GetWeakPtr()));
}

storage::BlobStorageContext* ChromeBlobStorageContext::context() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return context_.get();
}

void ChromeBlobStorageContext::BindMojoContext(
    mojo::PendingReceiver<storage::mojom::BlobStorageContext> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(context_) << "InitializeOnIOThread must be called first";
  context_->Bind(std::move(receiver));
}

std::unique_ptr<BlobHandle> ChromeBlobStorageContext::CreateMemoryBackedBlob(
    base::span<const uint8_t> data,
    const std::string& content_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::string uuid(base::GenerateGUID());
  auto blob_data_builder = std::make_unique<storage::BlobDataBuilder>(uuid);
  blob_data_builder->set_content_type(content_type);
  blob_data_builder->AppendData(data);

  std::unique_ptr<storage::BlobDataHandle> blob_data_handle =
      context_->AddFinishedBlob(std::move(blob_data_builder));
  if (!blob_data_handle)
    return std::unique_ptr<BlobHandle>();

  std::unique_ptr<BlobHandle> blob_handle(
      new BlobHandleImpl(std::move(blob_data_handle)));
  return blob_handle;
}

// static
scoped_refptr<network::SharedURLLoaderFactory>
ChromeBlobStorageContext::URLLoaderFactoryForToken(
    StoragePartition* storage_partition,
    mojo::PendingRemote<blink::mojom::BlobURLToken> token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      blob_url_loader_factory_remote;
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<BlobRegistryWrapper> registry,
             mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
             mojo::PendingRemote<blink::mojom::BlobURLToken> token) {
            storage::BlobURLLoaderFactory::Create(
                std::move(token), registry->url_registry()->AsWeakPtr(),
                std::move(receiver));
          },
          base::WrapRefCounted(
              static_cast<StoragePartitionImpl*>(storage_partition)
                  ->GetBlobRegistry()),
          blob_url_loader_factory_remote.InitWithNewPipeAndPassReceiver(),
          std::move(token)));
  return base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
      std::move(blob_url_loader_factory_remote));
}

// static
scoped_refptr<network::SharedURLLoaderFactory>
ChromeBlobStorageContext::URLLoaderFactoryForUrl(
    StoragePartition* storage_partition,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      blob_url_loader_factory_remote;
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<BlobRegistryWrapper> registry,
             mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
             const GURL& url) {
            auto blob_remote = registry->url_registry()->GetBlobFromUrl(url);
            storage::BlobURLLoaderFactory::Create(std::move(blob_remote), url,
                                                  std::move(receiver));
          },
          base::WrapRefCounted(
              static_cast<StoragePartitionImpl*>(storage_partition)
                  ->GetBlobRegistry()),
          blob_url_loader_factory_remote.InitWithNewPipeAndPassReceiver(),
          url));
  return base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
      std::move(blob_url_loader_factory_remote));
}

// static
mojo::PendingRemote<blink::mojom::Blob> ChromeBlobStorageContext::GetBlobRemote(
    BrowserContext* browser_context,
    const std::string& uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojo::PendingRemote<blink::mojom::Blob> blob_remote;
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<ChromeBlobStorageContext> context,
             mojo::PendingReceiver<blink::mojom::Blob> receiver,
             const std::string& uuid) {
            auto handle = context->context()->GetBlobDataFromUUID(uuid);
            if (handle)
              storage::BlobImpl::Create(std::move(handle), std::move(receiver));
          },
          base::WrapRefCounted(GetFor(browser_context)),
          blob_remote.InitWithNewPipeAndPassReceiver(), uuid));
  return blob_remote;
}

ChromeBlobStorageContext::~ChromeBlobStorageContext() = default;

storage::BlobStorageContext* GetBlobStorageContext(
    ChromeBlobStorageContext* blob_storage_context) {
  if (!blob_storage_context)
    return nullptr;
  return blob_storage_context->context();
}

const char kBlobStorageContextKeyName[] = "content_blob_storage_context";

}  // namespace content
