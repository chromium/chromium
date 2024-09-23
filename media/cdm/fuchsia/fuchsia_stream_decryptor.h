// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_FUCHSIA_FUCHSIA_STREAM_DECRYPTOR_H_
#define MEDIA_CDM_FUCHSIA_FUCHSIA_STREAM_DECRYPTOR_H_

#include <fuchsia/media/drm/cpp/fidl.h>

#include <memory>

#include "base/sequence_checker.h"
#include "media/base/decryptor.h"
#include "media/base/media_export.h"
#include "media/fuchsia/common/stream_processor_helper.h"
#include "media/fuchsia/common/sysmem_buffer_stream.h"
#include "media/fuchsia/common/sysmem_client.h"
#include "media/fuchsia/common/vmo_buffer_writer_queue.h"

namespace media {

// Stream decryptor used to decrypt protected media streams on Fuchsia. Must be
// created an used on the same thread.
class MEDIA_EXPORT FuchsiaStreamDecryptor
    : public SysmemBufferStream,
      public StreamProcessorHelper::Client {
 public:
  // Number of buffers that the decryptor allocates for input buffer
  // collection. Decryptors provided by fuchsia.media.drm API normally decrypt a
  // single buffer at a time. Second buffer is useful to allow reading/writing a
  // packet while the decryptor is working on another one.
  static constexpr size_t kInputBufferCount = 2;

  explicit FuchsiaStreamDecryptor(fuchsia::media::StreamProcessorPtr processor);
  ~FuchsiaStreamDecryptor() override;

  FuchsiaStreamDecryptor(const FuchsiaStreamDecryptor&) = delete;
  FuchsiaStreamDecryptor& operator=(const FuchsiaStreamDecryptor&) = delete;

  // Returns closure that should be called when the key changes. This class
  // uses this notification to handle NO_KEY errors. Note that this class can
  // queue multiple input buffers so it's also responsible for resubmitting
  // queued buffers after a new key is received. This is different from
  // FuchsiaClearStreamDecryptor and media::Decryptor: they report NO_KEY error
  // to the caller and expect the caller to resubmit same buffers again after
  // the key is updated.
  base::RepeatingClosure GetOnNewKeyClosure();

  // SysmemBufferStream implementation.
  void Initialize(Sink* sink,
                  size_t min_buffer_size,
                  size_t min_buffer_count) override;
  void EnqueueBuffer(scoped_refptr<DecoderBuffer> buffer) override;
  void Reset() override;

 private:
  // StreamProcessorHelper::Client overrides.
  void OnStreamProcessorAllocateOutputBuffers(
      const fuchsia::media::StreamBufferConstraints& stream_constraints) final;
  void OnStreamProcessorEndOfStream() final;
  void OnStreamProcessorOutputFormat(
      fuchsia::media::StreamOutputFormat format) final;
  void OnStreamProcessorOutputPacket(
      StreamProcessorHelper::IoPacket packet) final;
  void OnStreamProcessorNoKey() final;
  void OnStreamProcessorError() final;

  void OnError();

  void OnInputBuffersAcquired(
      std::vector<VmoBuffer> buffers,
      const fuchsia::sysmem2::SingleBufferSettings& buffer_settings);
  void SendInputPacket(const DecoderBuffer* buffer,
                       StreamProcessorHelper::IoPacket packet);
  void ProcessEndOfStream();

  // Callback returned by GetOnNewKeyClosure(). When waiting for a key this
  // method unpauses the stream to decrypt any pending buffers.
  void OnNewKey();

  StreamProcessorHelper processor_;

  SysmemAllocatorClient allocator_;

  Sink* sink_ = nullptr;

  size_t min_buffer_size_ = 0;
  size_t min_buffer_count_ = 0;

  std::unique_ptr<SysmemCollectionClient> input_buffer_collection_;
  VmoBufferWriterQueue input_writer_queue_;

  std::unique_ptr<SysmemCollectionClient> output_buffer_collection_;

  // Key ID for which we received the last OnNewKey() event.
  std::string last_new_key_id_;

  // Set to true if some keys have been updated recently. New key notifications
  // are received from a LicenseSession, while DECRYPTOR_NO_KEY error is
  // received from StreamProcessor. These are separate FIDL connections that are
  // handled on different threads, so they are not synchronized. As result
  // OnNewKey() may be called before we get OnNoKey(). To handle this case
  // correctly OnNewKey() sets |retry_on_no_key_event_| and then OnNoKey() tries
  // to restart the stream immediately if this flag is set.
  bool retry_on_no_key_event_ = false;

  // Set to true if the stream is paused while we are waiting for new keys.
  bool waiting_for_key_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FuchsiaStreamDecryptor> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CDM_FUCHSIA_FUCHSIA_STREAM_DECRYPTOR_H_
