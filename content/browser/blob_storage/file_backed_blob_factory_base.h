// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLOB_STORAGE_FILE_BACKED_BLOB_FACTORY_BASE_H_
#define CONTENT_BROWSER_BLOB_STORAGE_FILE_BACKED_BLOB_FACTORY_BASE_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "content/common/content_export.h"
#include "content/public/common/child_process_id.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/blob/file_backed_blob_factory.mojom.h"
#include "url/gurl.h"

namespace content {

class ChromeBlobStorageContext;

// `FileBackedBlobFactoryBase` is an abstract class to allow the registration of
// file backed blobs. The URL used for the registration should be the URL of the
// document that bound the interface so that file access checks are performed
// against the document that actually receives the file data. To be able to
// reliably retrieve the correct URL `FileBackedBlobFactoryFrameImpl` is a
// navigation-associated interface. This way the URL is retrieved in sync with
// the navigation.
//
// A `FileBackedBlobFactoryFrameImpl` object is created once per document and
// bound to a RenderFrameHost by a
// `FileBackedBlobFactoryFrameImpl::CreateForCurrentDocument()` call made from
// `RenderFrameHostImpl::BindAccessUrlRegistry`. The lifetime of
// `FileBackedBlobFactoryFrameImpl` is the same as that of document in the
// browser process.
//
// In the case the file is registered by a worker thread we cannot use the
// browser URL as the worker generally runs concurrent to the navigation.
// Therefore we fallback to the origin URL for file registrations triggered by
// workers. The `FileBackedBlobFactoryWorkerImpl` object is bound to the
// `RenderProcessHost`

// Both variants of this class live in the UI thread and all methods are meant
// to be called from the UI thread.
class CONTENT_EXPORT FileBackedBlobFactoryBase
    : public blink::mojom::FileBackedBlobFactory {
 public:
  explicit FileBackedBlobFactoryBase(ChildProcessId process_id);
  ~FileBackedBlobFactoryBase() override;
  FileBackedBlobFactoryBase(const FileBackedBlobFactoryBase&) = delete;
  FileBackedBlobFactoryBase& operator=(const FileBackedBlobFactoryBase&) =
      delete;

  // FileBackedBlobFactory:
  void RegisterBlob(mojo::PendingReceiver<blink::mojom::Blob> blob,
                    const std::string& uuid,
                    const std::string& content_type,
                    blink::mojom::DataElementFilePtr file) override;
  void RegisterBlobSync(mojo::PendingReceiver<blink::mojom::Blob> blob,
                        const std::string& uuid,
                        const std::string& content_type,
                        blink::mojom::DataElementFilePtr file,
                        RegisterBlobSyncCallback finish_callback) override;

 protected:
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;

 private:
  virtual GURL GetCurrentUrl() = 0;
  virtual mojo::ReportBadMessageCallback GetBadMessageCallback() = 0;

  const ChildProcessId process_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLOB_STORAGE_FILE_BACKED_BLOB_FACTORY_BASE_H_
