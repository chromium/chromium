// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/audio_file_reader.h"

#include <stddef.h>

#include <cmath>
#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/media_switches.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/ffmpeg_decoding_loop.h"
#include "media/ffmpeg/scoped_av_packet.h"
#include "media/formats/mpeg/mpeg1_audio_stream_parser.h"

namespace media {

// AAC(M4A) decoding specific constants.
static const int kAACPrimingFrameCount = 2112;
static const int kAACRemainderFrameCount = 519;

AudioFileReader::AudioFileReader(FFmpegURLProtocol* protocol)
    : stream_index_(0),
      protocol_(protocol),
      audio_codec_(AudioCodec::kUnknown),
      channels_(0),
      sample_rate_(0),
      av_sample_format_(0) {}

AudioFileReader::~AudioFileReader() {
  Close();
}

bool AudioFileReader::Open() {
  return OpenDemuxer() && OpenDecoder();
}

bool AudioFileReader::OpenDemuxer() {
  glue_ = std::make_unique<FFmpegGlue>(protocol_);
  AVFormatContext* format_context = glue_->format_context();

  // Open FFmpeg AVFormatContext.
  if (!glue_->OpenContext()) {
    DLOG(WARNING) << "AudioFileReader::Open() : error in avformat_open_input()";
    return false;
  }

  const int result = avformat_find_stream_info(format_context, NULL);
  if (result < 0) {
    DLOG(WARNING)
        << "AudioFileReader::Open() : error in avformat_find_stream_info()";
    return false;
  }

  // Calling avformat_find_stream_info can uncover new streams. We wait till now
  // to find the first audio stream, if any.
  codec_context_.reset();
  bool found_stream = false;
  for (size_t i = 0; i < format_context->nb_streams; ++i) {
    if (format_context->streams[i]->codecpar->codec_type ==
        AVMEDIA_TYPE_AUDIO) {
      stream_index_ = i;
      found_stream = true;
      break;
    }
  }

  if (!found_stream)
    return false;

  // Get the codec context.
  codec_context_ =
      AVStreamToAVCodecContext(format_context->streams[stream_index_]);
  if (!codec_context_)
    return false;

  // Future versions of ffmpeg may copy the allow list from the format context.
  if (base::FeatureList::IsEnabled(kFFmpegAllowLists) &&
      !codec_context_->codec_whitelist) {
    // Note: FFmpeg will try to free this string, so we must duplicate it.
    codec_context_->codec_whitelist =
        av_strdup(FFmpegGlue::GetAllowedAudioDecoders());
  }

  DCHECK_EQ(codec_context_->codec_type, AVMEDIA_TYPE_AUDIO);
  return true;
}

bool AudioFileReader::OpenDecoder() {
  const AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  if (codec) {
    // MP3 decodes to S16P which we don't support, tell it to use S16 instead.
    if (codec_context_->sample_fmt == AV_SAMPLE_FMT_S16P)
      codec_context_->request_sample_fmt = AV_SAMPLE_FMT_S16;

    const int result = avcodec_open2(codec_context_.get(), codec, nullptr);
    if (result < 0) {
      DLOG(WARNING) << "AudioFileReader::Open() : could not open codec -"
                    << " result: " << result;
      return false;
    }

    // Ensure avcodec_open2() respected our format request.
    if (codec_context_->sample_fmt == AV_SAMPLE_FMT_S16P) {
      DLOG(ERROR) << "AudioFileReader::Open() : unable to configure a"
                  << " supported sample format - "
                  << codec_context_->sample_fmt;
      return false;
    }
  } else {
    DLOG(WARNING) << "AudioFileReader::Open() : could not find codec.";
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

  // Store initial values to guard against midstream configuration changes.
  channels_ = codec_context_->ch_layout.nb_channels;
  audio_codec_ = CodecIDToAudioCodec(codec_context_->codec_id);
  sample_rate_ = codec_context_->sample_rate;
  av_sample_format_ = codec_context_->sample_fmt;
  return true;
}

bool AudioFileReader::HasKnownDuration() const {
  return glue_->format_context()->duration != AV_NOPTS_VALUE;
}

void AudioFileReader::Close() {
  codec_context_.reset();
  glue_.reset();
}

int AudioFileReader::Read(
    std::vector<std::unique_ptr<AudioBus>>* decoded_audio_packets,
    int packets_to_read) {
  DCHECK(glue_ && codec_context_)
      << "AudioFileReader::Read() : reader is not opened!";

  FFmpegDecodingLoop decode_loop(codec_context_.get());

  int total_frames = 0;
  auto frame_ready_cb =
      base::BindRepeating(&AudioFileReader::OnNewFrame, base::Unretained(this),
                          &total_frames, decoded_audio_packets);

  auto packet = ScopedAVPacket::Allocate();
  int packets_read = 0;
  while (packets_read++ < packets_to_read && ReadPacket(packet.get())) {
    const auto status = decode_loop.DecodePacket(packet.get(), frame_ready_cb);
    av_packet_unref(packet.get());

    if (status != FFmpegDecodingLoop::DecodeStatus::kOkay)
      break;
  }

  return total_frames;
}

base::TimeDelta AudioFileReader::GetDuration() const {
  const AVRational av_time_base = {1, AV_TIME_BASE};

  DCHECK_NE(glue_->format_context()->duration, AV_NOPTS_VALUE);
  base::CheckedNumeric<int64_t> estimated_duration_us =
      glue_->format_context()->duration;

  if (audio_codec_ == AudioCodec::kAAC) {
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
    if (output_packet->stream_index != stream_index_) {
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

    // MP3 packets may be zero-padded according to ffmpeg, so trim until we
    // have the packet.
    uint8_t* packet_end = output_packet->data + output_packet->size;
    uint8_t* header_start = output_packet->data;
    while (header_start < packet_end && !*header_start) {
      ++header_start;
    }

    if (packet_end - header_start < MPEG1AudioStreamParser::kHeaderSize ||
        !MPEG1AudioStreamParser::ParseHeader(nullptr, nullptr, header_start,
                                             nullptr)) {
      av_packet_unref(output_packet);
      continue;
    }

    return true;
  }
  return false;
}

bool AudioFileReader::OnNewFrame(
    int* total_frames,
    std::vector<std::unique_ptr<AudioBus>>* decoded_audio_packets,
    AVFrame* frame) {
  int frames_read = frame->nb_samples;
  if (frames_read < 0)
    return false;

  const int channels = frame->ch_layout.nb_channels;
  if (frame->sample_rate != sample_rate_ || channels != channels_ ||
      frame->format != av_sample_format_) {
    DLOG(ERROR) << "Unsupported midstream configuration change!"
                << " Sample Rate: " << frame->sample_rate << " vs "
                << sample_rate_ << ", Channels: " << channels << " vs "
                << channels_ << ", Sample Format: " << frame->format << " vs "
                << av_sample_format_;

    // This is an unrecoverable error, so bail out.  We'll return
    // whatever we've decoded up to this point.
    return false;
  }

  // AAC decoding doesn't properly trim the last packet in a stream, so if we
  // have duration information, use it to set the correct length to avoid extra
  // silence from being output. In the case where we are also discarding some
  // portion of the packet (as indicated by a negative pts), we further want to
  // adjust the duration downward by however much exists before zero.
  if (audio_codec_ == AudioCodec::kAAC && frame->duration) {
    const base::TimeDelta pkt_duration = ConvertFromTimeBase(
        glue_->format_context()->streams[stream_index_]->time_base,
        frame->duration + std::min(static_cast<int64_t>(0), frame->pts));
    const base::TimeDelta frame_duration =
        base::Seconds(frames_read / static_cast<double>(sample_rate_));

    if (pkt_duration < frame_duration && pkt_duration.is_positive()) {
      const int new_frames_read =
          base::ClampFloor(frames_read * (pkt_duration / frame_duration));
      DVLOG(2) << "Shrinking AAC frame from " << frames_read << " to "
               << new_frames_read << " based on packet duration.";
      frames_read = new_frames_read;

      // The above process may delete the entire packet.
      if (!frames_read)
        return true;
    }
  }

  // Deinterleave each channel and convert to 32bit floating-point with
  // nominal range -1.0 -> +1.0.  If the output is already in float planar
  // format, just copy it into the AudioBus.
  decoded_audio_packets->emplace_back(AudioBus::Create(channels, frames_read));
  AudioBus* audio_bus = decoded_audio_packets->back().get();

  if (codec_context_->sample_fmt == AV_SAMPLE_FMT_FLT) {
    audio_bus->FromInterleaved<Float32SampleTypeTraits>(
        reinterpret_cast<float*>(frame->data[0]), frames_read);
  } else if (codec_context_->sample_fmt == AV_SAMPLE_FMT_FLTP) {
    for (int ch = 0; ch < audio_bus->channels(); ++ch) {
      memcpy(audio_bus->channel(ch), frame->extended_data[ch],
             sizeof(float) * frames_read);
    }
  } else {
    int bytes_per_sample = av_get_bytes_per_sample(codec_context_->sample_fmt);
    switch (bytes_per_sample) {
      case 1:
        audio_bus->FromInterleaved<UnsignedInt8SampleTypeTraits>(
            reinterpret_cast<const uint8_t*>(frame->data[0]), frames_read);
        break;
      case 2:
        audio_bus->FromInterleaved<SignedInt16SampleTypeTraits>(
            reinterpret_cast<const int16_t*>(frame->data[0]), frames_read);
        break;
      case 4:
        audio_bus->FromInterleaved<SignedInt32SampleTypeTraits>(
            reinterpret_cast<const int32_t*>(frame->data[0]), frames_read);
        break;
      default:
        NOTREACHED_IN_MIGRATION()
            << "Unsupported bytes per sample encountered: " << bytes_per_sample;
        audio_bus->ZeroFrames(frames_read);
    }
  }

  (*total_frames) += frames_read;
  return true;
}

bool AudioFileReader::IsMp3File() {
  return glue_->container() ==
         container_names::MediaContainerName::kContainerMP3;
}

bool AudioFileReader::SeekForTesting(base::TimeDelta seek_time) {
  // Use the AVStream's time_base, since |codec_context_| does not have
  // time_base populated until after OpenDecoder().
  return av_seek_frame(
             glue_->format_context(), stream_index_,
             ConvertToTimeBase(GetAVStreamForTesting()->time_base, seek_time),
             AVSEEK_FLAG_BACKWARD) >= 0;
}

const AVStream* AudioFileReader::GetAVStreamForTesting() const {
  return glue_->format_context()->streams[stream_index_];
}

}  // namespace media
