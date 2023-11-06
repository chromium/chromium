// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/file_backed_blob_factory_frame_impl.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "url/gurl.h"

namespace content {

FileBackedBlobFactoryFrameImpl::~FileBackedBlobFactoryFrameImpl() = default;

FileBackedBlobFactoryFrameImpl::FileBackedBlobFactoryFrameImpl(
    RenderFrameHost* rfh,
    mojo::PendingAssociatedReceiver<blink::mojom::FileBackedBlobFactory>
        receiver)
    : DocumentUserData<FileBackedBlobFactoryFrameImpl>(rfh),
      content::FileBackedBlobFactoryBase(
          render_frame_host().GetProcess()->GetID()),
      receiver_(this, std::move(receiver)) {
  blob_storage_context_ = base::WrapRefCounted(ChromeBlobStorageContext::GetFor(
      render_frame_host().GetBrowserContext()));
  CHECK(blob_storage_context_);
}

GURL FileBackedBlobFactoryFrameImpl::GetCurrentUrl() {
  // TODO(b/276857839): handling of fenced frames is still in discussion. For
  // now we use an invalid GURL as destination URL. This will allow access to
  // unrestricted files but block access to restricted ones.
  if (render_frame_host().IsNestedWithinFencedFrame()) {
    return GURL();
  }
  return render_frame_host().GetOutermostMainFrame()->GetLastCommittedURL();
}

mojo::ReportBadMessageCallback
FileBackedBlobFactoryFrameImpl::GetBadMessageCallback() {
  return receiver_.GetBadMessageCallback();
}

DOCUMENT_USER_DATA_KEY_IMPL(FileBackedBlobFactoryFrameImpl);

}  // namespace content
