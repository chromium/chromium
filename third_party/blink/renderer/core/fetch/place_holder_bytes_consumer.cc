// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/place_holder_bytes_consumer.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

BytesConsumer::Result PlaceHolderBytesConsumer::BeginRead(
    base::span<const char>& buffer) {
  if (!underlying_) {
    buffer = {};
    return is_cancelled_ ? Result::kDone : Result::kShouldWait;
  }
  return underlying_->BeginRead(buffer);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!client_);
  DCHECK(client);
  if (underlying_)
    underlying_->SetClient(client);
  else
    client_ = client;
}

void PlaceHolderBytesConsumer::ClearClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (underlying_)
    underlying_->ClearClient();
  else
    client_ = nullptr;
}

void PlaceHolderBytesConsumer::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (underlying_) {
    underlying_->Cancel();
  } else {
    is_cancelled_ = true;
    client_ = nullptr;
  }
}

BytesConsumer::PublicState PlaceHolderBytesConsumer::GetPublicState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return underlying_ ? underlying_->GetPublicState()
                     : is_cancelled_ ? PublicState::kClosed
                                     : PublicState::kReadableOrWaiting;
}

BytesConsumer::Error PlaceHolderBytesConsumer::GetError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(underlying_);
  // We must not be in the errored state until we get updated.
  return underlying_->GetError();
}

String PlaceHolderBytesConsumer::DebugName() const {
  StringBuilder builder;
  builder.Append("PlaceHolderBytesConsumer(");
  builder.Append(underlying_ ? underlying_->DebugName() : "<nullptr>");
  builder.Append(")");
  return builder.ToString();
}

// This function can be called at most once.
void PlaceHolderBytesConsumer::Update(BytesConsumer* consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
