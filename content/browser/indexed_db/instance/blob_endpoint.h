// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_BLOB_ENDPOINT_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_BLOB_ENDPOINT_H_

#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace content::indexed_db {

// Common interface for objects that serve blobs to the renderer and track
// active connections on behalf of backing stores. It manages the "endpoints"
// (receivers) of various mojo interfaces related to blobs.
class BlobEndpoint : public blink::mojom::Blob,
                     public network::mojom::DataPipeGetter,
                     public storage::mojom::BlobDataItemReader {
 public:
  ~BlobEndpoint() override = default;

  // Like `blink::mojom::Blob::Clone()`, but called by the owner of `this`.
  virtual void AddReceiver(
      mojo::PendingReceiver<blink::mojom::Blob> receiver,
      storage::mojom::BlobStorageContext& blob_registry) = 0;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_BLOB_ENDPOINT_H_
