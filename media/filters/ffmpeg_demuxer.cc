// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/ffmpeg_demuxer.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/base/decrypt_config.h"
#include "media/base/demuxer.h"
#include "media/base/demuxer_memory_limit.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/media_tracks.h"
#include "media/base/media_types.h"
#include "media/base/sample_rates.h"
#include "media/base/supported_types.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_codecs.h"
#include "media/base/webvtt_util.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_aac_bitstream_converter.h"
#include "media/filters/ffmpeg_bitstream_converter.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/ffmpeg_h264_to_annex_b_bitstream_converter.h"
#include "media/formats/mpeg/mpeg1_audio_stream_parser.h"
#include "media/formats/webm/webm_crypto_helpers.h"
#include "media/media_buildflags.h"
#include "third_party/ffmpeg/ffmpeg_features.h"
#include "third_party/ffmpeg/libavcodec/packet.h"

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
#include "media/filters/ffmpeg_h265_to_annex_b_bitstream_converter.h"
#endif

namespace media {

namespace {

constexpr int64_t kInvalidPTSMarker = static_cast<int64_t>(0x8000000000000000);

void SetAVStreamDiscard(AVStream* stream, AVDiscard discard) {
  DCHECK(stream);
  stream->discard = discard;
}

int AVSeekFrame(AVFormatContext* s, int stream_index, int64_t timestamp) {
  // Seek to a timestamp <= to the desired timestamp.
  int result = av_seek_frame(s, stream_index, timestamp, AVSEEK_FLAG_BACKWARD);
  if (result >= 0) {
    return result;
  }

  // Seek to the nearest keyframe, wherever that may be.
  return av_seek_frame(s, stream_index, timestamp, 0);
}

bool IsStreamEnabled(container_names::MediaContainerName container,
                     AVStream* stream) {
  // Track enabled state is only handled for MP4 files.
  if (container != container_names::MediaContainerName::kContainerMOV) {
    return true;
  }
  // The mov demuxer translates MOV_TKHD_FLAG_ENABLED to AV_DISPOSITION_DEFAULT.
  return stream->disposition & AV_DISPOSITION_DEFAULT;
}

}  // namespace

static base::Time ExtractTimelineOffset(
    container_names::MediaContainerName container,
    const AVFormatContext* format_context) {
  if (container == container_names::MediaContainerName::kContainerWEBM) {
    const AVDictionaryEntry* entry =
        av_dict_get(format_context->metadata, "creation_time", nullptr, 0);

    base::Time timeline_offset;

    // FFmpegDemuxerTests assume base::Time::FromUTCString() is used here.
    if (entry != nullptr && entry->value != nullptr &&
        base::Time::FromUTCString(entry->value, &timeline_offset)) {
      return timeline_offset;
    }
  }

  return base::Time();
}

static base::TimeDelta FramesToTimeDelta(int frames, double sample_rate) {
  return base::Microseconds(frames * base::Time::kMicrosecondsPerSecond /
                            sample_rate);
}

static base::TimeDelta ExtractStartTime(AVStream* stream) {
  // The default start time is zero.
  base::TimeDelta start_time;

  // First try to use  the |start_time| value as is.
  if (stream->start_time != kNoFFmpegTimestamp)
    start_time = ConvertFromTimeBase(stream->time_base, stream->start_time);

  // Next try to use the first DTS value, for codecs where we know PTS == DTS
  // (excludes all H26x codecs). The start time must be returned in PTS.
  if (av_stream_get_first_dts(stream) != kNoFFmpegTimestamp &&
      stream->codecpar->codec_id != AV_CODEC_ID_HEVC &&
      stream->codecpar->codec_id != AV_CODEC_ID_H264 &&
      stream->codecpar->codec_id != AV_CODEC_ID_MPEG4) {
    const base::TimeDelta first_pts =
        ConvertFromTimeBase(stream->time_base, av_stream_get_first_dts(stream));
    if (first_pts < start_time)
      start_time = first_pts;
  }

  return start_time;
}

// Record audio decoder config UMA stats corresponding to a src= playback.
static void RecordAudioCodecStats(const AudioDecoderConfig& audio_config) {
  base::UmaHistogramEnumeration("Media.AudioCodec", audio_config.codec());
}

// Record video decoder config UMA stats corresponding to a src= playback.
static void RecordVideoCodecStats(container_names::MediaContainerName container,
                                  const VideoDecoderConfig& video_config,
                                  AVColorRange color_range,
                                  MediaLog* media_log) {
  // TODO(xhwang): Fix these misleading metric names. They should be something
  // like "Media.SRC.Xxxx". See http://crbug.com/716183.
  base::UmaHistogramEnumeration("Media.VideoCodec", video_config.codec());
  if (container == container_names::MediaContainerName::kContainerMOV) {
    base::UmaHistogramEnumeration("Media.SRC.VideoCodec.MP4",
                                  video_config.codec());
  } else if (container == container_names::MediaContainerName::kContainerWEBM) {
    base::UmaHistogramEnumeration("Media.SRC.VideoCodec.WebM",
                                  video_config.codec());
  }
}

static const char kCodecNone[] = "none";

static const char* GetCodecName(enum AVCodecID id) {
  const AVCodecDescriptor* codec_descriptor = avcodec_descriptor_get(id);
  // If the codec name can't be determined, return none for tracking.
  return codec_descriptor ? codec_descriptor->name : kCodecNone;
}

static base::Value GetTimeValue(base::TimeDelta value) {
  if (value == kInfiniteDuration)
    return base::Value("kInfiniteDuration");
  if (value == kNoTimestamp)
    return base::Value("kNoTimestamp");
  return base::Value(value.InSecondsF());
}

template <>
struct MediaLogPropertyTypeSupport<MediaLogProperty::kMaxDuration,
                                   base::TimeDelta> {
  static base::Value Convert(base::TimeDelta t) { return GetTimeValue(t); }
};

template <>
struct MediaLogPropertyTypeSupport<MediaLogProperty::kStartTime,
                                   base::TimeDelta> {
  static base::Value Convert(base::TimeDelta t) { return GetTimeValue(t); }
};

static int ReadFrameAndDiscardEmpty(AVFormatContext* context,
                                    AVPacket* packet) {
  // Skip empty packets in a tight loop to avoid timing out fuzzers.
  int result;
  bool drop_packet;
  do {
    result = av_read_frame(context, packet);
    drop_packet = (!packet->data || !packet->size) && result >= 0;
    if (drop_packet) {
      av_packet_unref(packet);
      DLOG(WARNING) << "Dropping empty packet, size: " << packet->size
                    << ", data: " << static_cast<void*>(packet->data);
    }
  } while (drop_packet);

  return result;
}

std::unique_ptr<FFmpegDemuxerStream> FFmpegDemuxerStream::Create(
    FFmpegDemuxer* demuxer,
    AVStream* stream,
    MediaLog* media_log) {
  if (!demuxer || !stream)
    return nullptr;

  std::unique_ptr<FFmpegDemuxerStream> demuxer_stream;
  std::unique_ptr<AudioDecoderConfig> audio_config;
  std::unique_ptr<VideoDecoderConfig> video_config;

  if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
    audio_config = std::make_unique<AudioDecoderConfig>();

    // TODO(chcunningham): Change AVStreamToAudioDecoderConfig to check
    // IsValidConfig internally and return a null scoped_ptr if not valid.
    if (!AVStreamToAudioDecoderConfig(stream, audio_config.get()) ||
        !audio_config->IsValidConfig() ||
        !IsSupportedAudioType(AudioType::FromDecoderConfig(*audio_config))) {
      MEDIA_LOG(DEBUG, media_log) << "Warning, FFmpegDemuxer failed to create "
                                     "a valid/supported audio decoder "
                                     "configuration from muxed stream, config:"
                                  << audio_config->AsHumanReadableString();
      return nullptr;
    }

    MEDIA_LOG(INFO, media_log) << "FFmpegDemuxer: created audio stream, config "
                               << audio_config->AsHumanReadableString();
  } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
    video_config = std::make_unique<VideoDecoderConfig>();

    // TODO(chcunningham): Change AVStreamToVideoDecoderConfig to check
    // IsValidConfig internally and return a null scoped_ptr if not valid.
    if (!AVStreamToVideoDecoderConfig(stream, video_config.get()) ||
        !video_config->IsValidConfig() ||
        !IsSupportedVideoType(VideoType::FromDecoderConfig(*video_config))) {
      MEDIA_LOG(DEBUG, media_log) << "Warning, FFmpegDemuxer failed to create "
                                     "a valid/supported video decoder "
                                     "configuration from muxed stream, config:"
                                  << video_config->AsHumanReadableString();
      return nullptr;
    }

    MEDIA_LOG(INFO, media_log) << "FFmpegDemuxer: created video stream, config "
                               << video_config->AsHumanReadableString();
  }

  return base::WrapUnique(
      new FFmpegDemuxerStream(demuxer, stream, std::move(audio_config),
                              std::move(video_config), media_log));
}

static void UnmarkEndOfStreamAndClearError(AVFormatContext* format_context) {
  format_context->pb->eof_reached = 0;
  format_context->pb->error = 0;
}

