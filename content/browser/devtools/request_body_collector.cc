// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/devtools/request_body_collector.h"

#include "base/containers/extend.h"
#include "base/memory/raw_ref.h"
#include "base/numerics/safe_conversions.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"

namespace content {

class RequestBodyCollector::BodyReader : public mojo::DataPipeDrainer::Client {
 public:
  BodyReader(
      RequestBodyCollector& collector,
      mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter)
      : collector_(collector), data_pipe_getter_(std::move(data_pipe_getter)) {
    data_pipe_getter_.set_disconnect_handler(
        base::BindOnce(&BodyReader::OnFailure, base::Unretained(this)));
    mojo::ScopedDataPipeProducerHandle pipe_producer;
    MojoResult result =
        CreateDataPipe(/*options=*/nullptr, pipe_producer, pipe_consumer_);
    CHECK_EQ(MOJO_RESULT_OK, result);
    data_pipe_getter_->Read(
        std::move(pipe_producer),
        base::BindOnce(&BodyReader::OnReadStarted, base::Unretained(this)));
  }

  ~BodyReader() override = default;

 private:
  // mojo::DataPipeDrainer::Client overrides
  void OnDataAvailable(base::span<const uint8_t> data) override {
    CHECK_NE(expected_size_, 0ul);
    base::Extend(bytes_, data);
  }

  void OnDataComplete() override {
    BodyEntry entry = expected_size_ == bytes_.size()
                          ? BodyEntry(std::move(bytes_))
                          : base::unexpected("Unexpected end of data");
    collector_->OnReaderComplete(this, std::move(entry));
  }

  void OnFailure() {
    collector_->OnReaderComplete(this, base::unexpected("Error reading blob"));
    // `this` is invalid at this point.
  }

  void OnReadStarted(int32_t status, uint64_t size) {
    if (status != net::OK) {
      OnFailure();
      // `this` is invalid at this point.
      return;
    }
    expected_size_ = base::checked_cast<size_t>(size);
    bytes_.reserve(expected_size_);
    pipe_drainer_.emplace(this, std::move(pipe_consumer_));
  }

  const raw_ref<RequestBodyCollector> collector_;
  mojo::Remote<network::mojom::DataPipeGetter> data_pipe_getter_;
  size_t expected_size_ = 0;
  mojo::ScopedDataPipeConsumerHandle pipe_consumer_;
  std::optional<mojo::DataPipeDrainer> pipe_drainer_;
  std::vector<uint8_t> bytes_;
};

// static
std::unique_ptr<RequestBodyCollector> RequestBodyCollector::Collect(
    const network::ResourceRequestBody& request_body,
    CompletionCallback callback) {
  std::vector<BodyEntry> bodies;
  const auto& elements = *request_body.elements();
  bodies.resize(elements.size());
  ReadersMap readers;

  std::unique_ptr<RequestBodyCollector> collector(new RequestBodyCollector());

  for (size_t i = 0; i < elements.size(); ++i) {
    const network::DataElement& element = elements[i];
    switch (element.type()) {
      case network::DataElement::Tag::kBytes:
        bodies[i] = element.As<network::DataElementBytes>().bytes();
        break;
      case network::DataElement::Tag::kDataPipe: {
        mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter =
            element.As<network::DataElementDataPipe>().CloneDataPipeGetter();
        readers.insert(
            std::make_pair(std::make_unique<BodyReader>(
                               *collector, std::move(data_pipe_getter)),
                           i));
        break;
      }
      case network::DataElement::Tag::kFile:
      case network::DataElement::Tag::kChunkedDataPipe:
        bodies[i] = base::unexpected("Unsupported entry");
        break;
    }
  }
  if (readers.empty()) {
    std::move(callback).Run(std::move(bodies));
    return nullptr;
  }
  collector->callback_ = std::move(callback);
  collector->bodies_ = std::move(bodies);
  collector->readers_ = std::move(readers);

  return collector;
}

RequestBodyCollector::~RequestBodyCollector() = default;

RequestBodyCollector::RequestBodyCollector() = default;

void RequestBodyCollector::OnReaderComplete(BodyReader* reader,
                                            BodyEntry entry) {
  auto it = readers_.find(reader);
  CHECK(it != readers_.end());
  bodies_[it->second] = std::move(entry);
  readers_.erase(it);

  if (!readers_.empty()) {
    return;
  }
  std::move(callback_).Run(bodies_);
  // `this` may be invalid at this point due to callback invocation above.
}

}  // namespace content
