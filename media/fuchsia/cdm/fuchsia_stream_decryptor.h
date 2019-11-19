// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_CDM_FUCHSIA_STREAM_DECRYPTOR_H_
#define MEDIA_FUCHSIA_CDM_FUCHSIA_STREAM_DECRYPTOR_H_

#include <fuchsia/media/drm/cpp/fidl.h>

#include <memory>

#include "base/sequence_checker.h"
#include "media/base/decryptor.h"
#include "media/fuchsia/common/stream_processor_helper.h"
#include "media/fuchsia/common/sysmem_buffer_pool.h"
#include "media/fuchsia/common/sysmem_buffer_writer_queue.h"

namespace media {
class SysmemBufferReader;

// Base class for media stream decryptor implementations.
class FuchsiaStreamDecryptorBase : public StreamProcessorHelper::Client {
 public:
  explicit FuchsiaStreamDecryptorBase(
      fuchsia::media::StreamProcessorPtr processor);
  ~FuchsiaStreamDecryptorBase() override;

 protected:
  // StreamProcessorHelper::Client overrides.
  void AllocateInputBuffers(
      const fuchsia::media::StreamBufferConstraints& stream_constraints) final;
  void OnOutputFormat(fuchsia::media::StreamOutputFormat format) final;

  void DecryptInternal(scoped_refptr<DecoderBuffer> encrypted);
  void ResetStream();

  StreamProcessorHelper processor_;

  BufferAllocator allocator_;

  SysmemBufferWriterQueue input_writer_queue_;

  // Key ID for which we received the last OnNewKey() event.
  std::string last_new_key_id_;

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  void OnInputBufferPoolCreated(std::unique_ptr<SysmemBufferPool> pool);
  void OnWriterCreated(std::unique_ptr<SysmemBufferWriter> writer);
  void SendInputPacket(const DecoderBuffer* buffer,
                       StreamProcessorHelper::IoPacket packet);
  void ProcessEndOfStream();

  std::unique_ptr<SysmemBufferPool::Creator> input_pool_creator_;
  std::unique_ptr<SysmemBufferPool> input_pool_;

  DISALLOW_COPY_AND_ASSIGN(FuchsiaStreamDecryptorBase);
};

// Stream decryptor that copies output to clear DecodeBuffer's. Used for audio
// streams.
class FuchsiaClearStreamDecryptor : public FuchsiaStreamDecryptorBase {
 public:
  static std::unique_ptr<FuchsiaClearStreamDecryptor> Create(
      fuchsia::media::drm::ContentDecryptionModule* cdm);

  FuchsiaClearStreamDecryptor(fuchsia::media::StreamProcessorPtr processor);
  ~FuchsiaClearStreamDecryptor() override;

  // Decrypt() behavior should match media::Decryptor interface.
  void Decrypt(scoped_refptr<DecoderBuffer> encrypted,
               Decryptor::DecryptCB decrypt_cb);
  void CancelDecrypt();

 private:
  // StreamProcessorHelper::Client overrides.
  void AllocateOutputBuffers(
      const fuchsia::media::StreamBufferConstraints& stream_constraints) final;
  void OnProcessEos() final;
  void OnOutputPacket(StreamProcessorHelper::IoPacket packet) final;
  void OnNoKey() final;
  void OnError() final;

  void OnOutputBufferPoolCreated(size_t num_buffers_for_client,
                                 size_t num_buffers_for_server,
                                 std::unique_ptr<SysmemBufferPool> pool);
  void OnOutputBufferPoolReaderCreated(
      std::unique_ptr<SysmemBufferReader> reader);

  Decryptor::DecryptCB decrypt_cb_;

  std::unique_ptr<SysmemBufferPool::Creator> output_pool_creator_;
  std::unique_ptr<SysmemBufferPool> output_pool_;
  std::unique_ptr<SysmemBufferReader> output_reader_;

  // Used to re-assemble decrypted output that was split between multiple sysmem
  // buffers.
  Decryptor::Status current_status_ = Decryptor::kSuccess;
  std::vector<uint8_t> output_data_;

  DISALLOW_COPY_AND_ASSIGN(FuchsiaClearStreamDecryptor);
};

// Stream decryptor that decrypts data to protected sysmem buffers. Used for
// video stream.
class FuchsiaSecureStreamDecryptor : public FuchsiaStreamDecryptorBase {
 public:
  class Client {
   public:
    virtual void OnDecryptorOutputPacket(
        StreamProcessorHelper::IoPacket packet) = 0;
    virtual void OnDecryptorEndOfStreamPacket() = 0;
    virtual void OnDecryptorError() = 0;
    virtual void OnDecryptorNoKey() = 0;

   protected:
    virtual ~Client() = default;
  };

  using NewKeyCB = base::RepeatingCallback<void(const std::string& key_id)>;

  FuchsiaSecureStreamDecryptor(fuchsia::media::StreamProcessorPtr processor,
                               Client* client);
  ~FuchsiaSecureStreamDecryptor() override;

  void SetOutputBufferCollectionToken(
      fuchsia::sysmem::BufferCollectionTokenPtr token,
      size_t num_buffers_for_decryptor,
      size_t num_buffers_for_codec);

  // Enqueues the specified buffer to the input queue. Caller is allowed to
  // queue as many buffers as it needs without waiting for results from the
  // previous Decrypt() calls.
  void Decrypt(scoped_refptr<DecoderBuffer> encrypted);

  // Returns closure that should be called when the key changes. This class
  // uses this notification to handle NO_KEY errors. Note that this class can
  // queue multiple input buffers so it's also responsible for resubmitting
  // queued buffers after a new key is received. This is different from
  // FuchsiaClearStreamDecryptor and media::Decryptor: they report NO_KEY error
  // to the caller and expect the caller to resubmit same buffers again after
  // the key is updated.
  NewKeyCB GetOnNewKeyClosure();

  // Drops all pending decryption requests.
  void Reset();

 private:
  // StreamProcessorHelper::Client overrides.
  void AllocateOutputBuffers(
      const fuchsia::media::StreamBufferConstraints& stream_constraints) final;
  void OnProcessEos() final;
  void OnOutputPacket(StreamProcessorHelper::IoPacket packet) final;
  void OnNoKey() final;
  void OnError() final;

  // Callback returned by GetOnNewKeyClosure(). When waiting for a key this
  // method unpauses the stream to decrypt any pending buffers.
  void OnNewKey(const std::string& key_id);

  Client* const client_;

  bool waiting_output_buffers_ = false;
  base::OnceClosure complete_buffer_allocation_callback_;

  // Set to true if some keys have been updated recently. New key notifications
  // are received from a LicenseSession, while DECRYPTOR_NO_KEY error is
  // received from StreamProcessor. These are separate FIDL connections that are
  // handled on different threads, so they are not synchronized. As result
  // OnNewKey() may be called before we get OnNoKey(). To handle this case
  // correctly OnNewKey() sets |retry_on_no_key_| and then OnNoKey() tries to
  // restart the stream immediately if this flag is set.
  bool retry_on_no_key_ = false;

  // Set to true if the stream is paused while we are waiting for new keys.
  bool waiting_for_key_ = false;

  base::WeakPtrFactory<FuchsiaSecureStreamDecryptor> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FuchsiaSecureStreamDecryptor);
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_CDM_FUCHSIA_STREAM_DECRYPTOR_H_