//
// FFmpegDemuxerStream
//
FFmpegDemuxerStream::FFmpegDemuxerStream(
    FFmpegDemuxer* demuxer,
    AVStream* stream,
    std::unique_ptr<AudioDecoderConfig> audio_config,
    std::unique_ptr<VideoDecoderConfig> video_config,
    MediaLog* media_log)
    : demuxer_(demuxer),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      stream_(stream),
      start_time_(kNoTimestamp),
      audio_config_(audio_config.release()),
      video_config_(video_config.release()),
      media_log_(media_log),
      end_of_stream_(false),
      last_packet_timestamp_(kNoTimestamp),
      last_packet_duration_(kNoTimestamp),
      is_enabled_(true),
      waiting_for_keyframe_(false),
      aborted_(false),
      fixup_negative_timestamps_(false),
      fixup_chained_ogg_(false),
      num_discarded_packet_warnings_(0),
      last_packet_pos_(AV_NOPTS_VALUE),
      last_packet_dts_(AV_NOPTS_VALUE) {
  DCHECK(demuxer_);

  bool is_encrypted = false;

  // Determine our media format.
  switch (stream->codecpar->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
      DCHECK(audio_config_.get() && !video_config_.get());
      type_ = AUDIO;
      is_encrypted = audio_config_->is_encrypted();
      break;
    case AVMEDIA_TYPE_VIDEO:
      DCHECK(video_config_.get() && !audio_config_.get());
      type_ = VIDEO;
      is_encrypted = video_config_->is_encrypted();
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  // Calculate the duration.
  duration_ = ConvertStreamTimestamp(stream->time_base, stream->duration);

  if (is_encrypted) {
    AVDictionaryEntry* key =
        av_dict_get(stream->metadata, "enc_key_id", nullptr, 0);
    DCHECK(key);
    DCHECK(key->value);
    if (!key || !key->value)
      return;
    std::string_view base64_key_id(key->value);
    std::string enc_key_id;
    base::Base64Decode(base64_key_id, &enc_key_id);
    DCHECK(!enc_key_id.empty());
    if (enc_key_id.empty())
      return;

    encryption_key_id_.assign(enc_key_id);
    demuxer_->OnEncryptedMediaInitData(EmeInitDataType::WEBM, enc_key_id);
  }
}

FFmpegDemuxerStream::~FFmpegDemuxerStream() {
  DCHECK(!demuxer_);
  DCHECK(!read_cb_);
  DCHECK(buffer_queue_.IsEmpty());
}

void FFmpegDemuxerStream::EnqueuePacket(ScopedAVPacket packet) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(packet->size);
  DCHECK(packet->data);

  const bool is_audio = type() == AUDIO;

  // dts == pts when dts is not present.
  int64_t packet_dts =
      packet->dts == AV_NOPTS_VALUE ? packet->pts : packet->dts;

  // Chained ogg files have non-monotonically increasing position and time stamp
  // values, which prevents us from using them to determine if a packet should
  // be dropped. Since chained ogg is only allowed on single track audio only
  // opus/vorbis media, and dropping packets is only necessary for multi-track
  // video-and-audio streams, we can just disable dropping when we detect
  // chained ogg.
  // For similar reasons, we only want to allow packet drops for audio streams;
  // video frame dropping is handled by the renderer when correcting for a/v
  // sync.
  if (is_audio && !fixup_chained_ogg_ && last_packet_pos_ != AV_NOPTS_VALUE) {
    // Some containers have unknown position...
    if (packet->pos == -1)
      packet->pos = last_packet_pos_;

    if (packet->pos < last_packet_pos_) {
      DVLOG(3) << "Dropped packet with out of order position (" << packet->pos
               << " < " << last_packet_pos_ << ")";
      return;
    }
    if (last_packet_dts_ != AV_NOPTS_VALUE && packet->pos == last_packet_pos_ &&
        packet_dts <= last_packet_dts_) {
      DVLOG(3) << "Dropped packet with out of order display timestamp ("
               << packet_dts << " < " << last_packet_dts_ << ")";
      return;
    }
  }

  if (!demuxer_ || end_of_stream_) {
    NOTREACHED_IN_MIGRATION()
        << "Attempted to enqueue packet on a stopped stream";
    return;
  }

  last_packet_pos_ = packet->pos;
  last_packet_dts_ = packet_dts;

  if (waiting_for_keyframe_) {
    if (packet->flags & AV_PKT_FLAG_KEY) {
      waiting_for_keyframe_ = false;
    } else {
      DVLOG(1) << "Dropped non-keyframe pts=" << packet->pts;
      return;
    }
  }

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  // Convert the packet if there is a bitstream filter.
  if (bitstream_converter_ &&
      !bitstream_converter_->ConvertPacket(packet.get())) {
    DVLOG(1) << "Format conversion failed.";
  }
#endif

  scoped_refptr<DecoderBuffer> buffer;

    size_t side_data_size = 0;
    uint8_t* side_data = av_packet_get_side_data(
        packet.get(), AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL, &side_data_size);

    std::unique_ptr<DecryptConfig> decrypt_config;
    int data_offset = 0;
    if ((type() == DemuxerStream::AUDIO && audio_config_->is_encrypted()) ||
        (type() == DemuxerStream::VIDEO && video_config_->is_encrypted())) {
      if (!WebMCreateDecryptConfig(
              packet->data, packet->size,
              reinterpret_cast<const uint8_t*>(encryption_key_id_.data()),
              encryption_key_id_.size(), &decrypt_config, &data_offset)) {
        MEDIA_LOG(ERROR, media_log_) << "Creation of DecryptConfig failed.";
      }
    }

    // FFmpeg may return garbage packets for MP3 stream containers, so we need
    // to drop these to avoid decoder errors. The ffmpeg team maintains that
    // this behavior isn't ideal, but have asked for a significant refactoring
    // of the AVParser infrastructure to fix this, which is overkill for now.
    // See http://crbug.com/794782.
    //
    // This behavior may also occur with ADTS streams, but is rarer in practice
    // because ffmpeg's ADTS demuxer does more validation on the packets, so
    // when invalid data is received, av_read_frame() fails and playback ends.
    if (is_audio && demuxer_->container() ==
                        container_names::MediaContainerName::kContainerMP3) {
      DCHECK(!data_offset);  // Only set for containers supporting encryption...

      // MP3 packets may be zero-padded according to ffmpeg, so trim until we
      // have the packet; adjust |data_offset| too so this work isn't repeated.
      uint8_t* packet_end = packet->data + packet->size;
      uint8_t* header_start = packet->data;
      while (header_start < packet_end && !*header_start) {
        ++header_start;
        ++data_offset;
      }

      if (packet_end - header_start < MPEG1AudioStreamParser::kHeaderSize ||
          !MPEG1AudioStreamParser::ParseHeader(nullptr, nullptr, header_start,
                                               nullptr)) {
        LIMITED_MEDIA_LOG(INFO, media_log_, num_discarded_packet_warnings_, 5)
            << "Discarding invalid MP3 packet, ts: "
            << ConvertStreamTimestamp(stream_->time_base, packet->pts)
            << ", duration: "
            << ConvertStreamTimestamp(stream_->time_base, packet->duration);
        return;
      }
    }

    // If a packet is returned by FFmpeg's av_parser_parse2() the packet will
    // reference inner memory of FFmpeg.  As such we should transfer the packet
    // into memory we control.
    buffer =
        DecoderBuffer::CopyFrom(AVPacketData(*packet).subspan(data_offset));
    if (side_data_size > 0) {
      buffer->WritableSideData().alpha_data.assign(side_data,
                                                   side_data + side_data_size);
    }

    size_t skip_samples_size = 0;
    const uint32_t* skip_samples_ptr =
        reinterpret_cast<const uint32_t*>(av_packet_get_side_data(
            packet.get(), AV_PKT_DATA_SKIP_SAMPLES, &skip_samples_size));
    const int kSkipSamplesValidSize = 10;
    const int kSkipEndSamplesOffset = 1;
    if (skip_samples_size >= kSkipSamplesValidSize) {
      // Because FFmpeg rolls codec delay and skip samples into one we can only
      // allow front discard padding on the first buffer.  Otherwise the discard
      // helper can't figure out which data to discard.  See AudioDiscardHelper.
      //
      // NOTE: Large values may end up as negative here, but negatives are
      // discarded below.
      auto discard_front_samples = static_cast<int>(*skip_samples_ptr);
      if (last_packet_timestamp_ != kNoTimestamp && discard_front_samples) {
        DLOG(ERROR) << "Skip samples are only allowed for the first packet.";
        discard_front_samples = 0;
      }

      if (discard_front_samples < 0) {
        // See https://crbug.com/1189939 and https://trac.ffmpeg.org/ticket/9622
        DLOG(ERROR) << "Negative skip samples are not allowed.";
        discard_front_samples = 0;
      }

      // NOTE: Large values may end up as negative here, which could lead to
      // a negative timestamp. It's not clear if this is intentional.
      const auto discard_end_samples =
          static_cast<int>(*(skip_samples_ptr + kSkipEndSamplesOffset));

      if (discard_front_samples || discard_end_samples) {
        DCHECK(is_audio);
        const int samples_per_second =
            audio_decoder_config().samples_per_second();
        buffer->set_discard_padding(std::make_pair(
            FramesToTimeDelta(discard_front_samples, samples_per_second),
            FramesToTimeDelta(discard_end_samples, samples_per_second)));
      }
    }

    if (decrypt_config)
      buffer->set_decrypt_config(std::move(decrypt_config));

  if (packet->duration >= 0) {
    buffer->set_duration(
        ConvertStreamTimestamp(stream_->time_base, packet->duration));
  } else {
    // TODO(wolenetz): Remove when FFmpeg stops returning negative durations.
    // https://crbug.com/394418
    DVLOG(1) << "FFmpeg returned a buffer with a negative duration! "
             << packet->duration;
    buffer->set_duration(kNoTimestamp);
  }

  // Note: If pts is kNoFFmpegTimestamp, stream_timestamp will be kNoTimestamp.
  const base::TimeDelta stream_timestamp =
      ConvertStreamTimestamp(stream_->time_base, packet->pts);

  if (stream_timestamp == kNoTimestamp ||
      stream_timestamp == kInfiniteDuration) {
    MEDIA_LOG(ERROR, media_log_) << "FFmpegDemuxer: PTS is not defined";
    demuxer_->NotifyDemuxerError(DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }

  // If this file has negative timestamps don't rebase any other stream types
  // against the negative starting time.
  base::TimeDelta start_time = demuxer_->start_time();
  if (!is_audio && start_time.is_negative()) {
    start_time = base::TimeDelta();
  }

  // Don't rebase timestamps for positive start times, the HTML Media Spec
  // details this in section "4.8.10.6 Offsets into the media resource." We
  // will still need to rebase timestamps before seeking with FFmpeg though.
  if (start_time.is_positive())
    start_time = base::TimeDelta();

  buffer->set_timestamp(stream_timestamp - start_time);

  // If the packet is marked for complete discard and it doesn't already have
  // any discard padding set, mark the DecoderBuffer for complete discard. We
  // don't want to overwrite any existing discard padding since the discard
  // padding may refer to frames beyond this packet.
  if (packet->flags & AV_PKT_FLAG_DISCARD &&
      buffer->discard_padding() == DecoderBuffer::DiscardPadding()) {
    buffer->set_discard_padding(
        std::make_pair(kInfiniteDuration, base::TimeDelta()));
  }

  if (is_audio) {
    // Fixup negative timestamps where the before-zero portion is completely
    // discarded after decoding.
    if (buffer->timestamp().is_negative()) {
      // Discard padding may also remove samples after zero.
      auto fixed_ts = buffer->discard_padding().first + buffer->timestamp();

      // Allow for rounding error in the discard padding calculations.
      if (fixed_ts == base::Microseconds(-1)) {
        fixed_ts = base::TimeDelta();
      }

      if (fixed_ts >= base::TimeDelta()) {
        buffer->set_timestamp(fixed_ts);
      }
    }

    // Only allow negative timestamps past if we know they'll be fixed up by the
    // code paths below; otherwise they should be treated as a parse error.
    if ((!fixup_chained_ogg_ || last_packet_timestamp_ == kNoTimestamp) &&
        buffer->timestamp().is_negative()) {
      MEDIA_LOG(ERROR, media_log_)
          << "FFmpegDemuxer: unfixable negative timestamp.";
      demuxer_->NotifyDemuxerError(DEMUXER_ERROR_COULD_NOT_PARSE);
      return;
    }

    // If enabled, and no codec delay is present, mark audio packets with
    // negative timestamps for post-decode discard. If codec delay is present,
    // discard is handled by the decoder using that value.
    if (fixup_negative_timestamps_ && stream_timestamp.is_negative() &&
        buffer->duration() != kNoTimestamp &&
        !audio_decoder_config().codec_delay()) {
      if ((stream_timestamp + buffer->duration()).is_negative()) {
        DCHECK_EQ(buffer->discard_padding().second, base::TimeDelta());

        // Discard the entire packet if it's entirely before zero, but don't
        // override the discard padding if it refers to frames beyond this
        // packet.
        if (buffer->discard_padding().first <= buffer->duration()) {
          buffer->set_discard_padding(
              std::make_pair(kInfiniteDuration, base::TimeDelta()));
        }
      } else {
        // Only discard part of the frame if it overlaps zero.
        buffer->set_discard_padding(std::make_pair(
            std::max(-stream_timestamp, buffer->discard_padding().first),
            buffer->discard_padding().second));
      }
    }
  }

  if (last_packet_timestamp_ != kNoTimestamp) {
    // FFmpeg doesn't support chained ogg correctly.  Instead of guaranteeing
    // continuity across links in the chain it uses the timestamp information
    // from each link directly.  Doing so can lead to timestamps which appear to
    // go backwards in time.
    //
    // If the new link starts with a negative timestamp or a timestamp less than
    // the original (positive) |start_time|, we will get a negative timestamp
    // here.
    //
    // Fixing chained ogg is non-trivial, so for now just reuse the last good
    // timestamp.  The decoder will rewrite the timestamps to be sample accurate
    // later.  See http://crbug.com/396864.
    //
    // Note: This will not work with codecs that have out of order frames like
    // H.264 with b-frames, but luckily you can't put those in ogg files...
    if (fixup_chained_ogg_ && buffer->timestamp() < last_packet_timestamp_) {
      buffer->set_timestamp(last_packet_timestamp_ +
                            (last_packet_duration_ != kNoTimestamp
                                 ? last_packet_duration_
                                 : base::Microseconds(1)));
    }

    if (is_audio) {
      // The demuxer should always output positive timestamps.
      DCHECK_GE(buffer->timestamp(), base::TimeDelta());
    }

    if (last_packet_timestamp_ < buffer->timestamp()) {
      buffered_ranges_.Add(last_packet_timestamp_, buffer->timestamp());
      demuxer_->NotifyBufferingChanged();
    }
  }

  if (packet->flags & AV_PKT_FLAG_KEY)
    buffer->set_is_key_frame(true);

  // One last sanity check on the packet timestamps in case any of the above
  // calculations have pushed the values to the limits.
  if (buffer->timestamp() == kNoTimestamp ||
      buffer->timestamp() == kInfiniteDuration) {
    MEDIA_LOG(ERROR, media_log_) << "FFmpegDemuxer: PTS is not defined";
    demuxer_->NotifyDemuxerError(DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }

  last_packet_timestamp_ = buffer->timestamp();
  last_packet_duration_ = buffer->duration();

  const base::TimeDelta new_duration = last_packet_timestamp_;
  if (new_duration > duration_ || duration_ == kNoTimestamp)
    duration_ = new_duration;

  buffer_queue_.Push(std::move(buffer));
  SatisfyPendingRead();
}

void FFmpegDemuxerStream::SetEndOfStream() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  end_of_stream_ = true;
  SatisfyPendingRead();
}

void FFmpegDemuxerStream::FlushBuffers(bool preserve_packet_position) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(preserve_packet_position || !read_cb_)
      << "There should be no pending read";

  // H264 and AAC require that we resend the header after flush.
  // Reset bitstream for converter to do so.
  // This is related to chromium issue 140371 (http://crbug.com/140371).
  ResetBitstreamConverter();

  if (!preserve_packet_position) {
    last_packet_pos_ = AV_NOPTS_VALUE;
    last_packet_dts_ = AV_NOPTS_VALUE;
  }

  buffer_queue_.Clear();
  end_of_stream_ = false;
  last_packet_timestamp_ = kNoTimestamp;
  last_packet_duration_ = kNoTimestamp;
  aborted_ = false;
}

