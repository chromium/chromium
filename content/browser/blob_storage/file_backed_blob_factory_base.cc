// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/file_backed_blob_factory_base.h"

#include "base/functional/callback_helpers.h"
#include "base/process/process_handle.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_registry_impl.h"
#include "third_party/blink/public/mojom/blob/data_element.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
GetAccessCallback(const GURL& url_for_file_access_checks) {
  if (!file_access::ScopedFileAccessDelegate::HasInstance() ||
      !url_for_file_access_checks.is_valid()) {
    return base::NullCallback();
  }

  file_access::ScopedFileAccessDelegate* file_access =
      file_access::ScopedFileAccessDelegate::Get();
  CHECK(file_access);

  return file_access->CreateFileAccessCallback(url_for_file_access_checks);
}

void ContinueRegisterBlob(
    mojo::PendingReceiver<blink::mojom::Blob> blob,
    std::string uuid,
    std::string content_type,
    blink::mojom::DataElementFilePtr file,
    file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
        file_access,
    bool security_check_success,
    mojo::ReportBadMessageCallback bad_message_callback,
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context,
    blink::mojom::FileBackedBlobFactory::RegisterBlobSyncCallback
        finish_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::ScopedClosureRunner scoped_finish_callback(std::move(finish_callback));

  if (blob_storage_context->context()->registry().HasEntry(uuid) ||
      uuid.empty()) {
    std::move(bad_message_callback)
        .Run("Invalid UUID passed to FileBackedBlobFactoryImpl::RegisterBlob");
    return;
  }

  if (!security_check_success) {
    std::unique_ptr<storage::BlobDataHandle> handle =
        blob_storage_context->context()->AddBrokenBlob(
            uuid, content_type, /*content_disposition=*/"",
            storage::BlobStatus::ERR_REFERENCED_FILE_UNAVAILABLE);
    storage::BlobImpl::Create(std::move(handle), std::move(blob));
    return;
  }

  auto builder = std::make_unique<storage::BlobDataBuilder>(uuid);
  if (file->length > 0) {
    builder->AppendFile(file->path, file->offset, file->length,
                        file->expected_modification_time.value_or(base::Time()),
                        file_access);
  }
  builder->set_content_type(content_type);

  std::unique_ptr<storage::BlobDataHandle> handle =
      blob_storage_context->context()->AddFinishedBlob(std::move(builder));

  CHECK(!handle->IsBroken());

  storage::BlobImpl::Create(std::move(handle), std::move(blob));
}

}  // namespace

FileBackedBlobFactoryBase::FileBackedBlobFactoryBase(int process_id)
    : process_id_(process_id) {}

FileBackedBlobFactoryBase::~FileBackedBlobFactoryBase() = default;

void FileBackedBlobFactoryBase::RegisterBlob(
    mojo::PendingReceiver<blink::mojom::Blob> blob,
    const std::string& uuid,
    const std::string& content_type,
    blink::mojom::DataElementFilePtr file) {
  // We can safely perform the registration asynchronously since blob remote
  // messages are managed by the mojo infrastructure until the blob pending
  // receiver is resolved, and this happens when the async registration is
  // completed. Without the mojo continuation callback the `RegisterBlobSync`
  // call is async.

  RegisterBlobSync(std::move(blob), uuid, content_type, std::move(file),
                   base::NullCallback());
}

void FileBackedBlobFactoryBase::RegisterBlobSync(
    mojo::PendingReceiver<blink::mojom::Blob> blob,
    const std::string& uuid,
    const std::string& content_type,
    blink::mojom::DataElementFilePtr file,
    RegisterBlobSyncCallback finish_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  bool security_check_success =
      ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(process_id_,
                                                                 file->path);

  GURL url_for_file_access_checks = GetCurrentUrl();

  if (finish_callback) {
    finish_callback =
        base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                           std::move(finish_callback));
  }

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ContinueRegisterBlob, std::move(blob), uuid, content_type,
                     std::move(file),
                     GetAccessCallback(url_for_file_access_checks),
                     security_check_success,
                     base::BindPostTask(GetUIThreadTaskRunner({}),
                                        GetBadMessageCallback()),
                     blob_storage_context_, std::move(finish_callback)));
}

}  // namespace content
