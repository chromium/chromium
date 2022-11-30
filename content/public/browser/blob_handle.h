// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BLOB_HANDLE_H_
#define CONTENT_PUBLIC_BROWSER_BLOB_HANDLE_H_

#include <string>

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"

namespace content {

// A handle to Blobs that can be stored outside of content/. This class holds
// a reference to the Blob and should be used to keep alive a Blob.
class BlobHandle {
 public:
  virtual ~BlobHandle() {}
  virtual std::string GetUUID() = 0;
  virtual mojo::PendingRemote<blink::mojom::Blob> PassBlob() = 0;
  virtual blink::mojom::SerializedBlobPtr Serialize() = 0;

 protected:
  BlobHandle() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BLOB_HANDLE_H_