void FFmpegDemuxerStream::Abort() {
  aborted_ = true;
  if (read_cb_)
    std::move(read_cb_).Run(DemuxerStream::kAborted, {});
}

void FFmpegDemuxerStream::Stop() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  buffer_queue_.Clear();
  demuxer_ = nullptr;
  stream_ = nullptr;
  end_of_stream_ = true;
  if (read_cb_) {
    std::move(read_cb_).Run(DemuxerStream::kOk,
                            {DecoderBuffer::CreateEOSBuffer()});
  }
}

DemuxerStream::Type FFmpegDemuxerStream::type() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return type_;
}

StreamLiveness FFmpegDemuxerStream::liveness() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return liveness_;
}

void FFmpegDemuxerStream::Read(uint32_t count, ReadCB read_cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  CHECK(!read_cb_) << "Overlapping reads are not supported";
  read_cb_ = base::BindPostTaskToCurrentDefault(std::move(read_cb));
  requested_buffer_count_ = static_cast<size_t>(count);
  DVLOG(3) << __func__
           << " requested_buffer_count_ = " << requested_buffer_count_;
  // Don't accept any additional reads if we've been told to stop.
  // The |demuxer_| may have been destroyed in the pipeline thread.
  //
  // TODO(scherkus): it would be cleaner to reply with an error message.
  if (!demuxer_) {
    std::move(read_cb_).Run(DemuxerStream::kOk,
                            {DecoderBuffer::CreateEOSBuffer()});
    return;
  }

  if (!is_enabled_) {
    DVLOG(1) << "Read from disabled stream, returning EOS";
    std::move(read_cb_).Run(kOk, {DecoderBuffer::CreateEOSBuffer()});
    return;
  }

  if (aborted_) {
    std::move(read_cb_).Run(kAborted, {});
    return;
  }

  SatisfyPendingRead();
}

