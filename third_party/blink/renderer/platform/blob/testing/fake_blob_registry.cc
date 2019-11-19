// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/blob/testing/fake_blob_registry.h"

#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/blob/data_element.mojom-blink.h"
#include "third_party/blink/renderer/platform/blob/testing/fake_blob.h"

namespace blink {

void FakeBlobRegistry::Register(mojo::PendingReceiver<mojom::blink::Blob> blob,
                                const String& uuid,
                                const String& content_type,
                                const String& content_disposition,
                                Vector<mojom::blink::DataElementPtr> elements,
                                RegisterCallback callback) {
  registrations.push_back(Registration{uuid, content_type, content_disposition,
                                       std::move(elements)});
  mojo::MakeSelfOwnedReceiver(std::make_unique<FakeBlob>(uuid),
                              std::move(blob));
  std::move(callback).Run();
}

void FakeBlobRegistry::RegisterFromStream(
    const String& content_type,
    const String& content_disposition,
    uint64_t expected_length,
    mojo::ScopedDataPipeConsumerHandle data,
    mojo::PendingAssociatedRemote<mojom::blink::ProgressClient>,
    RegisterFromStreamCallback) {
  NOTREACHED();
}

void FakeBlobRegistry::GetBlobFromUUID(
    mojo::PendingReceiver<mojom::blink::Blob> blob,
    const String& uuid,
    GetBlobFromUUIDCallback callback) {
  owned_receivers.push_back(OwnedReceiver{uuid});
  mojo::MakeSelfOwnedReceiver(std::make_unique<FakeBlob>(uuid),
                              std::move(blob));
  std::move(callback).Run();
}

void FakeBlobRegistry::URLStoreForOrigin(
    const scoped_refptr<const SecurityOrigin>& origin,
    mojo::PendingAssociatedReceiver<mojom::blink::BlobURLStore> receiver) {
  NOTREACHED();
}

}  // namespace blink
