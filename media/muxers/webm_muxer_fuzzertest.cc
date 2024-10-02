// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <random>
#include <string_view>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/time/time.h"
#include "media/base/audio_parameters.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_frame.h"
#include "media/muxers/live_webm_muxer_delegate.h"
#include "media/muxers/webm_muxer.h"

// Min and max number of encodec video/audio packets to send in the WebmMuxer.
const int kMinNumIterations = 1;
const int kMaxNumIterations = 10;

static const media::VideoCodec kSupportedVideoCodecs[] = {
    media::VideoCodec::kVP8, media::VideoCodec::kVP9, media::VideoCodec::kH264};
static const media::AudioCodec kSupportedAudioCodecs[] = {
    media::AudioCodec::kOpus, media::AudioCodec::kPCM};

static const int kSampleRatesInKHz[] = {48, 24, 16, 12, 8};

static struct {
  bool has_video;
  bool has_audio;
} kVideoAudioInputTypes[] = {{true, false}, {false, true}, {true, true}};

struct Env {
  Env() { logging::SetMinLogLevel(logging::LOGGING_FATAL); }

  base::SingleThreadTaskExecutor task_executor;
};
Env* env = new Env();

void OnWriteCallback(base::span<const uint8_t> data) {}

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::mt19937_64 rng;

  std::string str = std::string(reinterpret_cast<const char*>(data), size);
  {  // Seed rng from data.
    std::size_t data_hash = std::hash<std::string>()(str);
    rng.seed(data_hash);
  }

  for (const auto& input_type : kVideoAudioInputTypes) {
    const auto video_codec =
        kSupportedVideoCodecs[rng() % std::size(kSupportedVideoCodecs)];
    const auto audio_codec =
        kSupportedAudioCodecs[rng() % std::size(kSupportedAudioCodecs)];
    media::WebmMuxer muxer(audio_codec, input_type.has_video,
                           input_type.has_audio,
                           std::make_unique<media::LiveWebmMuxerDelegate>(
                               base::BindRepeating(&OnWriteCallback)),
                           std::nullopt);
    base::RunLoop().RunUntilIdle();
    bool is_first_frame = true;
    int num_iterations = kMinNumIterations + rng() % kMaxNumIterations;
    int index = 0;
    do {
      index++;
      if (input_type.has_video) {
        // VideoFrames cannot be arbitrarily small.
        const auto visible_rect = gfx::Size(16 + rng() % 128, 16 + rng() % 128);
        const auto video_frame =
            media::VideoFrame::CreateBlackFrame(visible_rect);
        const auto is_key_frame = rng() % 2;
        const auto has_alpha_frame = rng() % 4;
        auto parameters = media::Muxer::VideoParameters(*video_frame);
        parameters.codec = video_codec;
        auto buffer = media::DecoderBuffer::CopyFrom({data, size});
        if (has_alpha_frame) {
          buffer->WritableSideData().alpha_data.assign(data, data + size);
        }
        buffer->set_is_key_frame(is_key_frame != 0 || is_first_frame);
        muxer.PutFrame(media::Muxer::EncodedFrame{parameters, std::nullopt,
                                                  std::move(buffer)},
                       base::TimeDelta() + base::Milliseconds(index));
        is_first_frame = false;
        base::RunLoop().RunUntilIdle();
      }

      if (input_type.has_audio) {
        const media::ChannelLayoutConfig layout =
            rng() % 2 ? media::ChannelLayoutConfig::Stereo()
                      : media::ChannelLayoutConfig::Mono();
        const int sample_rate =
            kSampleRatesInKHz[rng() % std::size(kSampleRatesInKHz)];

        const media::AudioParameters params(
            media::AudioParameters::AUDIO_PCM_LOW_LATENCY, layout, sample_rate,
            60 * sample_rate);
        auto buffer = media::DecoderBuffer::CopyFrom({data, size});
        buffer->set_is_key_frame(true);
        muxer.PutFrame(
            media::Muxer::EncodedFrame{params, std::nullopt, std::move(buffer)},
            base::TimeDelta() + base::Milliseconds(index));
        base::RunLoop().RunUntilIdle();
      }
    } while (num_iterations--);
  }

  return 0;
}
