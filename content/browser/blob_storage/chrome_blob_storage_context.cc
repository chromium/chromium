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
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "content/browser/resource_context_impl.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_memory_controller.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_loader_factory.h"

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
      success &= base::DeleteFile(name, true /* recursive */);
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

  blink::mojom::BlobPtr PassBlob() override {
    blink::mojom::BlobPtr result;
    storage::BlobImpl::Create(
        std::make_unique<storage::BlobDataHandle>(*handle_),
        MakeRequest(&result));
    return result;
  }

 private:
  std::unique_ptr<storage::BlobDataHandle> handle_;
};

}  // namespace

ChromeBlobStorageContext::ChromeBlobStorageContext() {}

ChromeBlobStorageContext* ChromeBlobStorageContext::GetFor(
    BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context->GetUserData(kBlobStorageContextKeyName)) {
    scoped_refptr<ChromeBlobStorageContext> blob =
        new ChromeBlobStorageContext();
    context->SetUserData(
        kBlobStorageContextKeyName,
        std::make_unique<UserDataAdapter<ChromeBlobStorageContext>>(
            blob.get()));

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
      file_task_runner = base::CreateTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
      // Removes our old blob directories if they exist.
      BrowserThread::PostAfterStartupTask(
          FROM_HERE, file_task_runner,
          base::BindOnce(&RemoveOldBlobStorageDirectories,
                         std::move(blob_storage_parent), blob_storage_dir));
    }

    if (io_thread_valid) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(&ChromeBlobStorageContext::InitializeOnIOThread, blob,
                         std::move(blob_storage_dir),
                         std::move(file_task_runner)));
    }
  }

  return UserDataAdapter<ChromeBlobStorageContext>::Get(
      context, kBlobStorageContextKeyName);
}

void ChromeBlobStorageContext::InitializeOnIOThread(
    FilePath blob_storage_dir,
    scoped_refptr<base::TaskRunner> file_task_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  context_.reset(new BlobStorageContext(std::move(blob_storage_dir),
                                        std::move(file_task_runner)));
  // Signal the BlobMemoryController when it's appropriate to calculate its
  // storage limits.
  BrowserThread::PostAfterStartupTask(
      FROM_HERE,
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}),
      base::BindOnce(&storage::BlobMemoryController::CalculateBlobStorageLimits,
                     context_->mutable_memory_controller()->GetWeakPtr()));
}

std::unique_ptr<BlobHandle> ChromeBlobStorageContext::CreateMemoryBackedBlob(
    const char* data,
    size_t length,
    const std::string& content_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::string uuid(base::GenerateGUID());
  auto blob_data_builder = std::make_unique<storage::BlobDataBuilder>(uuid);
  blob_data_builder->set_content_type(content_type);
  blob_data_builder->AppendData(data, length);

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
    BrowserContext* browser_context,
    blink::mojom::BlobURLTokenPtr token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  network::mojom::URLLoaderFactoryPtr blob_url_loader_factory_ptr;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          [](scoped_refptr<ChromeBlobStorageContext> context,
             network::mojom::URLLoaderFactoryRequest request,
             blink::mojom::BlobURLTokenPtrInfo token) {
            storage::BlobURLLoaderFactory::Create(
                blink::mojom::BlobURLTokenPtr(std::move(token)),
                context->context()->AsWeakPtr(), std::move(request));
          },
          base::WrapRefCounted(GetFor(browser_context)),
          MakeRequest(&blob_url_loader_factory_ptr), token.PassInterface()));
  return base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
      std::move(blob_url_loader_factory_ptr));
}

// static
scoped_refptr<network::SharedURLLoaderFactory>
ChromeBlobStorageContext::URLLoaderFactoryForUrl(
    BrowserContext* browser_context,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  network::mojom::URLLoaderFactoryPtr blob_url_loader_factory_ptr;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          [](scoped_refptr<ChromeBlobStorageContext> context,
             network::mojom::URLLoaderFactoryRequest request, const GURL& url) {
            auto blob_handle =
                context->context()->GetBlobDataFromPublicURL(url);
            storage::BlobURLLoaderFactory::Create(std::move(blob_handle), url,
                                                  std::move(request));
          },
          base::WrapRefCounted(GetFor(browser_context)),
          MakeRequest(&blob_url_loader_factory_ptr), url));
  return base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
      std::move(blob_url_loader_factory_ptr));
}

// static
blink::mojom::BlobPtr ChromeBlobStorageContext::GetBlobPtr(
    BrowserContext* browser_context,
    const std::string& uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  blink::mojom::BlobPtr blob_ptr;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          [](scoped_refptr<ChromeBlobStorageContext> context,
             blink::mojom::BlobRequest request, const std::string& uuid) {
            auto handle = context->context()->GetBlobDataFromUUID(uuid);
            if (handle)
              storage::BlobImpl::Create(std::move(handle), std::move(request));
          },
          base::WrapRefCounted(GetFor(browser_context)), MakeRequest(&blob_ptr),
          uuid));
  return blob_ptr;
}

ChromeBlobStorageContext::~ChromeBlobStorageContext() {}

void ChromeBlobStorageContext::DeleteOnCorrectThread() const {
  if (BrowserThread::IsThreadInitialized(BrowserThread::IO) &&
      !BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, this);
    return;
  }
  delete this;
}

storage::BlobStorageContext* GetBlobStorageContext(
    ChromeBlobStorageContext* blob_storage_context) {
  if (!blob_storage_context)
    return nullptr;
  return blob_storage_context->context();
}

bool GetBodyBlobDataHandles(network::ResourceRequestBody* body,
                            ResourceContext* resource_context,
                            BlobHandles* blob_handles) {
  blob_handles->clear();

  storage::BlobStorageContext* blob_context = GetBlobStorageContext(
      GetChromeBlobStorageContextForResourceContext(resource_context));

  DCHECK(blob_context);
  for (size_t i = 0; i < body->elements()->size(); ++i) {
    const network::DataElement& element = (*body->elements())[i];
    if (element.type() != network::DataElement::TYPE_BLOB)
      continue;
    std::unique_ptr<storage::BlobDataHandle> handle =
        blob_context->GetBlobDataFromUUID(element.blob_uuid());
    if (!handle)
      return false;
    blob_handles->push_back(std::move(handle));
  }
  return true;
}

const char kBlobStorageContextKeyName[] = "content_blob_storage_context";

}  // namespace content
