// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/blob_bytes_consumer.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/form_data_bytes_consumer.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/network/wrapped_data_pipe_getter.h"

namespace blink {

BlobBytesConsumer::BlobBytesConsumer(
    ExecutionContext* execution_context,
    scoped_refptr<BlobDataHandle> blob_data_handle)
    : execution_context_(execution_context),
      blob_data_handle_(std::move(blob_data_handle)) {}

BlobBytesConsumer::~BlobBytesConsumer() {
}

BytesConsumer::Result BlobBytesConsumer::BeginRead(const char** buffer,
                                                   size_t* available) {
  if (!nested_consumer_) {
    if (!blob_data_handle_)
      return Result::kDone;

    scoped_refptr<EncodedFormData> form_data = EncodedFormData::Create();
    form_data->AppendDataPipe(base::MakeRefCounted<WrappedDataPipeGetter>(
        blob_data_handle_->AsDataPipeGetter()));
    nested_consumer_ = MakeGarbageCollected<FormDataBytesConsumer>(
        execution_context_, std::move(form_data));
    if (client_)
      nested_consumer_->SetClient(client_);
    blob_data_handle_ = nullptr;
    client_ = nullptr;
  }
  return nested_consumer_->BeginRead(buffer, available);
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

void BlobBytesConsumer::Trace(blink::Visitor* visitor) {
  visitor->Trace(execution_context_);
  visitor->Trace(nested_consumer_);
  visitor->Trace(client_);
  BytesConsumer::Trace(visitor);
}

}  // namespace blink