void FFmpegDemuxerStream::EnableBitstreamConverter() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  InitBitstreamConverter();
#else
  DLOG(ERROR) << "Proprietary codecs not enabled and stream requires bitstream "
                 "conversion. Playback will likely fail.";
#endif
}

void FFmpegDemuxerStream::ResetBitstreamConverter() {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (bitstream_converter_)
    InitBitstreamConverter();
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
}

void FFmpegDemuxerStream::InitBitstreamConverter() {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  switch (stream_->codecpar->codec_id) {
    case AV_CODEC_ID_H264:
      // Clear |extra_data| so that future (fallback) decoders will know that
      // conversion is forcibly enabled on this stream.
      //
      // TODO(sandersd): Ideally we would convert |extra_data| to concatenated
      // SPS/PPS data, but it's too late to be useful because Initialize() was
      // already called on GpuVideoDecoder, which is the only path that would
      // consume that data.
      if (video_config_)
        video_config_->SetExtraData(std::vector<uint8_t>());
      bitstream_converter_ =
          std::make_unique<FFmpegH264ToAnnexBBitstreamConverter>(
              stream_->codecpar);
      break;
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case AV_CODEC_ID_HEVC:
      bitstream_converter_ =
          std::make_unique<FFmpegH265ToAnnexBBitstreamConverter>(
              stream_->codecpar);
      break;
#endif
    case AV_CODEC_ID_AAC:
      // FFmpeg doesn't understand xHE-AAC profiles yet, which can't be put in
      // ADTS anyways, so skip bitstream conversion when the profile is
      // unknown.
      if (audio_config_->profile() != AudioCodecProfile::kXHE_AAC) {
        bitstream_converter_ =
            std::make_unique<FFmpegAACBitstreamConverter>(stream_->codecpar);
      }
      break;
    default:
      break;
  }
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
}

bool FFmpegDemuxerStream::SupportsConfigChanges() { return false; }

AudioDecoderConfig FFmpegDemuxerStream::audio_decoder_config() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(type_, AUDIO);
  DCHECK(audio_config_.get());
  return *audio_config_;
}

VideoDecoderConfig FFmpegDemuxerStream::video_decoder_config() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(type_, VIDEO);
  DCHECK(video_config_.get());
  return *video_config_;
}

bool FFmpegDemuxerStream::IsEnabled() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return is_enabled_;
}

void FFmpegDemuxerStream::SetEnabled(bool enabled, base::TimeDelta timestamp) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(demuxer_);
  DCHECK(demuxer_->ffmpeg_task_runner());
  if (enabled == is_enabled_)
    return;

  is_enabled_ = enabled;
  demuxer_->ffmpeg_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&SetAVStreamDiscard, av_stream(),
                                enabled ? AVDISCARD_DEFAULT : AVDISCARD_ALL));
  if (is_enabled_) {
    waiting_for_keyframe_ = true;
  }
  if (!is_enabled_ && read_cb_) {
    DVLOG(1) << "Read from disabled stream, returning EOS";
    std::move(read_cb_).Run(kOk, {DecoderBuffer::CreateEOSBuffer()});
  }
}

void FFmpegDemuxerStream::SetLiveness(StreamLiveness liveness) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(liveness_, StreamLiveness::kUnknown);
  liveness_ = liveness;
}

Ranges<base::TimeDelta> FFmpegDemuxerStream::GetBufferedRanges() const {
  return buffered_ranges_;
}

void FFmpegDemuxerStream::SatisfyPendingRead() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (read_cb_) {
    if (!buffer_queue_.IsEmpty()) {
      DemuxerStream::DecoderBufferVector output_buffers;

      for (size_t i = 0;
           i < std::min(requested_buffer_count_, buffer_queue_.queue_size());
           ++i) {
        output_buffers.emplace_back(buffer_queue_.Pop());
      }
      DVLOG(3) << __func__ << " Status:kOk, return output_buffers.size = "
               << output_buffers.size();
      std::move(read_cb_).Run(DemuxerStream::kOk, std::move(output_buffers));
    } else if (end_of_stream_) {
      std::move(read_cb_).Run(DemuxerStream::kOk,
                              {DecoderBuffer::CreateEOSBuffer()});
    }
  }
  // Have capacity? Ask for more!
  if (HasAvailableCapacity() && !end_of_stream_) {
    demuxer_->NotifyCapacityAvailable();
  }
}

bool FFmpegDemuxerStream::HasAvailableCapacity() {
  // Try to have two second's worth of encoded data per stream.
  const base::TimeDelta kCapacity = base::Seconds(2);
  return buffer_queue_.IsEmpty() || buffer_queue_.Duration() < kCapacity;
}

size_t FFmpegDemuxerStream::MemoryUsage() const {
  return buffer_queue_.memory_usage_in_bytes();
}

std::string FFmpegDemuxerStream::GetMetadata(const char* key) const {
  const AVDictionaryEntry* entry =
      av_dict_get(stream_->metadata, key, nullptr, 0);
  return (entry == nullptr || entry->value == nullptr) ? "" : entry->value;
}

// static
base::TimeDelta FFmpegDemuxerStream::ConvertStreamTimestamp(
    const AVRational& time_base,
    int64_t timestamp) {
  if (timestamp == kNoFFmpegTimestamp)
    return kNoTimestamp;

  return ConvertFromTimeBase(time_base, timestamp);
}

//
// FFmpegDemuxer
//
FFmpegDemuxer::FFmpegDemuxer(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    DataSource* data_source,
    const EncryptedMediaInitDataCB& encrypted_media_init_data_cb,
    MediaTracksUpdatedCB media_tracks_updated_cb,
    MediaLog* media_log,
    bool is_local_file)
    : task_runner_(task_runner),
      // FFmpeg has no asynchronous API, so we use base::WaitableEvents inside
      // the BlockingUrlProtocol to handle hops to the render thread for network
      // reads and seeks.
      blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING})),
      data_source_(data_source),
      media_log_(media_log),
      encrypted_media_init_data_cb_(encrypted_media_init_data_cb),
      media_tracks_updated_cb_(std::move(media_tracks_updated_cb)),
      is_local_file_(is_local_file) {
  DCHECK(task_runner_.get());
  DCHECK(data_source_);
  DCHECK(media_tracks_updated_cb_);
}

FFmpegDemuxer::~FFmpegDemuxer() {
  DCHECK(!init_cb_);
  DCHECK(!pending_seek_cb_);

  // NOTE: This class is not destroyed on |task_runner|, so we must ensure that
  // there are no outstanding WeakPtrs by the time we reach here.
  DCHECK(!weak_factory_.HasWeakPtrs());

  // Clear `streams_` before `glue_` below since they may reference it.
  streams_.clear();

  // There may be outstanding tasks in the blocking pool which are trying to use
  // these members, so release them in sequence with any outstanding calls. The
  // earlier call to Abort() on |data_source_| prevents further access to it.
  blocking_task_runner_->DeleteSoon(FROM_HERE, url_protocol_.release());
  blocking_task_runner_->DeleteSoon(FROM_HERE, glue_.release());
}

std::string FFmpegDemuxer::GetDisplayName() const {
  return "FFmpegDemuxer";
}

DemuxerType FFmpegDemuxer::GetDemuxerType() const {
  return DemuxerType::kFFmpegDemuxer;
}

void FFmpegDemuxer::Initialize(DemuxerHost* host,
                               PipelineStatusCallback init_cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  host_ = host;
  weak_this_ = cancel_pending_seek_factory_.GetWeakPtr();
  init_cb_ = std::move(init_cb);

  // Give a WeakPtr to BlockingUrlProtocol since we'll need to release it on the
  // blocking thread pool.
  url_protocol_ = std::make_unique<BlockingUrlProtocol>(
      data_source_, base::BindPostTaskToCurrentDefault(base::BindRepeating(
                        &FFmpegDemuxer::OnDataSourceError, weak_this_)));
  glue_ = std::make_unique<FFmpegGlue>(url_protocol_.get());
  AVFormatContext* format_context = glue_->format_context();

  // Disable ID3v1 tag reading to avoid costly seeks to end of file for data we
  // don't use.  FFmpeg will only read ID3v1 tags if no other metadata is
  // available, so add a metadata entry to ensure some is always present.
  av_dict_set(&format_context->metadata, "skip_id3v1_tags", "", 0);

  // Ensure ffmpeg doesn't give up too early while looking for stream params;
  // this does not increase the amount of data downloaded.  The default value
  // is 5 AV_TIME_BASE units (1 second each), which prevents some oddly muxed
  // streams from being detected properly; this value was chosen arbitrarily.
  format_context->max_analyze_duration = 60 * AV_TIME_BASE;

  // Open the AVFormatContext using our glue layer.
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FFmpegGlue::OpenContext, base::Unretained(glue_.get()),
                     is_local_file_),
      base::BindOnce(&FFmpegDemuxer::OnOpenContextDone,
                     weak_factory_.GetWeakPtr()));
}

