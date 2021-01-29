// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_LIBRARY_CDM_CLEAR_KEY_CDM_FFMPEG_CDM_AUDIO_DECODER_H_
#define MEDIA_CDM_LIBRARY_CDM_CLEAR_KEY_CDM_FFMPEG_CDM_AUDIO_DECODER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "media/base/timestamp_constants.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/ffmpeg/ffmpeg_deleters.h"

struct AVCodecContext;
struct AVFrame;

namespace media {

class AudioTimestampHelper;
class CdmHostProxy;
class FFmpegDecodingLoop;

// TODO(xhwang): This class is partially cloned from FFmpegAudioDecoder. When
// FFmpegAudioDecoder is updated, it's a pain to keep this class in sync with
// FFmpegAudioDecoder. We need a long term sustainable solution for this. See
// http://crbug.com/169203
class FFmpegCdmAudioDecoder {
 public:
  explicit FFmpegCdmAudioDecoder(CdmHostProxy* cdm_host_proxy);
  ~FFmpegCdmAudioDecoder();
  bool Initialize(const cdm::AudioDecoderConfig_2& config);
  void Deinitialize();
  void Reset();

  // Decodes |compressed_buffer|. Returns |cdm::kSuccess| after storing
  // output in |decoded_frames| when output is available. Returns
  // |cdm::kNeedMoreData| when |compressed_frame| does not produce output.
  // Returns |cdm::kDecodeError| when decoding fails.
  //
  // A null |compressed_buffer| will attempt to flush the decoder of any
  // remaining frames. |compressed_buffer_size| and |timestamp| are ignored.
  cdm::Status DecodeBuffer(const uint8_t* compressed_buffer,
                           int32_t compressed_buffer_size,
                           int64_t timestamp,
                           cdm::AudioFrames* decoded_frames);

 private:
  bool OnNewFrame(
      size_t* total_size,
      std::vector<std::unique_ptr<AVFrame, ScopedPtrAVFreeFrame>>* audio_frames,
      AVFrame* frame);
  void ResetTimestampState();
  void ReleaseFFmpegResources();
  void SerializeInt64(int64_t value, uint8_t* dest);

  bool is_initialized_ = false;

  CdmHostProxy* const cdm_host_proxy_ = nullptr;

  // FFmpeg structures owned by this object.
  std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context_;
  std::unique_ptr<FFmpegDecodingLoop> decoding_loop_;

  // Audio format.
  int samples_per_second_ = 0;
  int channels_ = 0;

  // AVSampleFormat initially requested; not Chrome's SampleFormat.
  int av_sample_format_ = 0;

  // Used for computing output timestamps.
  std::unique_ptr<AudioTimestampHelper> output_timestamp_helper_;
  int bytes_per_frame_ = 0;
  base::TimeDelta last_input_timestamp_ = kNoTimestamp;

  DISALLOW_COPY_AND_ASSIGN(FFmpegCdmAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_CDM_LIBRARY_CDM_CLEAR_KEY_CDM_FFMPEG_CDM_AUDIO_DECODER_H_
