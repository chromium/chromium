// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLOB_STORAGE_FILE_BACKED_BLOB_FACTORY_IMPL_H_
#define CONTENT_BROWSER_BLOB_STORAGE_FILE_BACKED_BLOB_FACTORY_IMPL_H_

#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/mojom/blob/data_element.mojom.h"
#include "third_party/blink/public/mojom/blob/file_backed_blob_factory.mojom.h"
#include "url/gurl.h"

namespace content {

// `FileBackedBlobFactoryImpl` allows the registration of file backed blobs.
// During the registration the last committed URL of the outermost frame, i.e.,
// the URL the user sees, is captured. This is a navigation-associated
// interface, so messages sent after a navigation are guaranteed to arrive in
// the browser process after the navigation-related messages.
//
// A FileBackedBlobFactoryImpl object is created once per document and bound to
// a RenderFrameHost by a
// `FileBackedBlobFactoryImpl::CreateForCurrentDocument()` call made from
// `RenderFrameHostImpl::BindAccessUrlRegistry`.
//
// The lifetime of FileBackedBlobFactoryImpl is the same as that of document in
// the browser process.
//
// This class lives in the UI thread and all methods are meant to be called from
// the UI thread.
class CONTENT_EXPORT FileBackedBlobFactoryImpl
    : public blink::mojom::FileBackedBlobFactory,
      public DocumentUserData<FileBackedBlobFactoryImpl> {
 public:
  ~FileBackedBlobFactoryImpl() override;
  FileBackedBlobFactoryImpl(const FileBackedBlobFactoryImpl&) = delete;
  FileBackedBlobFactoryImpl& operator=(const FileBackedBlobFactoryImpl&) =
      delete;

  void RegisterBlob(mojo::PendingReceiver<blink::mojom::Blob> blob,
                    const std::string& uuid,
                    const std::string& content_type,
                    blink::mojom::DataElementFilePtr file) override;

 private:
  friend class DocumentUserData<FileBackedBlobFactoryImpl>;

  FileBackedBlobFactoryImpl(
      RenderFrameHost* rfh,
      mojo::PendingAssociatedReceiver<blink::mojom::FileBackedBlobFactory>
          receiver);

  mojo::AssociatedReceiver<blink::mojom::FileBackedBlobFactory> receiver_;
  const int process_id_;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
  base::WeakPtrFactory<FileBackedBlobFactoryImpl> weak_factory_{this};
  DOCUMENT_USER_DATA_KEY_DECL();
};
}  // namespace content
#endif  // CONTENT_BROWSER_BLOB_STORAGE_FILE_BACKED_BLOB_FACTORY_IMPL_H_
