// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/blob/testing/fake_blob_url_store.h"

#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"

namespace blink {

void FakeBlobURLStore::Register(mojo::PendingRemote<mojom::blink::Blob> blob,
                                const KURL& url,
                                RegisterCallback callback) {
  registrations.insert(url, mojo::Remote<mojom::blink::Blob>(std::move(blob)));
  std::move(callback).Run();
}

void FakeBlobURLStore::Revoke(const KURL& url) {
  registrations.erase(url);
  revocations.push_back(url);
}

void FakeBlobURLStore::Resolve(const KURL& url, ResolveCallback callback) {
  auto it = registrations.find(url);
  if (it == registrations.end()) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }
  mojo::PendingRemote<mojom::blink::Blob> blob;
  it->value->Clone(blob.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(std::move(blob));
}

void FakeBlobURLStore::ResolveAsURLLoaderFactory(
    const KURL&,
    mojo::PendingReceiver<network::mojom::blink::URLLoaderFactory>) {
  NOTREACHED();
}

void FakeBlobURLStore::ResolveForNavigation(
    const KURL&,
    mojo::PendingReceiver<mojom::blink::BlobURLToken>) {
  NOTREACHED();
}

}  // namespace blink
