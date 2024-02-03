// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_NDK_AUDIO_ENCODER_H_
#define MEDIA_GPU_ANDROID_NDK_AUDIO_ENCODER_H_

#include <memory>

#include "base/android/requires_api.h"
#include "base/thread_annotations.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_encoder.h"
#include "media/base/audio_parameters.h"
#include "media/base/encoder_status.h"
#include "media/formats/mp4/aac.h"
#include "media/gpu/android/ndk_media_codec_wrapper.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

class AudioTimestampHelper;
class ConvertingAudioFifo;

// This class uses the Android NDK (the AMediaCodec APIs) to encode audio.
// It only supports encoding AAC-LC.
// This class must be created, used and destroyed on the same runner; the
// unerlying NdkMediaCodecWrapper handles thread hops from the platform encoding
// thread to `task_runner_`.
// Note: calling flush() forces a lazy re-creation of the underlying
//       `media_codec_` on the next Encode() call.
class REQUIRES_ANDROID_API(NDK_MEDIA_CODEC_MIN_API)
    MEDIA_GPU_EXPORT NdkAudioEncoder : public AudioEncoder,
                                       public NdkMediaCodecWrapper::Client {
 public:
  // `runner` - a task runner that will be used for all callbacks and external
  // calls to this instance.
  explicit NdkAudioEncoder(scoped_refptr<base::SequencedTaskRunner> runner);

  NdkAudioEncoder(const NdkAudioEncoder&) = delete;
  NdkAudioEncoder& operator=(const NdkAudioEncoder&) = delete;
  ~NdkAudioEncoder() override;

  // AudioEncoder implementation.
  void Initialize(const Options& options,
                  OutputCB output_callback,
                  EncoderStatusCB done_cb) override;

  void Encode(std::unique_ptr<AudioBus> audio_bus,
              base::TimeTicks capture_time,
              EncoderStatusCB done_cb) override;

  // Note: `media_codec_` will be destroyed after a successful flush, to be
  //        recreated with the same `options_` on the next Encode() call.
  void Flush(EncoderStatusCB done_cb) override;

  // MediaCodecWrapper::Client implementation.
  void OnInputAvailable() override;
  void OnOutputAvailable() override;
  void OnError(media_status_t error) override;

 private:
  enum class FlushState {
    kNone,             // Not currently flushing.
    kFlushingInputs,   // There is remaining data in `fifo_` left to be fed.
    kPendingEOS,       // We are waiting for the EOS from the encoder.
    kNeedsMediaCodec,  // The flush completed, but we need to lazily recreate
                       // `media_codec_`.
  };

  bool CreateAndStartMediaCodec();
  void ClearMediaCodec();

  // Input functions.
  void FeedAllInputs();
  bool InputReady();
  void FeedInput(const AudioBus* audio_bus);

  // Flush() related functions.
  void MaybeFeedEos();
  void FeedEos();
  void CompleteFlush();

  // Output reading functions.
  void DrainOutput();
  bool DrainConfig();

  // Logging and reporting functions.
  void LogError(EncoderStatus status);
  void LogAndReportError(EncoderStatus status, EncoderStatusCB done_cb);
  void ReportPendingError(EncoderStatusCB done_cb);
  void ReportOk(EncoderStatusCB done_cb);

  SEQUENCE_CHECKER(sequence_checker_);

  // A runner all for callbacks and externals calls to public methods.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The format of encoded chunks outputted through `output_cb_`.
  AudioParameters output_params_;

  bool error_occurred_ = false;

  // Delayed error status to be reported on the next Encode() or Flush() call.
  std::optional<EncoderStatus> pending_error_status_;

  // What portion of the flushing process we are in, if any.
  FlushState flush_state_ GUARDED_BY_CONTEXT(sequence_checker_) =
      NdkAudioEncoder::FlushState::kNone;

  // Pending callback for Initialize(), Encode() or Flush().
  EncoderStatusCB pending_flush_cb_;

  // All input received from Encode() not yet sent to `media_codec_`.
  std::unique_ptr<ConvertingAudioFifo> fifo_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<AudioTimestampHelper> input_timestamp_tracker_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<AudioTimestampHelper> output_timestamp_tracker_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::vector<uint8_t> codec_desc_;
  std::vector<uint8_t> temp_header_buffer_;
  mp4::AAC aac_config_parser_;

  // Platform encoder which actually performs the encoding.
  std::unique_ptr<NdkMediaCodecWrapper> media_codec_;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_NDK_AUDIO_ENCODER_H_
