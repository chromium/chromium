// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/fake_blob.h"

#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace storage {

FakeBlob::FakeBlob(const std::string& uuid) : uuid_(uuid) {}

mojo::PendingRemote<blink::mojom::Blob> FakeBlob::Clone() {
  mojo::PendingRemote<blink::mojom::Blob> result;
  Clone(result.InitWithNewPipeAndPassReceiver());
  return result;
}

void FakeBlob::Clone(mojo::PendingReceiver<blink::mojom::Blob> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<FakeBlob>(uuid_),
                              std::move(receiver));
}

void FakeBlob::AsDataPipeGetter(
    mojo::PendingReceiver<network::mojom::DataPipeGetter>) {
  NOTREACHED();
}

void FakeBlob::ReadRange(uint64_t offset,
                         uint64_t size,
                         mojo::ScopedDataPipeProducerHandle,
                         mojo::PendingRemote<blink::mojom::BlobReaderClient>) {
  NOTREACHED();
}

void FakeBlob::ReadAll(mojo::ScopedDataPipeProducerHandle,
                       mojo::PendingRemote<blink::mojom::BlobReaderClient>) {
  NOTREACHED();
}

void FakeBlob::Load(mojo::PendingReceiver<network::mojom::URLLoader>,
                    const std::string& method,
                    const net::HttpRequestHeaders&,
                    mojo::PendingRemote<network::mojom::URLLoaderClient>) {
  NOTREACHED();
}

void FakeBlob::ReadSideData(ReadSideDataCallback) {
  NOTREACHED();
}

void FakeBlob::CaptureSnapshot(CaptureSnapshotCallback) {
  NOTREACHED();
}

void FakeBlob::GetInternalUUID(GetInternalUUIDCallback callback) {
  std::move(callback).Run(uuid_);
}

}  // namespace storage
