// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_file_reader.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numeric>
#include <vector>

#include "base/auto_reset.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/time/time.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_switches.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/scoped_av_packet.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/formats/mpeg/mpeg1_audio_stream_parser.h"

#if BUILDFLAG(ENABLE_SYMPHONIA)
#include "media/filters/symphonia_audio_decoder.h"
#endif

namespace media {

namespace {

// AAC(M4A) decoding specific constants.
static const int kAACPrimingFrameCount = 2112;
static const int kAACRemainderFrameCount = 519;

}  // namespace

AudioFileReader::AudioFileReader(FFmpegURLProtocol* protocol)
    : protocol_(protocol) {}

AudioFileReader::~AudioFileReader() {
  Close();
}

bool AudioFileReader::Open() {
  return OpenDemuxer() && OpenDecoder();
}

bool AudioFileReader::OpenDecoder() {
  AudioDecoderConfig config;
  if (!AVCodecContextToAudioDecoderConfig(
          codec_context_.get(), EncryptionScheme::kUnencrypted, &config)) {
    return false;
  }

  // Verify the channel layout is supported by Chrome.  Acts as a sanity check
  // against invalid files.  See http://crbug.com/171962
  if (ChannelLayoutToChromeChannelLayout(
          codec_context_->ch_layout.u.mask,
          codec_context_->ch_layout.nb_channels) ==
      CHANNEL_LAYOUT_UNSUPPORTED) {
    return false;
  }

  // Under a very specific set of circumstances, we can use the
  // SymphoniaAudioDecoder.
#if BUILDFLAG(ENABLE_SYMPHONIA)
  if (base::FeatureList::IsEnabled(kSymphoniaAudioDecoding) &&
      SymphoniaAudioDecoder::IsCodecSupported(config.codec())) {
    decoder_ = std::make_unique<SymphoniaAudioDecoder>(
        nullptr, &media_log_,
        SymphoniaAudioDecoder::ExecutionMode::kSynchronous);
  }
#endif
  // By default, use the FFmpegAudioDecoder.
  if (!decoder_) {
    decoder_ = std::make_unique<FFmpegAudioDecoder>(
        nullptr, &media_log_, FFmpegAudioDecoder::ExecutionMode::kSynchronous);
  }

  std::optional<bool> initialize_status;
  decoder_->Initialize(
      config, nullptr,
      base::BindOnce([](std::optional<bool>* status_out,
                        DecoderStatus s) { *status_out = s.is_ok(); },
                     &initialize_status),
      base::BindRepeating(&AudioFileReader::OnOutput, base::Unretained(this)),
      base::DoNothing());
  config_ = std::move(config);
  return initialize_status.value();
}

bool AudioFileReader::OpenDemuxer() {
  glue_ = std::make_unique<FFmpegGlue>(protocol_);
  AVFormatContext* format_context = glue_->format_context();

  // Open FFmpeg AVFormatContext.
  if (!glue_->OpenContext()) {
    DLOG(WARNING) << "AudioFileReader::Open() : error in avformat_open_input()";
    return false;
  }

  const int result = avformat_find_stream_info(format_context, nullptr);
  if (result < 0) {
    DLOG(WARNING)
        << "AudioFileReader::Open() : error in avformat_find_stream_info()";
    return false;
  }

  // Calling avformat_find_stream_info can uncover new streams. We wait till now
  // to find the first audio stream, if any.
  codec_context_.reset();
  const auto streams = AVFormatContextToSpan(format_context);
  auto stream_result =
      std::ranges::find_if(streams, [](const AVStream* stream) {
        return stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO;
      });
  if (stream_result == streams.end()) {
    return false;
  }
  stream_index_ = std::distance(streams.begin(), stream_result);

  // Get the codec context.
  codec_context_ = AVStreamToAVCodecContext(stream());
  if (!codec_context_) {
    return false;
  }

  return true;
}

bool AudioFileReader::HasKnownDuration() const {
  return glue_->format_context()->duration != AV_NOPTS_VALUE;
}

void AudioFileReader::Close() {
  decoder_.reset();
  codec_context_.reset();
  glue_.reset();
}

size_t AudioFileReader::Read(
    std::vector<std::unique_ptr<AudioBus>>* decoded_audio_packets,
    int packets_to_read) {
  DCHECK(glue_ && decoder_)
      << "AudioFileReader::Read() : reader is not opened!";

  base::AutoReset packet_reset(&decoded_audio_packets_, decoded_audio_packets);

  bool decode_success = true;
  auto packet = ScopedAVPacket::Allocate();
  int packets_read = 0;
  while (packets_read++ < packets_to_read && ReadPacket(packet.get())) {
    decode_success = DecodePacket(packet) && !on_output_error_;
    av_packet_unref(packet.get());
    if (!decode_success) {
      break;
    }
  }

  if (decode_success) {
    std::optional<DecoderStatus> flush_status;
    // Flush the decoder.
    decoder_->Decode(
        DecoderBuffer::CreateEOSBuffer(),
        base::BindOnce([](std::optional<DecoderStatus>& flush_status,
                          DecoderStatus status) { flush_status = status; },
                       std::ref(flush_status)));

    // TODO(crbug.com/438286679): add more robust error checking to this method.
    // Currently, we just ensure that the decode callback is ran, and not its
    // success.
    CHECK(flush_status.has_value());
  }

  return std::accumulate(
      decoded_audio_packets->begin(), decoded_audio_packets->end(), 0,
      [](size_t total, const auto& bus) { return total + bus->frames(); });
}

void AudioFileReader::OnOutput(scoped_refptr<AudioBuffer> buffer) {
  // Ensure that there are no unsupported midstream configuration changes.
  if (buffer->sample_rate() != config_->samples_per_second() ||
      buffer->channel_count() != config_->channels() ||
      buffer->channel_layout() != config_->channel_layout()) {
    DLOG(ERROR) << "Unsupported midstream configuration change! sample_rate="
                << buffer->sample_rate() << " (expected "
                << config_->samples_per_second()
                << "), channel_layout=" << buffer->channel_layout()
                << " (expected " << config_->channel_layout()
                << "), channels=" << buffer->channel_count() << " (expected "
                << config_->channels() << "\")";

    // This is an unrecoverable error, so bail out.  We'll return
    // whatever we've decoded up to this point.
    on_output_error_ = true;
    return;
  }

  // Drop buffers that are entirely before the zero start time.
  if (buffer->timestamp() + buffer->duration() < base::TimeDelta()) {
    return;
  }

  // Trim buffers that start before the zero start time.
  if (buffer->timestamp() < base::TimeDelta()) {
    const base::TimeDelta trim_time = base::TimeDelta() - buffer->timestamp();
    const int frames_to_trim =
        AudioTimestampHelper::TimeToFrames(trim_time, buffer->sample_rate());
    buffer->TrimStart(frames_to_trim);
    buffer->set_timestamp(base::TimeDelta());
  }

  // If the entire buffer was trimmed, drop it.
  if (!buffer->frame_count()) {
    return;
  }

  // AAC decoding doesn't properly trim the last packet in a stream, so if we
  // have duration information, use it to set the correct length to avoid extra
  // silence from being output. In the case where we are also discarding some
  // portion of the packet (as indicated by a negative pts), we further want to
  // adjust the duration downward by however much exists before zero.
  if (config_->codec() == AudioCodec::kAAC &&
      last_packet_duration_ > base::TimeDelta()) {
    int frames_read = buffer->frame_count();

    const base::TimeDelta frame_duration =
        AudioTimestampHelper::FramesToTime(frames_read, sample_rate());

    if (last_packet_duration_ < frame_duration) {
      const int new_frames_read = base::ClampFloor(
          frames_read * (last_packet_duration_ / frame_duration));
      DVLOG(2) << "Shrinking AAC frame from " << frames_read << " to "
               << new_frames_read << " based on packet duration.";
      frames_read = new_frames_read;

      // The above process may delete the entire packet.
      if (!frames_read) {
        return;
      }

      // Otherwise, trim the empty frames at the end.
      buffer->TrimEnd(buffer->frame_count() - frames_read);
    }
  }

  if (decoded_audio_packets_) {
    decoded_audio_packets_->push_back(
        AudioBuffer::WrapOrCopyToAudioBus(std::move(buffer)));
  }
}

base::TimeDelta AudioFileReader::GetDuration() const {
  const AVRational av_time_base = {1, AV_TIME_BASE};

  DCHECK_NE(glue_->format_context()->duration, AV_NOPTS_VALUE);
  base::CheckedNumeric<int64_t> estimated_duration_us =
      glue_->format_context()->duration;

  if (config_->codec() == AudioCodec::kAAC) {
    // For certain AAC-encoded files, FFMPEG's estimated frame count might not
    // be sufficient to capture the entire audio content that we want. This is
    // especially noticeable for short files (< 10ms) resulting in silence
    // throughout the decoded buffer. Thus we add the priming frames and the
    // remainder frames to the estimation.
    // (See: crbug.com/513178)
    estimated_duration_us += ceil(
        1000000.0 *
        static_cast<double>(kAACPrimingFrameCount + kAACRemainderFrameCount) /
        sample_rate());
  } else {
    // Add one microsecond to avoid rounding-down errors which can occur when
    // |duration| has been calculated from an exact number of sample-frames.
    // One microsecond is much less than the time of a single sample-frame
    // at any real-world sample-rate.
    estimated_duration_us += 1;
  }

  return ConvertFromTimeBase(av_time_base, estimated_duration_us.ValueOrDie());
}

int AudioFileReader::GetNumberOfFrames() const {
  return base::ClampCeil(GetDuration().InSecondsF() * sample_rate());
}

bool AudioFileReader::OpenDemuxerForTesting() {
  return OpenDemuxer();
}

bool AudioFileReader::ReadPacketForTesting(AVPacket* output_packet) {
  return ReadPacket(output_packet);
}

bool AudioFileReader::ReadPacket(AVPacket* output_packet) {
  while (av_read_frame(glue_->format_context(), output_packet) >= 0) {
    // Skip packets from other streams.
    if (output_packet->stream_index != *stream_index_) {
      av_packet_unref(output_packet);
      continue;
    }

    if (!IsMp3File()) {
      return true;
    }

    // FFmpeg may return garbage packets for MP3 stream containers, so we need
    // to drop these to avoid decoder errors. The ffmpeg team maintains that
    // this behavior isn't ideal, but have asked for a significant refactoring
    // of the AVParser infrastructure to fix this, which is overkill for now.
    // See http://crbug.com/794782.
    base::span<const uint8_t> packet_data = AVPacketData(*output_packet);

    // MP3 packets may be zero-padded according to ffmpeg, so trim until we
    // have the packet.
    const auto first_valid = std::ranges::find_if(packet_data, std::identity{});
    const size_t trim_count = std::distance(packet_data.begin(), first_valid);
    packet_data = packet_data.subspan(trim_count);

    if (packet_data.size() < MPEG1AudioStreamParser::kHeaderSize ||
        !MPEG1AudioStreamParser::ParseHeader(nullptr, nullptr, packet_data,
                                             nullptr)) {
      av_packet_unref(output_packet);
      continue;
    }

    return true;
  }
  return false;
}

bool AudioFileReader::DecodePacket(const ScopedAVPacket& packet) {
  auto buffer = DecoderBuffer::CopyFrom(AVPacketData(*packet));

  base::TimeDelta stream_timestamp =
      ConvertStreamTimestamp(stream()->time_base, packet->pts);
  if (stream_timestamp == kNoTimestamp ||
      stream_timestamp == kInfiniteDuration) {
    stream_timestamp = last_packet_timestamp_;
  }
  buffer->set_timestamp(stream_timestamp);

  buffer->set_duration(
      ConvertStreamTimestamp(stream()->time_base, packet->duration));

  auto discard_padding = GetDiscardPaddingFromAVPacket(
      packet.get(), config_->samples_per_second());
  if (discard_padding) {
    // We only allow front discard padding on the first packet.
    if (last_packet_duration_ != kNoTimestamp &&
        discard_padding->first != kInfiniteDuration) {
      discard_padding->first = base::TimeDelta();
    }
    buffer->set_discard_padding(*discard_padding);
  }

  if (packet->flags & AV_PKT_FLAG_KEY) {
    buffer->set_is_key_frame(true);
  }

  last_packet_duration_ = buffer->duration();
  last_packet_timestamp_ = buffer->timestamp();

  std::optional<DecoderStatus> decode_status;
  decoder_->Decode(
      buffer,
      base::BindOnce([](std::optional<DecoderStatus>& decode_status,
                        DecoderStatus status) { decode_status = status; },
                     std::ref(decode_status)));
  return decode_status->is_ok();
}

bool AudioFileReader::IsMp3File() {
  return glue_->container() ==
         container_names::MediaContainerName::kContainerMP3;
}

const AVStream* AudioFileReader::stream() const {
  return AVFormatContextToSpan(glue_->format_context())[*stream_index_];
}

}  // namespace media
