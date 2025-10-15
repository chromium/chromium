// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_demuxer.h"

#include <algorithm>
#include <iterator>
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
#include "media/base/data_source.h"
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
#include "third_party/perfetto/include/perfetto/tracing/track.h"

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
#include "media/filters/ffmpeg_h265_to_annex_b_bitstream_converter.h"
#endif

namespace media {

namespace {

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
        !IsDecoderSupportedAudioType(
            AudioType::FromDecoderConfig(*audio_config))) {
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
        !IsDecoderSupportedVideoType(
            VideoType::FromDecoderConfig(*video_config))) {
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
      stream_start_time_(
          ConvertStreamTimestamp(stream->time_base, stream->start_time)),
      audio_config_(audio_config.release()),
      video_config_(video_config.release()),
      media_log_(media_log),
      duration_(ConvertStreamTimestamp(stream->time_base, stream->duration)),
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
      NOTREACHED();
  }

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

base::span<const uint8_t> GetSideData(const AVPacket* packet) {
  size_t side_data_size = 0;
  uint8_t* side_data = av_packet_get_side_data(
      packet, AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL, &side_data_size);

  // SAFETY:
  // https://ffmpeg.org/doxygen/6.0/group__lavc__packet.html#ga68712351b8a025b464e5c854d4a9fe1f
  // ffmpeg documentation: av_packet_get_side_data() returns a pointer to
  // already allocated data with a valid size if present and sets `size`
  // to its length, or nullptr if no data is available and sets `size` to zero.
  //
  // Since we are not allocating memory, and it is considered a valid use case
  // to construct a base::span<> from nullptr with size zero, this is safe.
  return UNSAFE_BUFFERS(base::span<const uint8_t>(side_data, side_data_size));
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
    DVLOG(3) << "Attempted to enqueue packet on a stopped stream";
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
    DVLOG(1) << "Dropped packet that can't be converted to AnnexB"
             << " pts=" << packet->pts;
    return;
  }
#endif

  scoped_refptr<DecoderBuffer> buffer;

  base::span<const uint8_t> side_data = GetSideData(packet.get());

  std::unique_ptr<DecryptConfig> decrypt_config;
  size_t data_offset = 0;
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
    base::span<const uint8_t> packet_span = AVPacketData(*packet);
    auto iter =
        std::ranges::find_if(packet_span, [](uint8_t v) { return v != 0; });
    data_offset = std::distance(packet_span.begin(), iter);
    packet_span = packet_span.subspan(data_offset);

