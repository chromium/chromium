// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/file_backed_blob_factory_impl.h"
#include "base/functional/callback_helpers.h"
#include "base/task/bind_post_task.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_registry_impl.h"
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
    GURL url_for_file_access_checks,
    bool security_check_success,
    mojo::ReportBadMessageCallback bad_message_callback,
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

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
                        GetAccessCallback(url_for_file_access_checks));
  }
  builder->set_content_type(content_type);

  std::unique_ptr<storage::BlobDataHandle> handle =
      blob_storage_context->context()->AddFinishedBlob(std::move(builder));

  CHECK(!handle->IsBroken());

  storage::BlobImpl::Create(std::move(handle), std::move(blob));
}

}  // namespace

FileBackedBlobFactoryImpl::~FileBackedBlobFactoryImpl() = default;

void FileBackedBlobFactoryImpl::RegisterBlob(
    mojo::PendingReceiver<blink::mojom::Blob> blob,
    const std::string& uuid,
    const std::string& content_type,
    blink::mojom::DataElementFilePtr file) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We can safely perform the registration asynchronously since blob remote
  // messages are managed by the mojo infrastructure until the blob pending
  // receiver is resolved, and this happens when the async registration is
  // completed.

  // TODO(b/289958501): will this interface ever need to support filesystem
  // files? In that case, how can we distinguish between file types in order to
  // perform the correct ChildProcessSecurityPolicyImpl check?
  bool security_check_success =
      ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(process_id_,
                                                                 file->path);

  // TODO(b/276857839): handling of fenced frames is still in discussion. For
  // now we use an invalid GURL as destination URL. This will allow access to
  // unrestricted files but block access to restricted ones.
  GURL url_for_file_access_checks =
      render_frame_host().IsNestedWithinFencedFrame()
          ? GURL()
          : render_frame_host().GetOutermostMainFrame()->GetLastCommittedURL();

  // Run most of the registration process asynchronously.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ContinueRegisterBlob, std::move(blob), uuid, content_type,
                     std::move(file), std::move(url_for_file_access_checks),
                     security_check_success,
                     base::BindPostTask(content::GetUIThreadTaskRunner({}),
                                        receiver_.GetBadMessageCallback()),
                     blob_storage_context_));
}

FileBackedBlobFactoryImpl::FileBackedBlobFactoryImpl(
    RenderFrameHost* rfh,
    mojo::PendingAssociatedReceiver<blink::mojom::FileBackedBlobFactory>
        receiver)
    : DocumentUserData<FileBackedBlobFactoryImpl>(rfh),
      receiver_(this, std::move(receiver)),
      process_id_(render_frame_host().GetProcess()->GetID()) {
  blob_storage_context_ = base::WrapRefCounted(ChromeBlobStorageContext::GetFor(
      render_frame_host().GetBrowserContext()));
  CHECK(blob_storage_context_);
}

DOCUMENT_USER_DATA_KEY_IMPL(FileBackedBlobFactoryImpl);

}  // namespace content
