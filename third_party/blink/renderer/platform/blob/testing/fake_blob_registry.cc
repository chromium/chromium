// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/blob/testing/fake_blob_registry.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/blob/data_element.mojom-blink.h"
#include "third_party/blink/renderer/platform/blob/testing/fake_blob.h"

namespace blink {

class FakeBlobRegistry::DataPipeDrainerClient
    : public mojo::DataPipeDrainer::Client {
 public:
  DataPipeDrainerClient(const String& uuid,
                        const String& content_type,
                        RegisterFromStreamCallback callback)
      : uuid_(uuid),
        content_type_(content_type),
        callback_(std::move(callback)) {}
  void OnDataAvailable(base::span<const uint8_t> data) override {
    length_ += data.size();
  }
  void OnDataComplete() override {
    mojo::Remote<mojom::blink::Blob> blob;
    mojo::MakeSelfOwnedReceiver(std::make_unique<FakeBlob>(uuid_),
                                blob.BindNewPipeAndPassReceiver());
    auto handle =
        BlobDataHandle::Create(uuid_, content_type_, length_, blob.Unbind());
    std::move(callback_).Run(std::move(handle));
  }

 private:
  const String uuid_;
  const String content_type_;
  RegisterFromStreamCallback callback_;
  uint64_t length_ = 0;
};

FakeBlobRegistry::FakeBlobRegistry() = default;
FakeBlobRegistry::~FakeBlobRegistry() = default;

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
    RegisterFromStreamCallback callback) {
  DCHECK(!drainer_);
  DCHECK(!drainer_client_);
  drainer_client_ = std::make_unique<DataPipeDrainerClient>(
      "someuuid", content_type, std::move(callback));
  drainer_ = std::make_unique<mojo::DataPipeDrainer>(drainer_client_.get(),
                                                     std::move(data));
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

}  // namespace blink