    if (packet_span.size() < MPEG1AudioStreamParser::kHeaderSize ||
        !MPEG1AudioStreamParser::ParseHeader(nullptr, nullptr, packet_span,
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
  buffer = DecoderBuffer::CopyFrom(AVPacketData(*packet).subspan(data_offset));

  if (side_data.size() > 8) {
    // First 8 bytes of side data is the side_data_id in big endian. This is the
    // same as the matroska BlockAddID whose values are documented here:
    // https://www.matroska.org/technical/codec_specs.html#block-addition-mappings
    const uint64_t side_data_id = base::U64FromBigEndian(side_data.first<8u>());
    if (side_data_id == 1) {
      buffer->WritableSideData().alpha_data =
          base::HeapArray<uint8_t>::CopiedFrom(side_data.subspan(8u));
    } else if (side_data_id == 4) {
      buffer->WritableSideData().itu_t35_data =
          base::HeapArray<uint8_t>::CopiedFrom(side_data.subspan(8u));
    }
  }

  if (decrypt_config) {
    buffer->set_decrypt_config(std::move(decrypt_config));
  }

  // Treat durations under 1ms as not having duration, later stages of the
  // pipeline will then use the timestamps to estimate duration. Incorrect
  // duration information can lead to stuttering effects during seeking. See
  // https://crbug.com/397343886.
  auto d = ConvertStreamTimestamp(stream_->time_base, packet->duration);
  buffer->set_duration(d <= base::Milliseconds(1) ? kNoTimestamp : d);

  // Note: If pts is kNoFFmpegTimestamp, stream_timestamp will be kNoTimestamp.
  const base::TimeDelta stream_timestamp =
      ConvertStreamTimestamp(stream_->time_base, packet->pts);

  if (stream_timestamp == kNoTimestamp ||
      stream_timestamp == kInfiniteDuration) {
    MEDIA_LOG(ERROR, media_log_) << "FFmpegDemuxer: PTS is not defined";
    demuxer_->NotifyDemuxerError(DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }

  buffer->set_timestamp(stream_timestamp);

  auto discard_padding = GetDiscardPaddingFromAVPacket(
      packet.get(), is_audio ? audio_decoder_config().samples_per_second() : 0);

  // Codec delay functions a bit weird for consistency with MSE (which has
  // no concept of codec delay). We shift time such that all the preroll is
  // part of the seekable timeline. This data is still discarded during
  // decoding and the timeline unchanged such that a/v sync works properly.
  //
  // We may be able to stop doing this, but it's been the behavior for so
  // long it's hard to know what might break.
  if (is_audio && audio_decoder_config().codec_delay() &&
      demuxer_->start_time().is_negative()) {
    buffer->set_timestamp(stream_timestamp - demuxer_->start_time());
    DCHECK_GE(buffer->timestamp(), base::TimeDelta());
  }

  if (discard_padding) {
    buffer->set_discard_padding(*discard_padding);
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

    if (last_packet_timestamp_ < buffer->timestamp() &&
        buffer->timestamp() >= base::TimeDelta()) {
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

  const auto start_padding = buffer->discard_padding()
                                 ? buffer->discard_padding()->first
                                 : base::TimeDelta();
  const auto end_padding = buffer->discard_padding()
                               ? buffer->discard_padding()->second
                               : base::TimeDelta();

  // Save the timestamp of the first non-discarded frame, to calculate duration
  // below. Only the first buffer should have discard padding.
  // Note: Some packets marked for total discard have their `start_padding` set
  // to kInfiniteDuration. Ignore these packets.
  if (!initial_start_padding_.has_value() &&
      start_padding != kInfiniteDuration) {
    initial_start_padding_ = start_padding;
  }

  last_packet_timestamp_ = buffer->timestamp();
  last_packet_duration_ = buffer->duration();

  // Check if `buffer` contains only padding.
  const bool is_padding = buffer->duration() == start_padding + end_padding;

  const base::TimeDelta new_duration =
      last_packet_timestamp_ -
      initial_start_padding_.value_or(base::TimeDelta());

  if ((!is_padding && new_duration > duration_) || duration_ == kNoTimestamp) {
    duration_ = new_duration;
  }

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
  TRACE_EVENT_BEGIN("media", "FFmpegDemuxer::Seek",
                    perfetto::Track::FromPointer(this));
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
  FFmpegDemuxerStream* audio_stream =
      GetFirstEnabledFFmpegStream(DemuxerStream::AUDIO);

  base::TimeDelta seek_time;
  if (start_time_.is_negative() && audio_stream &&
      audio_stream->audio_decoder_config().codec_delay()) {
    seek_time = time + start_time_;
  } else {
    seek_time = std::max(start_time_, time);
  }

  if (audio_stream) {
    const AudioDecoderConfig& config = audio_stream->audio_decoder_config();

    // When seeking in an opus stream we need to ensure we deliver enough data
    // to satisfy the seek preroll; otherwise the audio at the actual seek time
    // will not be entirely accurate.
    if (config.codec() == AudioCodec::kOpus) {
      seek_time = std::max(start_time_, seek_time - config.seek_preroll());
    }

    // Seeking in MP3s is not precise due to our usage of AVFMT_FLAG_FAST_SEEK;
    // which works by looking for 3 contiguous MP3 packets since MP3 headers
    // are simple enough they can occur by random chance in a byte stream.
    //
    // Bytes early in the stream often look enough like an MP3 header to trip up
    // ffmpeg's seeking logic. As a workaround, round seeks within the first
    // frame to zero. The audio renderer will trim off the excess later.
    //
    // Technically these issues can happen anywhere in the stream since the MP3
    // header is not complex enough to avoid it happening by chance. We could
    // also fix this by not using AVFMT_FLAG_FAST_SEEK, but that hurts seeking
    // performance on large files too much.
    //
    // 1152 is the number of frame in a MPEG1 packet. Though packets of size 576
    // may occur with MPEG 2.5 streams we use 1152 here since the problematic
    // boundary was 0.1 and 576/48000 = 0.012, so 0.024 provide more margin of
    // error for this workaround.
    //
    // See https://crbug.com/415092041
    if (config.codec() == AudioCodec::kMP3 &&
        seek_time < base::Seconds(1152.0 / config.samples_per_second())) {
      seek_time = start_time_;
    }
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
  for (AVStream* stream : AVFormatContextToSpan(format_context)) {
    AVCodecParameters* codec_parameters = stream->codecpar;
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
  for (AVStream* stream : AVFormatContextToSpan(format_context)) {
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
  const base::span<AVStream*> stream_span =
      AVFormatContextToSpan(format_context);
  for (size_t i = 0; i < stream_span.size(); ++i) {
    AVStream* stream = stream_span[i];
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

  if (needs_chained_ogg_fixup) {
    for (auto& stream : streams_) {
      if (stream) {
        stream->enable_chained_ogg_fixups();
      }
    }
  }

  // If no start time could be determined, default to zero.
  if (start_time_ == kInfiniteDuration)
    start_time_ = base::TimeDelta();

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
    if (stream->stream_start_time() == kNoTimestamp) {
      continue;
    }
    if (!lowest_start_time_stream ||
        stream->stream_start_time() <
            lowest_start_time_stream->stream_start_time()) {
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
    if (!stream) {
      continue;
    }
    if (stream->type() != DemuxerStream::VIDEO) {
      continue;
    }
    if (stream->stream_start_time() == kNoTimestamp) {
      continue;
    }
    if (!stream->IsEnabled()) {
      continue;
    }
    if (stream->stream_start_time() <= seek_time) {
      return stream.get();
    }
  }

  // If video stream is not present or |seek_time| is lower than the video start
  // time, then try to find an enabled stream with the lowest start time.
  FFmpegDemuxerStream* lowest_start_time_enabled_stream =
      FindStreamWithLowestStartTimestamp(true);
  if (lowest_start_time_enabled_stream &&
      lowest_start_time_enabled_stream->stream_start_time() <= seek_time) {
    return lowest_start_time_enabled_stream;
  }

  // If there's no enabled streams to consider from, try a disabled stream with
  // the lowest known start time.
  FFmpegDemuxerStream* lowest_start_time_disabled_stream =
      FindStreamWithLowestStartTimestamp(false);
  if (lowest_start_time_disabled_stream &&
      lowest_start_time_disabled_stream->stream_start_time() <= seek_time) {
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

void FFmpegDemuxer::OnTrackChangeSeekComplete(
    base::OnceClosure cb,
    FFmpegDemuxerStream* stream_to_flush,
    int seek_status) {
  if (stream_to_flush) {
    CHECK(stream_to_flush->IsEnabled());
    stream_to_flush->FlushBuffers(true);
  }
  // TODO(crbug.com/41393620): Report seek failures for track changes too.
  std::move(cb).Run();
}

void FFmpegDemuxer::OnTracksChanged(DemuxerStream::Type track_type,
                                    std::optional<MediaTrack::Id> track_id,
                                    base::TimeDelta curr_time,
                                    TrackChangeCB change_completed_cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  bool seek_after_changing_tracks = false;
  DemuxerStream* response = nullptr;
  FFmpegDemuxerStream* stream_to_flush = nullptr;

  // Enable the stream associated with `track_id`
  if (track_id.has_value()) {
    auto it = track_id_to_demux_stream_map_.find(*track_id);
    if (it != track_id_to_demux_stream_map_.end()) {
      FFmpegDemuxerStream* stream = it->second;
      DCHECK_EQ(track_type, stream->type());
      if (!stream->IsEnabled()) {
        stream_to_flush = stream;
        seek_after_changing_tracks = true;
      }
      response = stream;
      stream->SetEnabled(true, curr_time);
    }
  }

  for (const auto& s : streams_) {
    if (s && s->type() == track_type && s.get() != response && s->IsEnabled()) {
      seek_after_changing_tracks = true;
      s->SetEnabled(false, curr_time);
    }
  }

  base::OnceCallback<void(int)> seek_cb = base::BindOnce(
      &FFmpegDemuxer::OnTrackChangeSeekComplete, weak_factory_.GetWeakPtr(),
      base::BindOnce(std::move(change_completed_cb), response),
      stream_to_flush);

  if (seek_after_changing_tracks) {
    SeekInternal(curr_time, std::move(seek_cb));
  } else {
    std::move(seek_cb).Run(0);
  }
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
      GetDemuxerMemoryLimit(DemuxerType::kFFmpegDemuxer).InBytes();
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
  TRACE_EVENT_END("media", perfetto::Track::FromPointer(this), "status",
                  PipelineStatusToString(status));
  std::move(init_cb_).Run(status);
}

void FFmpegDemuxer::RunPendingSeekCB(PipelineStatus status) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(pending_seek_cb_);
  TRACE_EVENT_END("media", perfetto::Track::FromPointer(this), "status",
                  PipelineStatusToString(status));
  std::move(pending_seek_cb_).Run(status);
}

}  // namespace media
