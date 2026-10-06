// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_BLOB_TEST_UTILS_H_
#define STORAGE_BROWSER_TEST_BLOB_TEST_UTILS_H_

#include "base/containers/span.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace storage {

std::string BlobToString(blink::mojom::Blob* blob);

void RegisterBlobWithContents(mojom::BlobStorageContext& blob_storage_context,
                              mojo::PendingReceiver<blink::mojom::Blob> blob,
                              const std::string& uuid,
                              base::span<const uint8_t> data);

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_BLOB_TEST_UTILS_H_