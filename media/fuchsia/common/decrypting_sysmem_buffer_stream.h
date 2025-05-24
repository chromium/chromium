// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_COMMON_DECRYPTING_SYSMEM_BUFFER_STREAM_H_
#define MEDIA_FUCHSIA_COMMON_DECRYPTING_SYSMEM_BUFFER_STREAM_H_

#include <deque>

#include "base/memory/raw_ptr.h"
#include "media/base/cdm_context.h"
#include "media/base/decryptor.h"
#include "media/fuchsia/common/passthrough_sysmem_buffer_stream.h"

namespace media {

// A SysmemBufferStream that decrypts the stream before copying the data to
// sysmem buffers.
class MEDIA_EXPORT DecryptingSysmemBufferStream : public SysmemBufferStream {
 public:
  DecryptingSysmemBufferStream(SysmemAllocatorClient* sysmem_allocator,
                               CdmContext* cdm_context,
                               Decryptor::StreamType stream_type);
  ~DecryptingSysmemBufferStream() override;

  DecryptingSysmemBufferStream(const DecryptingSysmemBufferStream&) = delete;
  DecryptingSysmemBufferStream& operator=(const DecryptingSysmemBufferStream&) =
      delete;

  // SysmemBufferStream implementation:
  void Initialize(Sink* sink,
                  size_t min_buffer_size,
                  size_t min_buffer_count) override;
  void EnqueueBuffer(scoped_refptr<DecoderBuffer> buffer) override;
  void Reset() override;

 private:
  enum class State {
    kIdle,
    kDecryptPending,
    kWaitingKey,
  };

  void OnCdmContextEvent(CdmContext::Event event);
  void DecryptNextBuffer();
  void OnBufferDecrypted(Decryptor::Status status,
                         scoped_refptr<DecoderBuffer> decrypted_buffer);

  PassthroughSysmemBufferStream passthrough_stream_;
  const raw_ptr<Decryptor> decryptor_;
  const Decryptor::StreamType stream_type_;

  std::unique_ptr<CallbackRegistration> event_cb_registration_;

  raw_ptr<Sink> sink_ = nullptr;
  std::deque<scoped_refptr<DecoderBuffer>> buffer_queue_;
  State state_ = State::kIdle;

  bool retry_on_no_key_ = false;

  base::WeakPtrFactory<DecryptingSysmemBufferStream> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_COMMON_DECRYPTING_SYSMEM_BUFFER_STREAM_H_