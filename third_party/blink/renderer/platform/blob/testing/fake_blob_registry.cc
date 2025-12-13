// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/blob/testing/fake_blob_registry.h"

#include "base/notimplemented.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/blob/data_element.mojom-blink.h"
#include "third_party/blink/renderer/platform/blob/testing/fake_blob.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace {
namespace mojob = ::blink::mojom::blink;
class DataElementReader {
 public:
  DataElementReader(mojo::PendingReceiver<mojob::Blob> blob,
                    const blink::String& uuid,
                    blink::Vector<mojob::DataElementPtr> elements)
      : blob_(std::move(blob)),
        uuid_(uuid),
        elements_(std::move(elements)),
        elements_index_(elements_.begin()) {}

  void CreateFakeBlob() {
    if (elements_index_ == elements_.end()) {
      mojo::MakeSelfOwnedReceiver(
          std::make_unique<blink::FakeBlob>(uuid_, std::move(blob_body_bytes_)),
          std::move(blob_));
      delete this;
      return;
    }

    mojob::DataElementPtr& element = *elements_index_;
    elements_index_ = std::next(elements_index_);
    switch (element->which()) {
      case mojob::DataElement::Tag::kBytes: {
        const mojob::DataElementBytesPtr& bytes = element->get_bytes();
        blob_body_bytes_.AppendVector(*bytes->embedded_data);
        return CreateFakeBlob();
      }
      case mojob::DataElement::Tag::kFile: {
        NOTIMPLEMENTED();
        break;
      }
      case mojob::DataElement::Tag::kBlob: {
        auto& blob_element = element->get_blob();
        mojo::Remote<mojob::Blob> blob_remote(std::move(blob_element->blob));
        mojo::ScopedDataPipeProducerHandle data_pipe_producer;
        mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
        CHECK_EQ(MOJO_RESULT_OK,
                 mojo::CreateDataPipe(nullptr, data_pipe_producer,
                                      data_pipe_consumer));
        blob_remote->ReadAll(std::move(data_pipe_producer), mojo::NullRemote());
        async_reader_ = std::make_unique<DataPipeReader>(
            std::move(data_pipe_consumer), blob_body_bytes_,
            BindOnce(&DataElementReader::OnReadBlobComplete,
                     blink::Unretained(this)));
        return;
      }
    }
  }

  void OnReadBlobComplete() {
    async_reader_.reset();
    CreateFakeBlob();
  }

 private:
  class DataPipeReader : public mojo::DataPipeDrainer::Client {
   public:
    DataPipeReader(mojo::ScopedDataPipeConsumerHandle pipe,
                   blink::Vector<uint8_t>& blob_body_bytes,
                   base::OnceClosure done_callback)
        : drainer_(this, std::move(pipe)),
          blob_body_bytes_(blob_body_bytes),
          done_callback_(std::move(done_callback)) {}

    void OnDataAvailable(base::span<const uint8_t> data) override {
      blob_body_bytes_->AppendSpan(data);
    }

    void OnDataComplete() override { std::move(done_callback_).Run(); }

   private:
    mojo::DataPipeDrainer drainer_;
    raw_ref<blink::Vector<uint8_t>> blob_body_bytes_;
    base::OnceClosure done_callback_;
  };

  mojo::PendingReceiver<mojob::Blob> blob_;
  blink::String uuid_;
  blink::Vector<mojob::DataElementPtr> elements_;

  blink::Vector<mojob::DataElementPtr>::iterator elements_index_;
  blink::Vector<uint8_t> blob_body_bytes_;
  std::unique_ptr<DataPipeReader> async_reader_;
};

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

FakeBlobRegistry::FakeBlobRegistry(bool support_binary_blob_bodies)
    : support_binary_blob_bodies_(support_binary_blob_bodies) {}
FakeBlobRegistry::~FakeBlobRegistry() = default;

void FakeBlobRegistry::Register(mojo::PendingReceiver<mojom::blink::Blob> blob,
                                const String& uuid,
                                const String& content_type,
                                const String& content_disposition,
                                Vector<mojom::blink::DataElementPtr> elements,
                                RegisterCallback callback) {
  Vector<mojom::blink::DataElementPtr> body_elements;
  if (support_binary_blob_bodies_) {
    body_elements = std::move(elements);
  } else {
    registrations.push_back(Registration{
        uuid, content_type, content_disposition, std::move(elements)});
  }

  // DataElementReader will delete itself when it creates FakeBlob.
  DataElementReader* element_reader =
      new DataElementReader(std::move(blob), uuid, std::move(body_elements));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      BindOnce(&DataElementReader::CreateFakeBlob, Unretained(element_reader)));

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
