// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FAKE_VIDEO_DECODER_H_
#define MEDIA_FILTERS_FAKE_VIDEO_DECODER_H_

#include <stddef.h>

#include <list>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "media/base/callback_holder.h"
#include "media/base/decoder_buffer.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "ui/gfx/geometry/size.h"

namespace media {

using BytesDecodedCB = base::RepeatingCallback<void(int)>;

class FakeVideoDecoder : public VideoDecoder {
 public:
  // Constructs an object with a decoding delay of |decoding_delay| frames.
  // |bytes_decoded_cb| is called after each decode. The sum of the byte
  // count over all calls will be equal to total_bytes_decoded().
  // Allows setting a fake ID so that tests for wrapper decoders can check
  // that underlying decoders change successfully.
  FakeVideoDecoder(int decoder_id,
                   int decoding_delay,
                   int max_parallel_decoding_requests,
                   const BytesDecodedCB& bytes_decoded_cb);

  FakeVideoDecoder(const FakeVideoDecoder&) = delete;
  FakeVideoDecoder& operator=(const FakeVideoDecoder&) = delete;

  ~FakeVideoDecoder() override;

  // Enables encrypted config supported. Must be called before Initialize().
  void EnableEncryptedConfigSupport();

  // Sets whether this decoder is a platform decoder. Must be called before
  // Initialize().
  void SetIsPlatformDecoder(bool value);

  // Decoder implementation.
  bool SupportsDecryption() const override;
  bool IsPlatformDecoder() const override;
  VideoDecoderType GetDecoderType() const override;
  int GetDecoderId() { return decoder_id_; }

  // VideoDecoder implementation
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure closure) override;
  int GetMaxDecodeRequests() const override;

  base::WeakPtr<FakeVideoDecoder> GetWeakPtr();

  // Holds the next init/decode/reset callback from firing.
  void HoldNextInit();
  void HoldDecode();
  void HoldNextReset();

  // Satisfies the pending init/decode/reset callback, which must be ready to
  // fire when these methods are called.
  void SatisfyInit();
  void SatisfyDecode();
  void SatisfyReset();

  // Satisfies single  decode request.
  void SatisfySingleDecode();

  void SimulateError();
  // Fail with status DECODER_ERROR_NOT_SUPPORTED when Initialize() is called.
  void SimulateFailureToInit();

  int total_bytes_decoded() const { return total_bytes_decoded_; }

  int total_decoded_frames() const { return total_decoded_frames_; }

  auto eos_next_configs() const { return eos_next_configs_; }

 protected:
  enum State {
    STATE_UNINITIALIZED,
    STATE_NORMAL,
    STATE_END_OF_STREAM,
    STATE_ERROR,
  };

  // Derived classes may override to customize the VideoFrame.
  virtual scoped_refptr<VideoFrame> MakeVideoFrame(const DecoderBuffer& buffer);

  // Callback for updating |total_bytes_decoded_|.
  void OnFrameDecoded(int buffer_size,
                      DecodeCB decode_cb,
                      DecoderStatus status);

  // Runs |decode_cb| or puts it to |held_decode_callbacks_| depending on
  // current value of |hold_decode_|.
  void RunOrHoldDecode(DecodeCB decode_cb);

  // Runs |decode_cb| with a frame from |decoded_frames_|.
  void RunDecodeCallback(DecodeCB decode_cb);

  void DoReset();

  SEQUENCE_CHECKER(sequence_checker_);

  const int decoder_id_;
  const size_t decoding_delay_;
  const int max_parallel_decoding_requests_;
  BytesDecodedCB bytes_decoded_cb_;

  bool is_platform_decoder_ = false;
  bool supports_encrypted_config_ = false;

  State state_;

  CallbackHolder<InitCB> init_cb_;
  CallbackHolder<base::OnceClosure> reset_cb_;

  OutputCB output_cb_;

  bool hold_decode_;
  std::list<DecodeCB> held_decode_callbacks_;

  VideoDecoderConfig current_config_;

  std::list<scoped_refptr<VideoFrame> > decoded_frames_;

  int total_bytes_decoded_;

  bool fail_to_initialize_;

  int total_decoded_frames_ = 0;

  std::vector<VideoDecoderConfig> eos_next_configs_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<FakeVideoDecoder> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_FAKE_VIDEO_DECODER_H_
