// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/place_holder_bytes_consumer.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

BytesConsumer::Result PlaceHolderBytesConsumer::BeginRead(const char** buffer,
                                                          size_t* available) {
  if (!underlying_) {
    *buffer = nullptr;
    *available = 0;
    return is_cancelled_ ? Result::kDone : Result::kShouldWait;
  }
  return underlying_->BeginRead(buffer, available);
}

BytesConsumer::Result PlaceHolderBytesConsumer::EndRead(size_t read_size) {
  DCHECK(underlying_);
  return underlying_->EndRead(read_size);
}

scoped_refptr<BlobDataHandle> PlaceHolderBytesConsumer::DrainAsBlobDataHandle(
    BlobSizePolicy policy) {
  return underlying_ ? underlying_->DrainAsBlobDataHandle(policy) : nullptr;
}

scoped_refptr<EncodedFormData> PlaceHolderBytesConsumer::DrainAsFormData() {
  return underlying_ ? underlying_->DrainAsFormData() : nullptr;
}

mojo::ScopedDataPipeConsumerHandle PlaceHolderBytesConsumer::DrainAsDataPipe() {
  if (!underlying_) {
    return {};
  }
  return underlying_->DrainAsDataPipe();
}

void PlaceHolderBytesConsumer::SetClient(BytesConsumer::Client* client) {
  DCHECK(!client_);
  DCHECK(client);
  if (underlying_)
    underlying_->SetClient(client);
  else
    client_ = client;
}

void PlaceHolderBytesConsumer::ClearClient() {
  if (underlying_)
    underlying_->ClearClient();
  else
    client_ = nullptr;
}

void PlaceHolderBytesConsumer::Cancel() {
  if (underlying_) {
    underlying_->Cancel();
  } else {
    is_cancelled_ = true;
    client_ = nullptr;
  }
}

BytesConsumer::PublicState PlaceHolderBytesConsumer::GetPublicState() const {
  return underlying_ ? underlying_->GetPublicState()
                     : is_cancelled_ ? PublicState::kClosed
                                     : PublicState::kReadableOrWaiting;
}

BytesConsumer::Error PlaceHolderBytesConsumer::GetError() const {
  DCHECK(underlying_);
  // We must not be in the errored state until we get updated.
  return underlying_->GetError();
}

// This function can be called at most once.
void PlaceHolderBytesConsumer::Update(BytesConsumer* consumer) {
  DCHECK(!underlying_);
  if (is_cancelled_) {
    // This consumer has already been closed.
    return;
  }

  underlying_ = consumer;
  if (client_) {
    Client* client = client_;
    client_ = nullptr;
    underlying_->SetClient(client);
    client->OnStateChange();
  }
}

void PlaceHolderBytesConsumer::Trace(Visitor* visitor) const {
  visitor->Trace(underlying_);
  visitor->Trace(client_);
  BytesConsumer::Trace(visitor);
}

}  // namespace blink
