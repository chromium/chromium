// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/blob/testing/fake_blob_url_store.h"

#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"

namespace blink {

void FakeBlobURLStore::Register(
    mojo::PendingRemote<mojom::blink::Blob> blob,
    const KURL& url,
    // TODO(https://crbug.com/1224926): Remove this once experiment is over.
    const base::UnguessableToken& unsafe_agent_cluster_id,
    const std::optional<BlinkSchemefulSite>& unsafe_top_level_site,
    RegisterCallback callback) {
  registrations.insert(url, mojo::Remote<mojom::blink::Blob>(std::move(blob)));
  agent_registrations.insert(url, unsafe_agent_cluster_id);
  std::move(callback).Run();
}

void FakeBlobURLStore::Revoke(const KURL& url) {
  registrations.erase(url);
  revocations.push_back(url);
}

void FakeBlobURLStore::ResolveAsURLLoaderFactory(
    const KURL&,
    mojo::PendingReceiver<network::mojom::blink::URLLoaderFactory>,
    ResolveAsURLLoaderFactoryCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeBlobURLStore::ResolveForNavigation(
    const KURL&,
    mojo::PendingReceiver<mojom::blink::BlobURLToken>,
    ResolveForNavigationCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace blink