void FFmpegDemuxer::AbortPendingReads() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // If Stop() has been called, then drop this call.
  if (stopped_)
    return;

  // This should only be called after the demuxer has been initialized.
  DCHECK_GT(streams_.size(), 0u);

  // Abort all outstanding reads.
  for (const auto& stream : streams_) {
    if (stream)
      stream->Abort();
  }

  // It's important to invalidate read/seek completion callbacks to avoid any
  // errors that occur because of the data source abort.
  weak_factory_.InvalidateWeakPtrs();
  data_source_->Abort();

  // Aborting the read may cause EOF to be marked, undo this.
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UnmarkEndOfStreamAndClearError, glue_->format_context()));
  pending_read_ = false;

  // TODO(dalecurtis): We probably should report PIPELINE_ERROR_ABORT here
  // instead to avoid any preroll work that may be started upon return, but
  // currently the PipelineImpl does not know how to handle this.
  if (pending_seek_cb_)
    RunPendingSeekCB(PIPELINE_OK);
}

void FFmpegDemuxer::Stop() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (init_cb_)
    RunInitCB(PIPELINE_ERROR_ABORT);
  if (pending_seek_cb_)
    RunPendingSeekCB(PIPELINE_ERROR_ABORT);

  // The order of Stop() and Abort() is important here.  If Abort() is called
  // first, control may pass into FFmpeg where it can destruct buffers that are
  // in the process of being fulfilled by the DataSource.
  data_source_->Stop();
  url_protocol_->Abort();

  for (const auto& stream : streams_) {
    if (stream)
      stream->Stop();
  }

  data_source_ = nullptr;

  // Invalidate WeakPtrs on |task_runner_|, destruction may happen on another
  // thread. We don't need to wait for any outstanding tasks since they will all
  // fail to return after invalidating WeakPtrs.
  stopped_ = true;
  weak_factory_.InvalidateWeakPtrs();
  cancel_pending_seek_factory_.InvalidateWeakPtrs();
}

void FFmpegDemuxer::StartWaitingForSeek(base::TimeDelta seek_time) {}

void FFmpegDemuxer::CancelPendingSeek(base::TimeDelta seek_time) {
  if (task_runner_->RunsTasksInCurrentSequence()) {
    AbortPendingReads();
  } else {
    // Don't use GetWeakPtr() here since we are on the wrong thread.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FFmpegDemuxer::AbortPendingReads, weak_this_));
  }
}

void FFmpegDemuxer::Seek(base::TimeDelta time, PipelineStatusCallback cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!pending_seek_cb_);
  TRACE_EVENT_ASYNC_BEGIN0("media", "FFmpegDemuxer::Seek", this);
  pending_seek_cb_ = std::move(cb);
  SeekInternal(time, base::BindOnce(&FFmpegDemuxer::OnSeekFrameDone,
                                    weak_factory_.GetWeakPtr()));
}

bool FFmpegDemuxer::IsSeekable() const {
  return true;
}

void FFmpegDemuxer::SeekInternal(base::TimeDelta time,
                                 base::OnceCallback<void(int)> seek_cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // FFmpeg requires seeks to be adjusted according to the lowest starting time.
  // Since EnqueuePacket() rebased negative timestamps by the start time, we
  // must correct the shift here.
  //
  // Additionally, to workaround limitations in how we expose seekable ranges to
  // Blink (http://crbug.com/137275), we also want to clamp seeks before the
  // start time to the start time.
  base::TimeDelta seek_time;
  if (start_time_.is_negative()) {
    seek_time = time + start_time_;
  } else {
    seek_time = std::max(start_time_, time);
  }

  // When seeking in an opus stream we need to ensure we deliver enough data to
  // satisfy the seek preroll; otherwise the audio at the actual seek time will
  // not be entirely accurate.
  FFmpegDemuxerStream* audio_stream =
      GetFirstEnabledFFmpegStream(DemuxerStream::AUDIO);
  if (audio_stream) {
    const AudioDecoderConfig& config = audio_stream->audio_decoder_config();
    if (config.codec() == AudioCodec::kOpus)
      seek_time = std::max(start_time_, seek_time - config.seek_preroll());
  }

  // Choose the seeking stream based on whether it contains the seek time, if
  // no match can be found prefer the preferred stream.
  //
  // TODO(dalecurtis): Currently FFmpeg does not ensure that all streams in a
  // given container will demux all packets after the seek point.  Instead it
  // only guarantees that all packets after the file position of the seek will
  // be demuxed.  It's an open question whether FFmpeg should fix this:
  // http://lists.ffmpeg.org/pipermail/ffmpeg-devel/2014-June/159212.html
  // Tracked by http://crbug.com/387996.
  FFmpegDemuxerStream* demux_stream = FindPreferredStreamForSeeking(seek_time);
  DCHECK(demux_stream);
  const AVStream* seeking_stream = demux_stream->av_stream();
  DCHECK(seeking_stream);

  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AVSeekFrame, glue_->format_context(),
                     seeking_stream->index,
                     ConvertToTimeBase(seeking_stream->time_base, seek_time)),
      std::move(seek_cb));
}

base::Time FFmpegDemuxer::GetTimelineOffset() const {
  return timeline_offset_;
}

std::vector<DemuxerStream*> FFmpegDemuxer::GetAllStreams() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  std::vector<DemuxerStream*> result;
  // Put enabled streams at the beginning of the list so that
  // MediaResource::GetFirstStream returns the enabled stream if there is one.
  // TODO(servolk): Revisit this after media track switching is supported.
  for (const auto& stream : streams_) {
    if (stream && stream->IsEnabled())
      result.push_back(stream.get());
  }
  // And include disabled streams at the end of the list.
  for (const auto& stream : streams_) {
    if (stream && !stream->IsEnabled())
      result.push_back(stream.get());
  }
  return result;
}

FFmpegDemuxerStream* FFmpegDemuxer::GetFirstEnabledFFmpegStream(
    DemuxerStream::Type type) const {
  for (const auto& stream : streams_) {
    if (stream && stream->type() == type && stream->IsEnabled()) {
      return stream.get();
    }
  }
  return nullptr;
}

base::TimeDelta FFmpegDemuxer::GetStartTime() const {
  return std::max(start_time_, base::TimeDelta());
}

int64_t FFmpegDemuxer::GetMemoryUsage() const {
  int64_t allocation_size = 0;
  for (const auto& stream : streams_) {
    if (stream)
      allocation_size += stream->MemoryUsage();
  }
  return allocation_size;
}

std::optional<container_names::MediaContainerName>
FFmpegDemuxer::GetContainerForMetrics() const {
  return container();
}

void FFmpegDemuxer::OnEncryptedMediaInitData(
    EmeInitDataType init_data_type,
    const std::string& encryption_key_id) {
  std::vector<uint8_t> key_id_local(encryption_key_id.begin(),
                                    encryption_key_id.end());
  encrypted_media_init_data_cb_.Run(init_data_type, key_id_local);
}

void FFmpegDemuxer::NotifyCapacityAvailable() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  ReadFrameIfNeeded();
}

void FFmpegDemuxer::NotifyBufferingChanged() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  Ranges<base::TimeDelta> buffered;
  bool initialized_buffered_ranges = false;
  for (const auto& stream : streams_) {
    if (!stream)
      continue;
    if (initialized_buffered_ranges) {
      buffered = buffered.IntersectionWith(stream->GetBufferedRanges());
    } else {
      buffered = stream->GetBufferedRanges();
      initialized_buffered_ranges = true;
    }
  }
  host_->OnBufferedTimeRangesChanged(buffered);
}

// Helper for calculating the bitrate of the media based on information stored
// in |format_context| or failing that the size and duration of the media.
//
// Returns 0 if a bitrate could not be determined.
static int CalculateBitrate(AVFormatContext* format_context,
                            const base::TimeDelta& duration,
                            int64_t filesize_in_bytes) {
  // If there is a bitrate set on the container, use it.
  if (format_context->bit_rate > 0)
    return format_context->bit_rate;

  // Then try to sum the bitrates individually per stream.
  int bitrate = 0;
  for (size_t i = 0; i < format_context->nb_streams; ++i) {
    AVCodecParameters* codec_parameters = format_context->streams[i]->codecpar;
    bitrate += codec_parameters->bit_rate;
  }
  if (bitrate > 0)
    return bitrate;

  // See if we can approximate the bitrate as long as we have a filesize and
  // valid duration.
  if (duration <= base::TimeDelta() || duration == kInfiniteDuration ||
      !filesize_in_bytes)
    return 0;

  // Don't multiply by 8 first; it will overflow if (filesize_in_bytes >= 2^60).
  return base::ClampRound(filesize_in_bytes * duration.ToHz() * 8);
}

void FFmpegDemuxer::OnOpenContextDone(bool result) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (stopped_) {
    MEDIA_LOG(ERROR, media_log_) << GetDisplayName() << ": bad state";
    RunInitCB(PIPELINE_ERROR_ABORT);
    return;
  }

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_HLS_DEMUXER)
  if (glue_->detected_hls()) {
    MEDIA_LOG(INFO, media_log_)
        << GetDisplayName() << ": detected HLS manifest";
    RunInitCB(DEMUXER_ERROR_DETECTED_HLS);
    return;
  }
#endif

  if (!result) {
    MEDIA_LOG(ERROR, media_log_) << GetDisplayName() << ": open context failed";
    RunInitCB(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  // Fully initialize AVFormatContext by parsing the stream a little.
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&avformat_find_stream_info, glue_->format_context(),
                     static_cast<AVDictionary**>(nullptr)),
      base::BindOnce(&FFmpegDemuxer::OnFindStreamInfoDone,
                     weak_factory_.GetWeakPtr()));
}

