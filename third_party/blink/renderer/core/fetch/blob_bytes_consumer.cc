// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/blob_bytes_consumer.h"

#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/loader/fetch/data_pipe_bytes_consumer.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/network/wrapped_data_pipe_getter.h"

namespace blink {

// Class implementing the BlobReaderClient interface.  This is used to
// propagate the completion of blob read to the DataPipeBytesConsumer.
class BlobBytesConsumer::BlobClient
    : public GarbageCollected<BlobBytesConsumer::BlobClient>,
      public mojom::blink::BlobReaderClient {
 public:
  BlobClient(ExecutionContext* context,
             DataPipeBytesConsumer::CompletionNotifier* completion_notifier)
      : client_receiver_(this, context),
        completion_notifier_(completion_notifier),
        task_runner_(context->GetTaskRunner(TaskType::kNetworking)) {}
  BlobClient(const BlobClient&) = delete;
  BlobClient& operator=(const BlobClient&) = delete;

  mojo::PendingRemote<mojom::blink::BlobReaderClient>
  BindNewPipeAndPassRemote() {
    return client_receiver_.BindNewPipeAndPassRemote(task_runner_);
  }

  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override {}

  void OnComplete(int32_t status, uint64_t data_length) override {
    client_receiver_.reset();

    // 0 is net::OK
    if (status == 0)
      completion_notifier_->SignalComplete();
    else
      completion_notifier_->SignalError(BytesConsumer::Error());
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(completion_notifier_);
    visitor->Trace(client_receiver_);
  }

 private:
  HeapMojoReceiver<mojom::blink::BlobReaderClient, BlobClient> client_receiver_;
  Member<DataPipeBytesConsumer::CompletionNotifier> completion_notifier_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

BlobBytesConsumer::BlobBytesConsumer(
    ExecutionContext* execution_context,
    scoped_refptr<BlobDataHandle> blob_data_handle)
    : execution_context_(execution_context),
      blob_data_handle_(std::move(blob_data_handle)) {}

BlobBytesConsumer::~BlobBytesConsumer() = default;

BytesConsumer::Result BlobBytesConsumer::BeginRead(
    base::span<const char>& buffer) {
  if (!nested_consumer_) {
    if (!blob_data_handle_)
      return Result::kDone;

    // Create a DataPipe to transport the data from the blob.
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes =
        blink::BlobUtils::GetDataPipeCapacity(blob_data_handle_->size());

    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    mojo::ScopedDataPipeProducerHandle producer_handle;
    MojoResult rv =
        mojo::CreateDataPipe(&options, producer_handle, consumer_handle);
    if (rv != MOJO_RESULT_OK)
      return Result::kError;

    // Setup the DataPipe consumer.
    DataPipeBytesConsumer::CompletionNotifier* completion_notifier;
    nested_consumer_ = MakeGarbageCollected<DataPipeBytesConsumer>(
        execution_context_->GetTaskRunner(TaskType::kNetworking),
        std::move(consumer_handle), &completion_notifier);
    if (client_)
      nested_consumer_->SetClient(client_);

    // Start reading the blob.
    blob_client_ = MakeGarbageCollected<BlobClient>(execution_context_,
                                                    completion_notifier);
    blob_data_handle_->ReadAll(std::move(producer_handle),
                               blob_client_->BindNewPipeAndPassRemote());

    blob_data_handle_ = nullptr;
    client_ = nullptr;
  }
  return nested_consumer_->BeginRead(buffer);
}

BytesConsumer::Result BlobBytesConsumer::EndRead(size_t read) {
  DCHECK(nested_consumer_);
  return nested_consumer_->EndRead(read);
}

scoped_refptr<BlobDataHandle> BlobBytesConsumer::DrainAsBlobDataHandle(
    BlobSizePolicy policy) {
  if (!blob_data_handle_)
    return nullptr;
  if (policy == BlobSizePolicy::kDisallowBlobWithInvalidSize &&
      blob_data_handle_->size() == UINT64_MAX)
    return nullptr;
  return std::move(blob_data_handle_);
}

scoped_refptr<EncodedFormData> BlobBytesConsumer::DrainAsFormData() {
  scoped_refptr<BlobDataHandle> handle =
      DrainAsBlobDataHandle(BlobSizePolicy::kAllowBlobWithInvalidSize);
  if (!handle)
    return nullptr;
  scoped_refptr<EncodedFormData> form_data = EncodedFormData::Create();
  form_data->AppendBlob(handle->Uuid(), handle);
  return form_data;
}

void BlobBytesConsumer::SetClient(BytesConsumer::Client* client) {
  DCHECK(!client_);
  DCHECK(client);
  if (nested_consumer_)
    nested_consumer_->SetClient(client);
  else
    client_ = client;
}

void BlobBytesConsumer::ClearClient() {
  client_ = nullptr;
  if (nested_consumer_)
    nested_consumer_->ClearClient();
}

void BlobBytesConsumer::Cancel() {
  if (nested_consumer_)
    nested_consumer_->Cancel();
  blob_data_handle_ = nullptr;
  client_ = nullptr;
}

BytesConsumer::Error BlobBytesConsumer::GetError() const {
  DCHECK(nested_consumer_);
  return nested_consumer_->GetError();
}

BytesConsumer::PublicState BlobBytesConsumer::GetPublicState() const {
  if (!nested_consumer_) {
    return blob_data_handle_ ? PublicState::kReadableOrWaiting
                             : PublicState::kClosed;
  }
  return nested_consumer_->GetPublicState();
}

void BlobBytesConsumer::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
  visitor->Trace(blob_client_);
  visitor->Trace(nested_consumer_);
  visitor->Trace(client_);
  BytesConsumer::Trace(visitor);
}

}  // namespace blink
