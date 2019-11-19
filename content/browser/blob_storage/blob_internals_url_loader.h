// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLOB_STORAGE_BLOB_INTERNALS_URL_LOADER_H_
#define CONTENT_BROWSER_BLOB_STORAGE_BLOB_INTERNALS_URL_LOADER_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {
class ChromeBlobStorageContext;

void StartBlobInternalsURLLoader(
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
    ChromeBlobStorageContext* blob_storage_context);
}  // namespace content

#endif  // CONTENT_BROWSER_BLOB_STORAGE_VIEW_BLOB_INTERNALS_URL_LOADER_H_
