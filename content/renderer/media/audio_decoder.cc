// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_decoder.h"

#include <stdint.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/limits.h"
#include "media/filters/audio_file_reader.h"
#include "media/filters/in_memory_url_protocol.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/platform/web_audio_bus.h"

using media::AudioBus;
using media::AudioFileReader;
using media::InMemoryUrlProtocol;
using std::vector;
using blink::WebAudioBus;

namespace content {

// Decode in-memory audio file data.
std::unique_ptr<blink::WebAudioBus> DecodeAudioFileData(
    base::span<const char> data) {
#if BUILDFLAG(ENABLE_FFMPEG)
  const base::TimeTicks start_time = base::TimeTicks::Now();

  InMemoryUrlProtocol url_protocol(base::as_byte_span(data), false);
  auto reader = std::make_unique<AudioFileReader>(&url_protocol);
  bool open_success = reader->Open();
  base::UmaHistogramBoolean("Media.ContentAudioDecoder.CreateReaderSuccess",
                            open_success);
  if (!open_success) {
    return nullptr;
  }

  const size_t number_of_channels = reader->channels();
  const double sample_rate = reader->sample_rate();

  // Apply sanity checks to make sure crazy values aren't coming out of
  // FFmpeg.
  if (!number_of_channels ||
      number_of_channels > static_cast<size_t>(media::limits::kMaxChannels) ||
      sample_rate < media::limits::kMinSampleRate ||
      sample_rate > media::limits::kMaxSampleRate) {
    return nullptr;
  }

  std::vector<std::unique_ptr<AudioBus>> decoded_audio_packets;
  const size_t number_of_frames = reader->Read(&decoded_audio_packets);
  if (number_of_frames == 0) {
    return nullptr;
  }

  // Allocate and configure the output audio channel data and then
  // copy the decoded data to the destination.
  auto out = std::make_unique<WebAudioBus>();
  out->Initialize(number_of_channels, number_of_frames, sample_rate);

  std::vector<base::SpanWriter<float>> dest_channels;
  dest_channels.reserve(number_of_channels);
  for (size_t ch = 0; ch < number_of_channels; ++ch) {
    dest_channels.emplace_back(
        UNSAFE_TODO(base::span(out->ChannelData(ch), out->length())));
  }

  // Append all `decoded_audio_packets`, channel per channel.
  for (const auto& packet : decoded_audio_packets) {
    for (size_t ch = 0; ch < number_of_channels; ++ch) {
      dest_channels[ch].Write(packet->channel(ch));
    }
  }

  const auto duration =
      media::AudioTimestampHelper::FramesToTime(number_of_frames, sample_rate);
  DVLOG(1) << "Successfully decoded an audio file."
           << " data: " << base::ToString(data) << " data size: " << data.size()
           << ", decoded duration: " << duration
           << ", number of frames: " << number_of_frames
           << ", estimated frames (if available): "
           << (reader->HasKnownDuration() ? reader->GetNumberOfFrames() : 0)
           << ", sample rate: " << sample_rate
           << ", number of channels: " << number_of_channels;

  // NOTE: using the "medium timings" function to get better visibility into
  // behavior in the [0, 3] minute range (although the distribution tail is
  // likely to be cut off in this histogram scheme).
  base::UmaHistogramMediumTimes("Media.ContentAudioDecoder.Duration", duration);
  base::UmaHistogramTimes(
      "Media.ContentAudioDecoder.DecodeTimePerFrame",
      (base::TimeTicks::Now() - start_time) / number_of_frames);

  if (number_of_frames > 0) {
    return out;
  }
#endif  // BUILDFLAG(ENABLE_FFMPEG)

  return nullptr;
}

}  // namespace content
