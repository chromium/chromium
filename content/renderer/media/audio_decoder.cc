// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_decoder.h"

#include <stdint.h>

#include <variant>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/filters/audio_file_reader.h"
#include "media/filters/in_memory_url_protocol.h"
#include "media/filters/legacy_audio_file_reader.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/platform/web_audio_bus.h"

using media::AudioBus;
using media::AudioFileReader;
using media::InMemoryUrlProtocol;
using std::vector;
using blink::WebAudioBus;

namespace content {

namespace {

#if BUILDFLAG(ENABLE_FFMPEG)
// AudioFileReader and LegacyAudioFileReader do not share an interface. Since
// the legacy implementation is planned to be removed soon, this is the only
// usage of these classes, and the interface is small, we instead have a
// temporary reader class.
//
// TODO(crbug.com/440616500): remove this class once the new AudioFileReader
// has sufficiently baked in Stable.
class Reader {
 public:
  static std::unique_ptr<Reader> Create(base::span<const char> data) {
    auto out = base::WrapUnique(new Reader(data));
    return out->Open() ? std::move(out) : nullptr;
  }

  Reader(const Reader&) = delete;
  Reader(Reader&&) = delete;
  Reader& operator=(const Reader&) = delete;
  Reader& operator=(Reader&&) = delete;

  size_t Read(std::vector<std::unique_ptr<AudioBus>>* decoded_audio_packets) {
    if (std::holds_alternative<media::AudioFileReader>(*reader_)) {
      return std::get<media::AudioFileReader>(*reader_).Read(
          decoded_audio_packets);
    }
    return base::checked_cast<size_t>(
        std::get<media::LegacyAudioFileReader>(*reader_).Read(
            decoded_audio_packets));
  }

  size_t estimated_frames() const {
    if (std::holds_alternative<media::AudioFileReader>(*reader_)) {
      const auto& r = std::get<media::AudioFileReader>(*reader_);
      return r.HasKnownDuration() ? r.GetNumberOfFrames() : 0;
    }
    const auto& r = std::get<media::LegacyAudioFileReader>(*reader_);
    return r.HasKnownDuration() ? r.GetNumberOfFrames() : 0;
  }

  size_t channels() const {
    if (std::holds_alternative<media::AudioFileReader>(*reader_)) {
      return std::get<media::AudioFileReader>(*reader_).channels();
    }
    return std::get<media::LegacyAudioFileReader>(*reader_).channels();
  }

  double sample_rate() const {
    if (std::holds_alternative<media::AudioFileReader>(*reader_)) {
      return std::get<media::AudioFileReader>(*reader_).sample_rate();
    }
    return std::get<media::LegacyAudioFileReader>(*reader_).sample_rate();
  }

 private:
  explicit Reader(base::span<const char> data)
      : url_protocol_(base::as_byte_span(data), false) {
    if (base::FeatureList::IsEnabled(media::kAudioDecoderAudioFileReader)) {
      reader_ = std::make_unique<ReaderImpl>(
          std::in_place_type<media::AudioFileReader>, &url_protocol_);
    } else {
      reader_ = std::make_unique<ReaderImpl>(
          std::in_place_type<media::LegacyAudioFileReader>, &url_protocol_);
    }
  }

  bool Open() {
    if (std::holds_alternative<media::AudioFileReader>(*reader_)) {
      return std::get<media::AudioFileReader>(*reader_).Open();
    }
    return std::get<media::LegacyAudioFileReader>(*reader_).Open();
  }

  InMemoryUrlProtocol url_protocol_;

  using ReaderImpl =
      std::variant<media::AudioFileReader, media::LegacyAudioFileReader>;
  std::unique_ptr<ReaderImpl> reader_;
};

#endif
}  // namespace

// Decode in-memory audio file data.
bool DecodeAudioFileData(blink::WebAudioBus* destination_bus,
                         base::span<const char> data) {
  DCHECK(destination_bus);
  if (!destination_bus) {
    return false;
  }

#if BUILDFLAG(ENABLE_FFMPEG)
  auto reader = Reader::Create(data);
  if (!reader) {
    return false;
  }

  const size_t number_of_channels = reader->channels();
  const double file_sample_rate = reader->sample_rate();

  // Apply sanity checks to make sure crazy values aren't coming out of
  // FFmpeg.
  if (!number_of_channels ||
      number_of_channels > static_cast<size_t>(media::limits::kMaxChannels) ||
      file_sample_rate < media::limits::kMinSampleRate ||
      file_sample_rate > media::limits::kMaxSampleRate) {
    return false;
  }

  std::vector<std::unique_ptr<AudioBus>> decoded_audio_packets;
  const size_t number_of_frames = reader->Read(&decoded_audio_packets);
  if (number_of_frames == 0) {
    return false;
  }

  // Allocate and configure the output audio channel data and then
  // copy the decoded data to the destination.
  destination_bus->Initialize(number_of_channels, number_of_frames,
                              file_sample_rate);

  std::vector<base::SpanWriter<float>> dest_channels;
  dest_channels.reserve(number_of_channels);
  for (size_t ch = 0; ch < number_of_channels; ++ch) {
    dest_channels.emplace_back(UNSAFE_TODO(base::span(
        destination_bus->ChannelData(ch), destination_bus->length())));
  }

  // Append all `decoded_audio_packets`, channel per channel.
  for (const auto& packet : decoded_audio_packets) {
    for (size_t ch = 0; ch < number_of_channels; ++ch) {
      dest_channels[ch].Write(packet->channel_span(ch));
    }
  }

  DVLOG(1) << "Decoded file data (unknown duration)-"
           << " data: " << base::ToString(data) << " data size: " << data.size()
           << ", decoded duration: " << (number_of_frames / file_sample_rate)
           << ", number of frames: " << number_of_frames
           << ", estimated frames (if available): "
           << reader->estimated_frames()
           << ", sample rate: " << file_sample_rate
           << ", number of channels: " << number_of_channels;

  return number_of_frames > 0;
#else
  return false;
#endif  // BUILDFLAG(ENABLE_FFMPEG)
}

}  // namespace content
