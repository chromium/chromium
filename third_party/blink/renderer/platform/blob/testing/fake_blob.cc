// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/blob/testing/fake_blob.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "services/network/public/mojom/data_pipe_getter.mojom-blink.h"

namespace blink {
namespace {

class SimpleDataPipeGetter : public network::mojom::blink::DataPipeGetter {
 public:
  explicit SimpleDataPipeGetter(const Vector<uint8_t>& bytes) : bytes_(bytes) {}
  SimpleDataPipeGetter(const SimpleDataPipeGetter&) = delete;
  SimpleDataPipeGetter& operator=(const SimpleDataPipeGetter&) = delete;
  ~SimpleDataPipeGetter() override = default;

  // network::mojom::DataPipeGetter implementation:
  void Read(mojo::ScopedDataPipeProducerHandle handle,
            ReadCallback callback) override {
    std::move(callback).Run(0 /* OK */, bytes_.size());
    std::string byte_string(bytes_.begin(), bytes_.end());
    bool result = mojo::BlockingCopyFromString(byte_string, handle);
    DCHECK(result);
  }

  void Clone(mojo::PendingReceiver<network::mojom::blink::DataPipeGetter>
                 receiver) override {
    mojo::MakeSelfOwnedReceiver(std::make_unique<SimpleDataPipeGetter>(bytes_),
                                std::move(receiver));
  }

 private:
  Vector<uint8_t> bytes_;
};

}  // namespace

FakeBlob::FakeBlob(const String& uuid, const String& body, State* state)
    : uuid_(uuid), state_(state) {
  body_.assign(body.Utf8());
}

FakeBlob::FakeBlob(const String& uuid,
                   const Vector<uint8_t>& body_bytes,
                   State* state)
    : uuid_(uuid), body_(body_bytes), state_(state) {}

void FakeBlob::Clone(mojo::PendingReceiver<mojom::blink::Blob> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<FakeBlob>(uuid_, body_, state_),
                              std::move(receiver));
}

void FakeBlob::AsDataPipeGetter(
    mojo::PendingReceiver<network::mojom::blink::DataPipeGetter> receiver) {
  if (state_)
    state_->did_initiate_read_operation = true;
  mojo::MakeSelfOwnedReceiver(std::make_unique<SimpleDataPipeGetter>(body_),
                              std::move(receiver));
}

void FakeBlob::ReadRange(uint64_t offset,
                         uint64_t length,
                         mojo::ScopedDataPipeProducerHandle,
                         mojo::PendingRemote<mojom::blink::BlobReaderClient>) {
  NOTREACHED();
}

void FakeBlob::ReadAll(
    mojo::ScopedDataPipeProducerHandle handle,
    mojo::PendingRemote<mojom::blink::BlobReaderClient> client) {
  mojo::Remote<mojom::blink::BlobReaderClient> client_remote(std::move(client));
  if (state_)
    state_->did_initiate_read_operation = true;
  if (client_remote)
    client_remote->OnCalculatedSize(body_.size(), body_.size());
  std::string body_byte_string(body_.begin(), body_.end());
  bool result = mojo::BlockingCopyFromString(body_byte_string, handle);
  DCHECK(result);
  if (client_remote)
    client_remote->OnComplete(0 /* OK */, body_.size());
}

void FakeBlob::Load(
    mojo::PendingReceiver<network::mojom::blink::URLLoader>,
    const String& method,
    const net::HttpRequestHeaders&,
    mojo::PendingRemote<network::mojom::blink::URLLoaderClient>) {
  NOTREACHED();
}

void FakeBlob::ReadSideData(ReadSideDataCallback callback) {
  NOTREACHED();
}

void FakeBlob::CaptureSnapshot(CaptureSnapshotCallback callback) {
  std::move(callback).Run(body_.size(), std::nullopt);
}

void FakeBlob::GetInternalUUID(GetInternalUUIDCallback callback) {
  std::move(callback).Run(uuid_);
}

}  // namespace blink
