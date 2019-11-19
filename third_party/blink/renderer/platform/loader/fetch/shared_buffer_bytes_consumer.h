// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SHARED_BUFFER_BYTES_CONSUMER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SHARED_BUFFER_BYTES_CONSUMER_H_

#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

// BytesConsumer to get data from a SharedBuffer.
class PLATFORM_EXPORT SharedBufferBytesConsumer final : public BytesConsumer {
 public:
  // |data| should not be modified after it passed to SharedBufferBytesConsumer.
  explicit SharedBufferBytesConsumer(scoped_refptr<const SharedBuffer> data);

  // Implements BytesConsumer.
  Result BeginRead(const char** buffer, size_t* available) override;
  Result EndRead(size_t read_size) override;
  void SetClient(Client* client) override {}
  void ClearClient() override {}
  void Cancel() override;
  PublicState GetPublicState() const override;
  Error GetError() const override {
    NOTREACHED();
    return Error();
  }
  String DebugName() const override;

 private:
  scoped_refptr<const SharedBuffer> data_;
  SharedBuffer::Iterator iterator_;
  size_t bytes_read_in_chunk_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SharedBufferBytesConsumer);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SHARED_BUFFER_BYTES_CONSUMER_H_
