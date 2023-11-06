// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLOB_STORAGE_FILE_BACKED_BLOB_FACTORY_WORKER_IMPL_H_
#define CONTENT_BROWSER_BLOB_STORAGE_FILE_BACKED_BLOB_FACTORY_WORKER_IMPL_H_

#include "content/browser/blob_storage/file_backed_blob_factory_base.h"

#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/blob/file_backed_blob_factory.mojom.h"
#include "url/gurl.h"

namespace content {

class CONTENT_EXPORT FileBackedBlobFactoryWorkerImpl
    : public FileBackedBlobFactoryBase {
 public:
  explicit FileBackedBlobFactoryWorkerImpl(BrowserContext* browser_context,
                                           int process_id);
  ~FileBackedBlobFactoryWorkerImpl() override;
  void BindReceiver(
      mojo::PendingReceiver<blink::mojom::FileBackedBlobFactory> receiver,
      const GURL& url);

  // FileBackedBlobFactoryBase:
  GURL GetCurrentUrl() override;
  mojo::ReportBadMessageCallback GetBadMessageCallback() override;

 private:
  struct BindingContext {
    const GURL url;
  };

  mojo::ReceiverSet<FileBackedBlobFactory, BindingContext> receiver_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLOB_STORAGE_FILE_BACKED_BLOB_FACTORY_WORKER_IMPL_H_
