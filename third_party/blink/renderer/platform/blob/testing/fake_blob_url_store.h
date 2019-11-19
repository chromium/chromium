// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BLOB_TESTING_FAKE_BLOB_URL_STORE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BLOB_TESTING_FAKE_BLOB_URL_STORE_H_

#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-blink.h"

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Mocked BlobURLStore implementation for testing.
class FakeBlobURLStore : public mojom::blink::BlobURLStore {
 public:
  void Register(mojo::PendingRemote<mojom::blink::Blob>,
                const KURL&,
                RegisterCallback) override;
  void Revoke(const KURL&) override;
  void Resolve(const KURL&, ResolveCallback) override;
  void ResolveAsURLLoaderFactory(
      const KURL&,
      mojo::PendingReceiver<network::mojom::blink::URLLoaderFactory>) override;
  void ResolveForNavigation(
      const KURL&,
      mojo::PendingReceiver<mojom::blink::BlobURLToken>) override;

  HashMap<KURL, mojo::Remote<mojom::blink::Blob>> registrations;
  Vector<KURL> revocations;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BLOB_TESTING_FAKE_BLOB_URL_STORE_H_
