// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/blob/testing/fake_blob_url_store.h"

#include "third_party/blink/public/common/blob/blob_url.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

void FakeBlobURLStore::Register(mojo::PendingRemote<mojom::blink::Blob> blob,
                                bool security_origin_serializes_as_null,
                                RegisterCallback callback) {
  const scoped_refptr<SecurityOrigin> origin =
      SecurityOrigin::CreateUniqueOpaque();
  const KURL url(
      CreateBlobUrl(origin->ToUrlOrigin(), security_origin_serializes_as_null));
  registrations.insert(url, mojo::Remote<mojom::blink::Blob>(std::move(blob)));
  std::move(callback).Run(url);
}

void FakeBlobURLStore::Revoke(const KURL& url) {
  registrations.erase(url);
  revocations.push_back(url);
}

void FakeBlobURLStore::ResolveAsURLLoaderFactory(
    const KURL&,
    mojo::PendingReceiver<network::mojom::blink::URLLoaderFactory>) {
  NOTREACHED();
}

void FakeBlobURLStore::ResolveAsBlobURLToken(
    const KURL&,
    mojo::PendingReceiver<mojom::blink::BlobURLToken>,
    bool is_top_level_navigation) {
  NOTREACHED();
}

}  // namespace blink
