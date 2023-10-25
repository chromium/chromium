// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLOB_STORAGE_FILE_BACKED_BLOB_FACTORY_FRAME_IMPL_H_
#define CONTENT_BROWSER_BLOB_STORAGE_FILE_BACKED_BLOB_FACTORY_FRAME_IMPL_H_

#include "content/browser/blob_storage/file_backed_blob_factory_base.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/mojom/blob/file_backed_blob_factory.mojom.h"
#include "url/gurl.h"

namespace content {

class CONTENT_EXPORT FileBackedBlobFactoryFrameImpl
    : public DocumentUserData<FileBackedBlobFactoryFrameImpl>,
      public FileBackedBlobFactoryBase {
 public:
  ~FileBackedBlobFactoryFrameImpl() override;
  FileBackedBlobFactoryFrameImpl(const FileBackedBlobFactoryFrameImpl&) =
      delete;
  FileBackedBlobFactoryFrameImpl& operator=(
      const FileBackedBlobFactoryFrameImpl&) = delete;

 private:
  friend class DocumentUserData<FileBackedBlobFactoryFrameImpl>;

  FileBackedBlobFactoryFrameImpl(
      RenderFrameHost* rfh,
      mojo::PendingAssociatedReceiver<blink::mojom::FileBackedBlobFactory>
          receiver);

  // FileBackedBlobFactoryBase:
  GURL GetCurrentUrl() override;
  mojo::ReportBadMessageCallback GetBadMessageCallback() override;

  mojo::AssociatedReceiver<blink::mojom::FileBackedBlobFactory> receiver_;
  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLOB_STORAGE_FILE_BACKED_BLOB_FACTORY_FRAME_IMPL_H_
