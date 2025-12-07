// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/devtools_durable_msg.h"

#include "base/functional/callback_helpers.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "net/base/io_buffer.h"
#include "net/filter/filter_source_stream.h"
#include "net/filter/source_stream.h"
#include "net/filter/source_stream_type.h"

namespace network {

class DurableMessageEncodedSourceStream : public net::SourceStream {
 public:
  explicit DurableMessageEncodedSourceStream(
      base::span<const uint8_t> encoded_bytes)
      : SourceStream(net::SourceStreamType::kNone),
        encoded_bytes_(encoded_bytes) {}

  int Read(net::IOBuffer* dest_buffer,
           int buffer_size,
           net::CompletionOnceCallback callback) override {
    size_t consume = std::min(base::checked_cast<size_t>(buffer_size),
                              encoded_bytes_.size());
    if (consume == 0) {
      return 0;
    }

    dest_buffer->span().copy_prefix_from(encoded_bytes_.take_first(consume));
    return base::checked_cast<int>(consume);
  }

  std::string Description() const override {
    return "DurableMessageEncodedSourceStream";
  }

  bool MayHaveMoreBytes() const override { return !encoded_bytes_.empty(); }

 private:
  base::raw_span<const uint8_t> encoded_bytes_;
};

DevtoolsDurableMessage::DevtoolsDurableMessage(
    std::string request_id,
    DevtoolsDurableMessageAccountingDelegate& accounting_delegate)
    : request_id_(std::move(request_id)),
      accounting_delegate_(accounting_delegate) {}

DevtoolsDurableMessage::~DevtoolsDurableMessage() {
  accounting_delegate_->WillRemoveBytes(*this);
}

void DevtoolsDurableMessage::AddBytes(base::span<const uint8_t> bytes,
                                      size_t encoded_byte_size) {
  CHECK(!is_complete_);
  base::WeakPtr<DevtoolsDurableMessage> self = GetWeakPtr();
  accounting_delegate_->WillAddBytes(*this, encoded_byte_size);
  if (!self) {
    return;
  }

  bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
  encoded_byte_size_ += encoded_byte_size;
}

mojo_base::BigBuffer DevtoolsDurableMessage::Retrieve() const {
  CHECK(is_complete_);

  if (client_decoding_types_.empty()) {
    return mojo_base::BigBuffer(bytes_);
  }

  // Stored data needs to be decoded before shipping out.
  std::unique_ptr<DurableMessageEncodedSourceStream> encoded_stream =
      std::make_unique<DurableMessageEncodedSourceStream>(bytes_);
  std::unique_ptr<net::SourceStream> decoding_stream =
      net::FilterSourceStream::CreateDecodingSourceStream(
          std::move(encoded_stream), client_decoding_types_);
  scoped_refptr<net::GrowableIOBuffer> decode_buffer =
      base::MakeRefCounted<net::GrowableIOBuffer>();
  // Set to encoded size initially.
  decode_buffer->SetCapacity(encoded_byte_size_);
  while (decoding_stream->MayHaveMoreBytes()) {
    if (decode_buffer->RemainingCapacity() == 0) {
      decode_buffer->SetCapacity(decode_buffer->capacity() * 2);
    }
    int result = decoding_stream->Read(decode_buffer.get(),
                                       decode_buffer->RemainingCapacity(),
                                       base::DoNothing());
    if (result > 0) {
      // Update the offset of the `decode_buffer` to reflect the new data.
      decode_buffer->DidConsume(result);
    } else {
      // If someone makes a FilterSourceStream that decompresses asynchronously
      // (on another thread, for example), then crash noisily.
      CHECK_NE(result, net::ERR_IO_PENDING);
    }
  }

  return mojo_base::BigBuffer(decode_buffer->span_before_offset());
}

void DevtoolsDurableMessage::MarkComplete() {
  is_complete_ = true;
}

}  // namespace network
