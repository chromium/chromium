// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/file_backed_blob_factory_base.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/uuid.h"
#include "components/file_access/scoped_file_access.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/security/cpsp/child_process_security_policy_impl.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_registry_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/mojom/blob/data_element.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
GetAccessCallback(const GURL& url_for_file_access_checks) {
  if (!file_access::ScopedFileAccessDelegate::HasInstance()) {
    return base::NullCallback();
  }

  // When a delegate is installed, per-destination access checks are required.
  // If the destination URL cannot be determined or is invalid, explicitly
  // deny access rather than returning a null callback which would fall back
  // to default access handling.
  if (!url_for_file_access_checks.is_valid()) {
    return base::BindRepeating(
        [](const std::vector<base::FilePath>&,
           base::OnceCallback<void(file_access::ScopedFileAccess)> callback) {
          std::move(callback).Run(file_access::ScopedFileAccess::Denied());
        });
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
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M159);

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

FileBackedBlobFactoryBase::FileBackedBlobFactoryBase(ChildProcessId process_id)
    : process_id_(process_id) {}

FileBackedBlobFactoryBase::~FileBackedBlobFactoryBase() = default;

void FileBackedBlobFactoryBase::RegisterBlob(
    mojo::PendingReceiver<blink::mojom::Blob> blob,
    const std::string& content_type,
    blink::mojom::DataElementFilePtr file,
    bool block_on_registration,
    RegisterBlobCallback finish_callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);

  const bool security_check_success =
      ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(process_id_,
                                                                 file->path);

  const GURL url_for_file_access_checks = GetCurrentUrl();
  const std::string uuid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  base::OnceClosure done = base::BindOnce(std::move(finish_callback), uuid);

  base::OnceClosure do_register = base::BindOnce(
      &ContinueRegisterBlob, std::move(blob), uuid, content_type,
      std::move(file), GetAccessCallback(url_for_file_access_checks),
      security_check_success, blob_storage_context_);
  if (block_on_registration) {
    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE, std::move(do_register), std::move(done));
  } else {
    content::GetIOThreadTaskRunner({})->PostTask(FROM_HERE,
                                                 std::move(do_register));
    std::move(done).Run();
  }
}

}  // namespace content
