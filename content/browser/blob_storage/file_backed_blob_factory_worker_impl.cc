// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/file_backed_blob_factory_worker_impl.h"

#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "url/gurl.h"

namespace content {

FileBackedBlobFactoryWorkerImpl::~FileBackedBlobFactoryWorkerImpl() = default;

FileBackedBlobFactoryWorkerImpl::FileBackedBlobFactoryWorkerImpl(
    BrowserContext* browser_context,
    ChildProcessId process_id)
    : FileBackedBlobFactoryBase(process_id) {
  blob_storage_context_ =
      base::WrapRefCounted(ChromeBlobStorageContext::GetFor(browser_context));
}

void FileBackedBlobFactoryWorkerImpl::BindReceiver(
    mojo::PendingReceiver<blink::mojom::FileBackedBlobFactory> receiver,
    const GURL& url) {
  receiver_.Add(this, std::move(receiver), {url});
}

GURL FileBackedBlobFactoryWorkerImpl::GetCurrentUrl() {
  return receiver_.current_context().url;
}

mojo::ReportBadMessageCallback
FileBackedBlobFactoryWorkerImpl::GetBadMessageCallback() {
  return receiver_.GetBadMessageCallback();
}

}  // namespace content
