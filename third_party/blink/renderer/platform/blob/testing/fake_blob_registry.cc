// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/blob/testing/fake_blob_registry.h"

#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/blob/data_element.mojom-blink.h"
#include "third_party/blink/renderer/platform/blob/testing/fake_blob.h"

namespace {
class DataPipeReader : public mojo::DataPipeDrainer::Client {
 public:
  DataPipeReader(Vector<uint8_t>* data_out, base::OnceClosure done_callback)
      : data_out_(data_out), done_callback_(std::move(done_callback)) {}

  void OnDataAvailable(base::span<const uint8_t> data) override {
    data_out_->AppendSpan(data);
  }

  void OnDataComplete() override { std::move(done_callback_).Run(); }

 private:
  raw_ptr<Vector<uint8_t>> data_out_;
  base::OnceClosure done_callback_;
};

Vector<uint8_t> ReadDataPipe(mojo::ScopedDataPipeConsumerHandle pipe) {
  base::RunLoop loop;
  Vector<uint8_t> data;
  DataPipeReader reader(&data, loop.QuitClosure());
  mojo::DataPipeDrainer drainer(&reader, std::move(pipe));
  loop.Run();
  return data;
}
}  // namespace

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
  Vector<uint8_t> blob_body_bytes;
  if (support_binary_blob_bodies_) {
    // Copy the blob's body from `elements`.
    for (const mojom::blink::DataElementPtr& element : elements) {
      switch (element->which()) {
        case mojom::blink::DataElement::Tag::kBytes: {
          const mojom::blink::DataElementBytesPtr& bytes = element->get_bytes();
          blob_body_bytes.AppendVector(*bytes->embedded_data);
          break;
        }
        case mojom::blink::DataElement::Tag::kFile: {
          NOTIMPLEMENTED();
          break;
        }
        case mojom::blink::DataElement::Tag::kBlob: {
          auto& blob_element = element->get_blob();
          mojo::Remote<mojom::blink::Blob> blob_remote(
              std::move(blob_element->blob));
          mojo::ScopedDataPipeProducerHandle data_pipe_producer;
          mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
          CHECK_EQ(MOJO_RESULT_OK,
                   mojo::CreateDataPipe(nullptr, data_pipe_producer,
                                        data_pipe_consumer));
          blob_remote->ReadAll(std::move(data_pipe_producer),
                               mojo::NullRemote());
          Vector<uint8_t> received =
              ReadDataPipe(std::move(data_pipe_consumer));
          blob_body_bytes.AppendVector(received);
          break;
        }
      }
    }
  }

  registrations.push_back(Registration{uuid, content_type, content_disposition,
                                       std::move(elements)});
  mojo::MakeSelfOwnedReceiver(std::make_unique<FakeBlob>(uuid, blob_body_bytes),
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

  // `support_binary_blob_bodies_` is not implemented for
  // `RegisterFromStream()`.
  CHECK(!support_binary_blob_bodies_);

  drainer_client_ = std::make_unique<DataPipeDrainerClient>(
      "someuuid", content_type, std::move(callback));
  drainer_ = std::make_unique<mojo::DataPipeDrainer>(drainer_client_.get(),
                                                     std::move(data));
}

}  // namespace blink