void FFmpegDemuxer::OnFindStreamInfoDone(int result) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (stopped_ || !data_source_) {
    MEDIA_LOG(ERROR, media_log_) << GetDisplayName() << ": bad state";
    RunInitCB(PIPELINE_ERROR_ABORT);
    return;
  }

  if (result < 0) {
    MEDIA_LOG(ERROR, media_log_) << GetDisplayName()
                                 << ": find stream info failed";
    RunInitCB(DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }

  // Create demuxer stream entries for each possible AVStream. Each stream
  // is examined to determine if it is supported or not (is the codec enabled
  // for it in this release?). Unsupported streams are skipped, allowing for
  // partial playback. At least one audio or video stream must be playable.
  AVFormatContext* format_context = glue_->format_context();
  streams_.resize(format_context->nb_streams);

  std::unique_ptr<MediaTracks> media_tracks(new MediaTracks());

  DCHECK(track_id_to_demux_stream_map_.empty());

  // If available, |start_time_| will be set to the lowest stream start time.
  start_time_ = kInfiniteDuration;

  // Check if there is any enabled stream. If there are none, the stream enabled
  // flag will be ignored.
  bool has_disabled_stream = false;
  bool has_enabled_stream = false;
  for (size_t i = 0; i < format_context->nb_streams; ++i) {
    AVStream* stream = format_context->streams[i];

    // Only consider audio and video streams.
    const AVMediaType codec_type = stream->codecpar->codec_type;
    if (codec_type != AVMEDIA_TYPE_AUDIO && codec_type != AVMEDIA_TYPE_VIDEO) {
      continue;
    }

    if (!IsStreamEnabled(container(), stream)) {
      has_disabled_stream = true;
    } else {
      has_enabled_stream = true;
    }

    if (has_disabled_stream && has_enabled_stream) {
      break;
    }
  }
  // Display a warning if all streams are disabled.
  if (has_disabled_stream && !has_enabled_stream) {
    MEDIA_LOG(WARNING, media_log_)
        << GetDisplayName()
        << ": no tracks are enabled, track enabled flag will be ignored";
  }

  // Create an FFmpegDemuxerStreams for each enabled audio/video stream.
  base::TimeDelta max_duration;
  int supported_audio_track_count = 0;
  int supported_video_track_count = 0;
  bool has_opus_or_vorbis_audio = false;
  bool needs_negative_timestamp_fixup = false;
  for (size_t i = 0; i < format_context->nb_streams; ++i) {
    AVStream* stream = format_context->streams[i];
    const AVCodecParameters* codec_parameters = stream->codecpar;
    const AVMediaType codec_type = codec_parameters->codec_type;
    const AVCodecID codec_id = codec_parameters->codec_id;
    // Skip streams which are not properly detected.
    if (codec_id == AV_CODEC_ID_NONE) {
      stream->discard = AVDISCARD_ALL;
      continue;
    }

    if (codec_type == AVMEDIA_TYPE_AUDIO) {
      // Log the codec detected, whether it is supported or not, and whether or
      // not we have already detected a supported codec in another stream.
      const int32_t codec_hash = HashCodecName(GetCodecName(codec_id));
      base::UmaHistogramSparse("Media.DetectedAudioCodecHash", codec_hash);
      if (is_local_file_) {
        base::UmaHistogramSparse("Media.DetectedAudioCodecHash.Local",
                                 codec_hash);
      }
    } else if (codec_type == AVMEDIA_TYPE_VIDEO) {
      // Log the codec detected, whether it is supported or not, and whether or
      // not we have already detected a supported codec in another stream.
      const int32_t codec_hash = HashCodecName(GetCodecName(codec_id));
      base::UmaHistogramSparse("Media.DetectedVideoCodecHash", codec_hash);
      if (is_local_file_) {
        base::UmaHistogramSparse("Media.DetectedVideoCodecHash.Local",
                                 codec_hash);
      }

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
      if (codec_id == AV_CODEC_ID_HEVC) {
        // If ffmpeg is built without HEVC parser/decoder support, it will be
        // able to demux HEVC based solely on container-provided information,
        // but unable to get some of the parameters without parsing the stream
        // (e.g. coded size needs to be read from SPS, pixel format is typically
        // deduced from decoder config in hvcC box). These are not really needed
        // when using external decoder (e.g. hardware decoder), so override them
        // to make sure this translates into a valid VideoDecoderConfig. Coded
        // size is overridden in AVStreamToVideoDecoderConfig().
        if (stream->codecpar->format == AV_PIX_FMT_NONE)
          stream->codecpar->format = AV_PIX_FMT_YUV420P;
      }
#endif
    } else if (codec_type == AVMEDIA_TYPE_SUBTITLE) {
      stream->discard = AVDISCARD_ALL;
      continue;
    } else {
      stream->discard = AVDISCARD_ALL;
      continue;
    }

    // Attempt to create a FFmpegDemuxerStream from the AVStream. This will
    // return nullptr if the AVStream is invalid. Validity checks will verify
    // things like: codec, channel layout, sample/pixel format, etc...
    std::unique_ptr<FFmpegDemuxerStream> demuxer_stream =
        FFmpegDemuxerStream::Create(this, stream, media_log_);
    if (demuxer_stream.get()) {
      streams_[i] = std::move(demuxer_stream);
    } else {
      if (codec_type == AVMEDIA_TYPE_AUDIO) {
        MEDIA_LOG(INFO, media_log_)
            << GetDisplayName()
            << ": skipping invalid or unsupported audio track";
      } else if (codec_type == AVMEDIA_TYPE_VIDEO) {
        MEDIA_LOG(INFO, media_log_)
            << GetDisplayName()
            << ": skipping invalid or unsupported video track";
      }

      // This AVStream does not successfully convert.
      continue;
    }

    StreamParser::TrackId track_id =
        static_cast<StreamParser::TrackId>(media_tracks->tracks().size() + 1);
    auto track_label =
        MediaTrack::Label(streams_[i]->GetMetadata("handler_name"));
    auto track_language =
        MediaTrack::Language(streams_[i]->GetMetadata("language"));

    // Some metadata is named differently in FFmpeg for webm files.
    if (glue_->container() ==
        container_names::MediaContainerName::kContainerWEBM) {
      track_label = MediaTrack::Label(streams_[i]->GetMetadata("title"));
    }

    // Enable the first non-disabled stream for each of audio and video.
    // If all streams are disabled, they are treated as enabled instead.
    bool stream_enabled = true;
    if (codec_type == AVMEDIA_TYPE_AUDIO) {
      if (has_enabled_stream && !IsStreamEnabled(container(), stream)) {
        MEDIA_LOG(INFO, media_log_)
            << GetDisplayName() << ": skipping disabled audio track";
        stream_enabled = false;
      } else {
        ++supported_audio_track_count;
        stream_enabled = supported_audio_track_count == 1;
      }
    } else if (codec_type == AVMEDIA_TYPE_VIDEO) {
      if (has_enabled_stream && !IsStreamEnabled(container(), stream)) {
        MEDIA_LOG(INFO, media_log_)
            << GetDisplayName() << ": skipping disabled video track";
        stream_enabled = false;
      } else {
        ++supported_video_track_count;
        stream_enabled = supported_video_track_count == 1;
      }
    }
    streams_[i]->SetEnabled(stream_enabled, base::TimeDelta());

    // TODO(chcunningham): Remove the IsValidConfig() checks below. If the
    // config isn't valid we shouldn't have created a demuxer stream nor
    // an entry in |media_tracks|, so the check should always be true.
    if ((codec_type == AVMEDIA_TYPE_AUDIO &&
         media_tracks->getAudioConfig(track_id).IsValidConfig()) ||
        (codec_type == AVMEDIA_TYPE_VIDEO &&
         media_tracks->getVideoConfig(track_id).IsValidConfig())) {
      MEDIA_LOG(INFO, media_log_)
          << GetDisplayName()
          << ": skipping duplicate media stream id=" << track_id;
      continue;
    }

    // Note when we find our audio/video stream (we only want one of each) and
    // record src= playback UMA stats for the stream's decoder config.
    MediaTrack* media_track = nullptr;
    if (codec_type == AVMEDIA_TYPE_AUDIO) {
      AudioDecoderConfig audio_config = streams_[i]->audio_decoder_config();
      RecordAudioCodecStats(audio_config);

      media_track = media_tracks->AddAudioTrack(
          audio_config, stream_enabled, track_id, MediaTrack::Kind("main"),
          track_label, track_language);
      media_track->set_id(MediaTrack::Id(base::NumberToString(track_id)));
      DCHECK(track_id_to_demux_stream_map_.find(media_track->track_id()) ==
             track_id_to_demux_stream_map_.end());
      track_id_to_demux_stream_map_[media_track->track_id()] =
          streams_[i].get();
    } else if (codec_type == AVMEDIA_TYPE_VIDEO) {
      VideoDecoderConfig video_config = streams_[i]->video_decoder_config();

      RecordVideoCodecStats(glue_->container(), video_config,
                            stream->codecpar->color_range, media_log_);

      media_track = media_tracks->AddVideoTrack(
          video_config, stream_enabled, track_id, MediaTrack::Kind("main"),
          track_label, track_language);
      media_track->set_id(MediaTrack::Id(base::NumberToString(track_id)));
      DCHECK(track_id_to_demux_stream_map_.find(media_track->track_id()) ==
             track_id_to_demux_stream_map_.end());
      track_id_to_demux_stream_map_[media_track->track_id()] =
          streams_[i].get();
    }

    max_duration = std::max(max_duration, streams_[i]->duration());

    base::TimeDelta start_time = ExtractStartTime(stream);

    // Note: This value is used for seeking, so we must take the true value and
    // not the one possibly clamped to zero below.
    if (start_time != kNoTimestamp && start_time < start_time_)
      start_time_ = start_time;

    const bool is_opus_or_vorbis =
        codec_id == AV_CODEC_ID_OPUS || codec_id == AV_CODEC_ID_VORBIS;
    if (!has_opus_or_vorbis_audio)
      has_opus_or_vorbis_audio = is_opus_or_vorbis;

    if (codec_type == AVMEDIA_TYPE_AUDIO && start_time.is_negative() &&
        is_opus_or_vorbis) {
      needs_negative_timestamp_fixup = true;

      // Fixup the seeking information to avoid selecting the audio stream
      // simply because it has a lower starting time.
      start_time = base::TimeDelta();
    }

    streams_[i]->set_start_time(start_time);
  }

  if (media_tracks->tracks().empty()) {
    MEDIA_LOG(ERROR, media_log_) << GetDisplayName()
                                 << ": no supported streams";
    RunInitCB(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
    return;
  }

  if (format_context->duration != kNoFFmpegTimestamp) {
    // If there is a duration value in the container use that to find the
    // maximum between it and the duration from A/V streams.
    const AVRational av_time_base = {1, AV_TIME_BASE};
    max_duration =
        std::max(max_duration,
                 ConvertFromTimeBase(av_time_base, format_context->duration));
  } else {
    // The duration is unknown, in which case this is likely a live stream.
    max_duration = kInfiniteDuration;
  }

  // Chained ogg is only allowed on single track audio only opus/vorbis media.
  const bool needs_chained_ogg_fixup =
      glue_->container() ==
          container_names::MediaContainerName::kContainerOgg &&
      supported_audio_track_count == 1 && !supported_video_track_count &&
      has_opus_or_vorbis_audio;

  // FFmpeg represents audio data marked as before the beginning of stream as
  // having negative timestamps.  This data must be discarded after it has been
  // decoded, not before since it is used to warmup the decoder.  There are
  // currently two known cases for this: vorbis in ogg and opus.
  //
  // For API clarity, it was decided that the rest of the media pipeline should
  // not be exposed to negative timestamps.  Which means we need to rebase these
  // negative timestamps and mark them for discard post decoding.
  //
  // Post-decode frame dropping for packets with negative timestamps is outlined
  // in section A.2 in the Ogg Vorbis spec:
  // http://xiph.org/vorbis/doc/Vorbis_I_spec.html
  //
  // FFmpeg's use of negative timestamps for opus pre-skip is nonstandard, but
  // for more information on pre-skip see section 4.2 of the Ogg Opus spec:
  // https://tools.ietf.org/html/draft-ietf-codec-oggopus-08#section-4.2
  if (needs_negative_timestamp_fixup || needs_chained_ogg_fixup) {
    for (auto& stream : streams_) {
      if (!stream)
        continue;
      if (needs_negative_timestamp_fixup)
        stream->enable_negative_timestamp_fixups();
      if (needs_chained_ogg_fixup)
        stream->enable_chained_ogg_fixups();
    }
  }

  // If no start time could be determined, default to zero.
  if (start_time_ == kInfiniteDuration)
    start_time_ = base::TimeDelta();

  // MPEG-4 B-frames cause grief for a simple container like AVI. Enable PTS
  // generation so we always get timestamps, see http://crbug.com/169570
  if (glue_->container() ==
      container_names::MediaContainerName::kContainerAVI) {
    format_context->flags |= AVFMT_FLAG_GENPTS;
  }

  // FFmpeg will incorrectly adjust the start time of MP3 files into the future
  // based on discard samples. We were unable to fix this upstream without
  // breaking ffmpeg functionality. https://crbug.com/1062037
  if (glue_->container() ==
      container_names::MediaContainerName::kContainerMP3) {
    start_time_ = base::TimeDelta();
  }

  // For testing purposes, don't overwrite the timeline offset if set already.
  if (timeline_offset_.is_null()) {
    timeline_offset_ =
        ExtractTimelineOffset(glue_->container(), format_context);
  }

  // Since we're shifting the externally visible start time to zero, we need to
  // adjust the timeline offset to compensate.
  if (!timeline_offset_.is_null() && start_time_.is_negative())
    timeline_offset_ += start_time_;

  if (max_duration == kInfiniteDuration && !timeline_offset_.is_null()) {
    SetLiveness(StreamLiveness::kLive);
  } else if (max_duration != kInfiniteDuration) {
    SetLiveness(StreamLiveness::kRecorded);
  } else {
    SetLiveness(StreamLiveness::kUnknown);
  }

  // Good to go: set the duration and bitrate and notify we're done
  // initializing.
  host_->SetDuration(max_duration);
  duration_ = max_duration;
  duration_known_ = (max_duration != kInfiniteDuration);

  int64_t filesize_in_bytes = 0;
  url_protocol_->GetSize(&filesize_in_bytes);
  bitrate_ = CalculateBitrate(format_context, max_duration, filesize_in_bytes);
  if (bitrate_ > 0)
    data_source_->SetBitrate(bitrate_);

  LogMetadata(format_context, max_duration);
  media_tracks_updated_cb_.Run(std::move(media_tracks));

  RunInitCB(PIPELINE_OK);
}

void FFmpegDemuxer::LogMetadata(AVFormatContext* avctx,
                                base::TimeDelta max_duration) {
  std::vector<AudioDecoderConfig> audio_tracks;
  std::vector<VideoDecoderConfig> video_tracks;

  DCHECK_EQ(avctx->nb_streams, streams_.size());

  for (auto const& stream : streams_) {
    if (!stream)
      continue;
    if (stream->type() == DemuxerStream::AUDIO) {
      audio_tracks.push_back(stream->audio_decoder_config());
    } else if (stream->type() == DemuxerStream::VIDEO) {
      video_tracks.push_back(stream->video_decoder_config());
    }
  }
  media_log_->SetProperty<MediaLogProperty::kAudioTracks>(audio_tracks);
  media_log_->SetProperty<MediaLogProperty::kVideoTracks>(video_tracks);
  media_log_->SetProperty<MediaLogProperty::kMaxDuration>(max_duration);
  media_log_->SetProperty<MediaLogProperty::kStartTime>(start_time_);
  media_log_->SetProperty<MediaLogProperty::kBitrate>(bitrate_);
}

FFmpegDemuxerStream* FFmpegDemuxer::FindStreamWithLowestStartTimestamp(
    bool enabled) {
  FFmpegDemuxerStream* lowest_start_time_stream = nullptr;
  for (const auto& stream : streams_) {
    if (!stream || stream->IsEnabled() != enabled)
      continue;
    if (av_stream_get_first_dts(stream->av_stream()) == kInvalidPTSMarker)
      continue;
    if (!lowest_start_time_stream ||
        stream->start_time() < lowest_start_time_stream->start_time()) {
      lowest_start_time_stream = stream.get();
    }
  }
  return lowest_start_time_stream;
}

FFmpegDemuxerStream* FFmpegDemuxer::FindPreferredStreamForSeeking(
    base::TimeDelta seek_time) {
  // If we have a selected/enabled video stream and its start time is lower
  // than the |seek_time| or unknown, then always prefer it for seeking.
  for (const auto& stream : streams_) {
    if (!stream)
      continue;

    if (stream->type() != DemuxerStream::VIDEO)
      continue;

    if (av_stream_get_first_dts(stream->av_stream()) == kInvalidPTSMarker)
      continue;

    if (!stream->IsEnabled())
      continue;

    if (stream->start_time() <= seek_time)
      return stream.get();
  }

  // If video stream is not present or |seek_time| is lower than the video start
  // time, then try to find an enabled stream with the lowest start time.
  FFmpegDemuxerStream* lowest_start_time_enabled_stream =
      FindStreamWithLowestStartTimestamp(true);
  if (lowest_start_time_enabled_stream &&
      lowest_start_time_enabled_stream->start_time() <= seek_time) {
    return lowest_start_time_enabled_stream;
  }

  // If there's no enabled streams to consider from, try a disabled stream with
  // the lowest known start time.
  FFmpegDemuxerStream* lowest_start_time_disabled_stream =
      FindStreamWithLowestStartTimestamp(false);
  if (lowest_start_time_disabled_stream &&
      lowest_start_time_disabled_stream->start_time() <= seek_time) {
    return lowest_start_time_disabled_stream;
  }

  // Otherwise fall back to any other stream.
  for (const auto& stream : streams_) {
    if (stream)
      return stream.get();
  }

  NOTREACHED();
}

void FFmpegDemuxer::OnSeekFrameDone(int result) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(pending_seek_cb_);

  if (stopped_) {
    MEDIA_LOG(ERROR, media_log_) << GetDisplayName() << ": bad state";
    RunPendingSeekCB(PIPELINE_ERROR_ABORT);
    return;
  }

  if (result < 0) {
    MEDIA_LOG(ERROR, media_log_) << GetDisplayName() << ": demuxer seek failed";
    RunPendingSeekCB(PIPELINE_ERROR_READ);
    return;
  }

  // Tell streams to flush buffers due to seeking.
  for (const auto& stream : streams_) {
    if (stream)
      stream->FlushBuffers(false);
  }

  // Resume reading until capacity.
  ReadFrameIfNeeded();

  // Notify we're finished seeking.
  RunPendingSeekCB(PIPELINE_OK);
}

