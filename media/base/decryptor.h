// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECRYPTOR_H_
#define MEDIA_BASE_DECRYPTOR_H_

#include <list>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "media/base/audio_buffer.h"
#include "media/base/media_export.h"

namespace media {

class AudioDecoderConfig;
class DecoderBuffer;
class VideoDecoderConfig;
class VideoFrame;

// Decrypts (and decodes) encrypted buffer.
//
// All methods are called on the (video/audio) decoder thread. Decryptor
// implementations must be thread safe when methods are called this way.
// Depending on the implementation callbacks may be fired synchronously or
// asynchronously.
class MEDIA_EXPORT Decryptor {
 public:
  enum Status {
    kSuccess,  // Decryption successfully completed. Decrypted buffer ready.
    kNoKey,    // No key is available to decrypt.
    kNeedMoreData,  // Decoder needs more data to produce a frame.
    kError,         // Key is available but an error occurred during decryption.
    kStatusMax = kError
  };

  static const char* GetStatusName(Status status);

  enum StreamType { kAudio, kVideo, kStreamTypeMax = kVideo };

  Decryptor();

  Decryptor(const Decryptor&) = delete;
  Decryptor& operator=(const Decryptor&) = delete;

  virtual ~Decryptor();

  // Indicates completion of a decryption operation.
  //
  // First parameter: The status of the decryption operation.
  // - Set to kSuccess if the encrypted buffer is successfully decrypted and
  //   the decrypted buffer is ready to be read.
  // - Set to kNoKey if no decryption key is available to decrypt the encrypted
  //   buffer. In this case the decrypted buffer must be NULL.
  // - Set to kError if unexpected error has occurred. In this case the
  //   decrypted buffer must be NULL.
  // - This parameter should not be set to kNeedMoreData.
  // Second parameter: The decrypted buffer.
  // - Only |data|, |data_size| and |timestamp| are set in the returned
  //   DecoderBuffer. The callback handler is responsible for setting other
  //   fields as appropriate.
  using DecryptCB =
      base::OnceCallback<void(Status, scoped_refptr<DecoderBuffer>)>;

  // Decrypts the |encrypted| buffer. The decrypt status and decrypted buffer
  // are returned via the provided callback |decrypt_cb|. The |encrypted| buffer
  // must not be NULL.
  // Decrypt() should not be called until any previous DecryptCB of the same
  // |stream_type| has completed. Thus, only one DecryptCB may be pending at
  // a time for a given |stream_type|.
  virtual void Decrypt(StreamType stream_type,
                       scoped_refptr<DecoderBuffer> encrypted,
                       DecryptCB decrypt_cb) = 0;

  // Cancels the scheduled decryption operation for |stream_type| and fires the
  // pending DecryptCB immediately with kSuccess and NULL.
  // Decrypt() should not be called again before the pending DecryptCB for the
  // same |stream_type| is fired.
  virtual void CancelDecrypt(StreamType stream_type) = 0;

  // Indicates completion of audio/video decoder initialization.
  //
  // First Parameter: Indicates initialization success.
  // - Set to true if initialization was successful. False if an error occurred.
  using DecoderInitCB = base::OnceCallback<void(bool)>;

  // Initializes a decoder with the given |config|, executing the |init_cb|
  // upon completion.
  virtual void InitializeAudioDecoder(const AudioDecoderConfig& config,
                                      DecoderInitCB init_cb) = 0;
  virtual void InitializeVideoDecoder(const VideoDecoderConfig& config,
                                      DecoderInitCB init_cb) = 0;

  // Helper structure for managing multiple decoded audio buffers per input.
  typedef std::list<scoped_refptr<AudioBuffer> > AudioFrames;

  // Indicates completion of audio/video decrypt-and-decode operation.
  //
  // First parameter: The status of the decrypt-and-decode operation.
  // - Set to kSuccess if the encrypted buffer is successfully decrypted and
  //   decoded. In this case, the decoded frame/buffers can be/contain:
  //   1) NULL, which means the operation has been aborted.
  //   2) End-of-stream (EOS) frame, which means that the decoder has hit EOS,
  //      flushed all internal buffers and cannot produce more video frames.
  //   3) Decrypted and decoded video frame or audio buffer.
  // - Set to kNoKey if no decryption key is available to decrypt the encrypted
  //   buffer. In this case the returned frame(s) must be NULL/empty.
  // - Set to kNeedMoreData if more data is needed to produce a video frame. In
  //   this case the returned frame(s) must be NULL/empty.
  // - Set to kError if unexpected error has occurred. In this case the
  //   returned frame(s) must be NULL/empty.
  // Second parameter: The decoded video frame or audio buffers.
  using AudioDecodeCB = base::OnceCallback<void(Status, const AudioFrames&)>;
  using VideoDecodeCB =
      base::OnceCallback<void(Status, scoped_refptr<VideoFrame>)>;

  // Decrypts and decodes the |encrypted| buffer. The status and the decrypted
  // buffer are returned via the provided callback.
  // The |encrypted| buffer must not be NULL.
  // At end-of-stream, this method should be called repeatedly with
  // end-of-stream DecoderBuffer until no frame/buffer can be produced.
  // These methods can only be called after the corresponding decoder has
  // been successfully initialized.
  // DecryptAndDecodeAudio() should not be called until any previous
  // AudioDecodeCB has completed. Thus, only one AudioDecodeCB may be pending at
  // any time. Same for DecryptAndDecodeVideo();
  virtual void DecryptAndDecodeAudio(scoped_refptr<DecoderBuffer> encrypted,
                                     AudioDecodeCB audio_decode_cb) = 0;
  virtual void DecryptAndDecodeVideo(scoped_refptr<DecoderBuffer> encrypted,
                                     VideoDecodeCB video_decode_cb) = 0;

  // Resets the decoder to an initialized clean state, cancels any scheduled
  // decrypt-and-decode operations, and fires any pending
  // AudioDecodeCB/VideoDecodeCB immediately with kError and NULL.
  // This method can only be called after the corresponding decoder has been
  // successfully initialized.
  virtual void ResetDecoder(StreamType stream_type) = 0;

  // Releases decoder resources, deinitializes the decoder, cancels any
  // scheduled initialization or decrypt-and-decode operations, and fires
  // any pending DecoderInitCB/AudioDecodeCB/VideoDecodeCB immediately.
  // DecoderInitCB should be fired with false. AudioDecodeCB/VideoDecodeCB
  // should be fired with kError.
  // This method can be called any time after Initialize{Audio|Video}Decoder()
  // has been called (with the correct stream type).
  // After this operation, the decoder is set to an uninitialized state.
  // The decoder can be reinitialized after it is uninitialized.
  virtual void DeinitializeDecoder(StreamType stream_type) = 0;

  // Returns whether or not the decryptor implementation supports decrypt-only.
  virtual bool CanAlwaysDecrypt();
};

}  // namespace media

#endif  // MEDIA_BASE_DECRYPTOR_H_
