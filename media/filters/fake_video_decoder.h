// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FAKE_VIDEO_DECODER_H_
#define MEDIA_FILTERS_FAKE_VIDEO_DECODER_H_

#include <stddef.h>

#include <list>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
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
  FakeVideoDecoder(const std::string& decoder_name,
                   int decoding_delay,
                   int max_parallel_decoding_requests,
                   const BytesDecodedCB& bytes_decoded_cb);
  ~FakeVideoDecoder() override;

  // Enables encrypted config supported. Must be called before Initialize().
  void EnableEncryptedConfigSupport();

  // VideoDecoder implementation.
  std::string GetDisplayName() const override;
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

 private:
  enum State {
    STATE_UNINITIALIZED,
    STATE_NORMAL,
    STATE_END_OF_STREAM,
    STATE_ERROR,
  };

  // Callback for updating |total_bytes_decoded_|.
  void OnFrameDecoded(int buffer_size, DecodeCB decode_cb, DecodeStatus status);

  // Runs |decode_cb| or puts it to |held_decode_callbacks_| depending on
  // current value of |hold_decode_|.
  void RunOrHoldDecode(DecodeCB decode_cb);

  // Runs |decode_cb| with a frame from |decoded_frames_|.
  void RunDecodeCallback(DecodeCB decode_cb);

  void DoReset();

  base::ThreadChecker thread_checker_;

  const std::string decoder_name_;
  const size_t decoding_delay_;
  const int max_parallel_decoding_requests_;
  BytesDecodedCB bytes_decoded_cb_;

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

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<FakeVideoDecoder> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FAKE_VIDEO_DECODER_H_
