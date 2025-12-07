// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/fake_blob.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/net_errors.h"

namespace storage {

FakeBlob::FakeBlob(const std::string& uuid) : uuid_(uuid) {}
FakeBlob::~FakeBlob() = default;

mojo::PendingRemote<blink::mojom::Blob> FakeBlob::Clone() {
  mojo::PendingRemote<blink::mojom::Blob> result;
  Clone(result.InitWithNewPipeAndPassReceiver());
  return result;
}

void FakeBlob::Clone(mojo::PendingReceiver<blink::mojom::Blob> receiver) {
  auto other = std::make_unique<FakeBlob>(uuid_);
  if (body_.has_value()) {
    other->set_body(*body_);
  }
  mojo::MakeSelfOwnedReceiver(std::move(other), std::move(receiver));
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

void FakeBlob::ReadAll(
    mojo::ScopedDataPipeProducerHandle handle,
    mojo::PendingRemote<blink::mojom::BlobReaderClient> client) {
  mojo::Remote<blink::mojom::BlobReaderClient> client_remote(std::move(client));
  if (!body_) {
    if (client_remote.is_bound()) {
      client_remote->OnComplete(net::Error::ERR_FILE_NOT_FOUND, 0);
    }
  } else {
    if (client_remote.is_bound()) {
      client_remote->OnCalculatedSize(body_->size(), body_->size());
    }
    CHECK(mojo::BlockingCopyFromString(*body_, handle));
    if (client_remote.is_bound()) {
      client_remote->OnComplete(net::Error::OK, body_->size());
    }
  }
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