void FFmpegDemuxer::FindAndEnableProperTracks(
    const std::vector<MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    DemuxerStream::Type track_type,
    TrackChangeCB change_completed_cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  std::set<FFmpegDemuxerStream*> enabled_streams;
  for (const auto& id : track_ids) {
    auto it = track_id_to_demux_stream_map_.find(id);
    if (it == track_id_to_demux_stream_map_.end())
      continue;
    FFmpegDemuxerStream* stream = it->second;
    DCHECK_EQ(track_type, stream->type());
    // TODO(servolk): Remove after multiple enabled audio tracks are supported
    // by the media::RendererImpl.
    if (!enabled_streams.empty()) {
      MEDIA_LOG(INFO, media_log_)
          << "Only one enabled audio track is supported, ignoring track " << id;
      continue;
    }
    enabled_streams.insert(stream);
    stream->SetEnabled(true, curr_time);
  }

  // First disable all streams that need to be disabled and then enable streams
  // that are enabled.
  for (const auto& stream : streams_) {
    if (stream && stream->type() == track_type &&
        enabled_streams.find(stream.get()) == enabled_streams.end()) {
      DVLOG(1) << __func__ << ": disabling stream " << stream.get();
      stream->SetEnabled(false, curr_time);
    }
  }

  std::vector<DemuxerStream*> streams(enabled_streams.begin(),
                                      enabled_streams.end());
  std::move(change_completed_cb).Run(streams);
}

