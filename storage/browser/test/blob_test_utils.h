// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_BLOB_TEST_UTILS_H_
#define STORAGE_BROWSER_TEST_BLOB_TEST_UTILS_H_

#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace storage {

std::string BlobToString(blink::mojom::Blob* blob);

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_BLOB_TEST_UTILS_H_