void FFmpegDemuxer::OnEnabledAudioTracksChanged(
    const std::vector<MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  FindAndEnableProperTracks(track_ids, curr_time, DemuxerStream::AUDIO,
                            std::move(change_completed_cb));
}

void FFmpegDemuxer::OnVideoSeekedForTrackChange(
    DemuxerStream* video_stream,
    base::OnceClosure seek_completed_cb,
    int result) {
  static_cast<FFmpegDemuxerStream*>(video_stream)->FlushBuffers(true);
  // TODO(crbug.com/40898124): Report seek failures for track changes too.
  std::move(seek_completed_cb).Run();
}

void FFmpegDemuxer::SeekOnVideoTrackChange(
    base::TimeDelta seek_to_time,
    TrackChangeCB seek_completed_cb,
    const std::vector<DemuxerStream*>& streams) {
  if (streams.size() != 1u) {
    // If FFmpegDemuxer::FindAndEnableProperTracks() was not able to find the
    // selected streams in the ID->DemuxerStream map, then its possible for
    // this vector to be empty. If that's the case, we don't want to bother
    // with seeking, and just call the callback immediately.
    std::move(seek_completed_cb).Run(streams);
    return;
  }
  SeekInternal(
      seek_to_time,
      base::BindOnce(&FFmpegDemuxer::OnVideoSeekedForTrackChange,
                     weak_factory_.GetWeakPtr(), streams[0],
                     base::BindOnce(std::move(seek_completed_cb), streams)));
}

void FFmpegDemuxer::OnSelectedVideoTrackChanged(
    const std::vector<MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  // Find tracks -> Seek track -> run callback.
  FindAndEnableProperTracks(
      track_ids, curr_time, DemuxerStream::VIDEO,
      track_ids.empty() ? std::move(change_completed_cb)
                        : base::BindOnce(&FFmpegDemuxer::SeekOnVideoTrackChange,
                                         weak_factory_.GetWeakPtr(), curr_time,
                                         std::move(change_completed_cb)));
}

void FFmpegDemuxer::ReadFrameIfNeeded() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Make sure we have work to do before reading.
  if (stopped_ || !StreamsHaveAvailableCapacity() || pending_read_ ||
      pending_seek_cb_) {
    return;
  }

  // Allocate and read an AVPacket from the media. Save |packet_ptr| since
  // evaluation order of packet.get() and std::move(&packet) is
  // undefined.
  auto packet = ScopedAVPacket::Allocate();
  AVPacket* packet_ptr = packet.get();

  pending_read_ = true;
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ReadFrameAndDiscardEmpty, glue_->format_context(),
                     packet_ptr),
      base::BindOnce(&FFmpegDemuxer::OnReadFrameDone,
                     weak_factory_.GetWeakPtr(), std::move(packet)));
}

void FFmpegDemuxer::OnReadFrameDone(ScopedAVPacket packet, int result) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(pending_read_);
  pending_read_ = false;

  if (stopped_ || pending_seek_cb_)
    return;

  // Consider the stream as ended if:
  // - either underlying ffmpeg returned an error
  // - or FFMpegDemuxer reached the maximum allowed memory usage.
  if (result < 0 || IsMaxMemoryUsageReached()) {
    if (result < 0) {
      MEDIA_LOG(DEBUG, media_log_)
          << GetDisplayName()
          << ": av_read_frame(): " << AVErrorToString(result);
    } else {
      MEDIA_LOG(DEBUG, media_log_)
          << GetDisplayName() << ": memory limit exceeded";
    }

    // Update the duration based on the highest elapsed time across all streams.
    base::TimeDelta max_duration;
    for (const auto& stream : streams_) {
      if (!stream)
        continue;

      base::TimeDelta duration = stream->duration();
      if (duration != kNoTimestamp && duration > max_duration)
        max_duration = duration;
    }

    if (duration_ == kInfiniteDuration || max_duration > duration_) {
      host_->SetDuration(max_duration);
      duration_known_ = true;
      duration_ = max_duration;
    }

    // If we have reached the end of stream, tell the downstream filters about
    // the event.
    StreamHasEnded();
    return;
  }

  // Queue the packet with the appropriate stream; we must defend against ffmpeg
  // giving us a bad stream index.  See http://crbug.com/698549 for example.
  if (packet->stream_index >= 0 &&
      static_cast<size_t>(packet->stream_index) < streams_.size()) {
    // This is ensured by ReadFrameAndDiscardEmpty.
    DCHECK(packet->data);
    DCHECK(packet->size);

    if (auto& demuxer_stream = streams_[packet->stream_index]) {
      if (demuxer_stream->IsEnabled())
        demuxer_stream->EnqueuePacket(std::move(packet));

      // If duration estimate was incorrect, update it and tell higher layers.
      if (duration_known_) {
        const base::TimeDelta duration = demuxer_stream->duration();
        if (duration != kNoTimestamp && duration > duration_) {
          duration_ = duration;
          host_->SetDuration(duration_);
        }
      }
    }
  }

  // Keep reading until we've reached capacity.
  ReadFrameIfNeeded();
}

bool FFmpegDemuxer::StreamsHaveAvailableCapacity() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  for (const auto& stream : streams_) {
    if (stream && stream->IsEnabled() && stream->HasAvailableCapacity())
      return true;
  }
  return false;
}

bool FFmpegDemuxer::IsMaxMemoryUsageReached() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  size_t memory_left =
      GetDemuxerMemoryLimit(Demuxer::DemuxerTypes::kFFmpegDemuxer);
  for (const auto& stream : streams_) {
    if (!stream)
      continue;

    size_t stream_memory_usage = stream->MemoryUsage();
    if (stream_memory_usage > memory_left)
      return true;
    memory_left -= stream_memory_usage;
  }
  return false;
}

void FFmpegDemuxer::StreamHasEnded() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  for (const auto& stream : streams_) {
    if (stream)
      stream->SetEndOfStream();
  }
}

void FFmpegDemuxer::OnDataSourceError() {
  MEDIA_LOG(ERROR, media_log_) << GetDisplayName() << ": data source error";
  host_->OnDemuxerError(PIPELINE_ERROR_READ);
}

void FFmpegDemuxer::NotifyDemuxerError(PipelineStatus status) {
  MEDIA_LOG(ERROR, media_log_) << GetDisplayName()
                               << ": demuxer error: " << status;
  host_->OnDemuxerError(status);
}

void FFmpegDemuxer::SetLiveness(StreamLiveness liveness) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  for (const auto& stream : streams_) {
    if (stream)
      stream->SetLiveness(liveness);
  }
}

void FFmpegDemuxer::RunInitCB(PipelineStatus status) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(init_cb_);
  TRACE_EVENT_ASYNC_END1("media", "FFmpegDemuxer::Initialize", this, "status",
                         PipelineStatusToString(status));
  std::move(init_cb_).Run(status);
}

void FFmpegDemuxer::RunPendingSeekCB(PipelineStatus status) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(pending_seek_cb_);
  TRACE_EVENT_ASYNC_END1("media", "FFmpegDemuxer::Seek", this, "status",
                         PipelineStatusToString(status));
  std::move(pending_seek_cb_).Run(status);
}

}  // namespace media
