// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/muxers/mp4_muxer_delegate.h"

#include "base/logging.h"
#include "components/version_info/version_info.h"
#include "media/base/audio_parameters.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_codecs.h"
#include "media/formats/mp4/avc.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/muxers/box_byte_stream.h"
#include "media/muxers/mp4_box_writer.h"
#include "media/muxers/mp4_fragment_box_writer.h"
#include "media/muxers/mp4_movie_box_writer.h"
#include "media/muxers/mp4_muxer_delegate_fragment.h"
#include "media/muxers/mp4_type_conversion.h"
#include "media/muxers/output_position_tracker.h"
#include "third_party/libgav1/src/src/obu_parser.h"

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/formats/mp4/h264_annex_b_to_avc_bitstream_converter.h"
#endif

namespace media {

namespace {

using mp4::writable_boxes::TrackHeaderFlags;

constexpr char kVideoHandlerName[] = "VideoHandler";
constexpr char kAudioHandlerName[] = "SoundHandler";
constexpr char kUndefinedLanguageName[] = "und";

// Milliseconds time scale is set in the Movie header and it will
// be the base for the duration.
constexpr uint32_t kMillisecondsTimeScale = 1000u;
constexpr uint32_t kAudioSamplesPerFrame = 1024u;

void BuildTrack(
    mp4::writable_boxes::Movie& moov,
    size_t track_index,
    bool is_audio,
    uint32_t timescale,
    const mp4::writable_boxes::SampleDescription& sample_description) {
  mp4::writable_boxes::Track& track = moov.tracks[track_index];
  // `tkhd`.
  mp4::writable_boxes::TrackHeader track_header(track_index + 1, is_audio);
  track_header.flags = BuildFlags<TrackHeaderFlags>(
      {TrackHeaderFlags::kTrackEnabled, TrackHeaderFlags::kTrackInMovie});
  track.header = std::move(track_header);

  // `mdhd`
  track.media.header.timescale = timescale;
  track.media.header.language =
      kUndefinedLanguageName;  // use 'und' as default at this time.

  // `dinf`, `dref`, `url`.
  mp4::writable_boxes::DataInformation data_info;
  mp4::writable_boxes::DataUrlEntry url;
  data_info.data_reference.entries.emplace_back(std::move(url));
  track.media.information.data_information = std::move(data_info);

  // `trex`.
  mp4::writable_boxes::TrackExtends& audio_extends =
      moov.extends.track_extends[track_index];
  audio_extends.track_id = track_index + 1;

  // TODO(crbug.com/40275472): Various MP4 samples doesn't need
  // default_sample_duration, default_sample_size, default_sample_flags. We need
  // to investigate it further though whether we need to set these fields.
  audio_extends.default_sample_description_index = 1;
  audio_extends.default_sample_size = 0;
  audio_extends.default_sample_duration = base::Milliseconds(0);
  audio_extends.default_sample_flags = 0;

  // `stbl`, `stco`, `stsz`, `stts`, `stsc'.
  mp4::writable_boxes::SampleTable sample_table;
  sample_table.sample_to_chunk = mp4::writable_boxes::SampleToChunk();
  sample_table.decoding_time_to_sample =
      mp4::writable_boxes::DecodingTimeToSample();
  sample_table.sample_size = mp4::writable_boxes::SampleSize();
  sample_table.sample_chunk_offset = mp4::writable_boxes::SampleChunkOffset();
  sample_table.sample_description = std::move(sample_description);

  track.media.information.sample_table = std::move(sample_table);
}

void CopyCreationTimeAndDuration(mp4::writable_boxes::Track& track,
                                 const mp4::writable_boxes::MovieHeader& header,
                                 base::TimeDelta track_duration) {
  track.header.creation_time = header.creation_time;
  track.header.modification_time = header.modification_time;

  track.media.header.creation_time = header.creation_time;
  track.media.header.modification_time = header.modification_time;

  // video track duration on the `tkhd` and `mdhd`.
  track.header.duration = track_duration;
  track.media.header.duration = track_duration;
}

}  // namespace

Mp4MuxerDelegate::Mp4MuxerDelegate(
    AudioCodec audio_codec,
    std::optional<VideoCodecProfile> video_profile,
    std::optional<VideoCodecLevel> video_level,
    Muxer::WriteDataCB write_callback,
    size_t audio_sample_count_per_fragment)
    : write_callback_(std::move(write_callback)),
      audio_codec_(audio_codec),
      video_profile_(std::move(video_profile)),
      video_level_(std::move(video_level)),
      audio_sample_count_per_fragment_(audio_sample_count_per_fragment) {}

Mp4MuxerDelegate::~Mp4MuxerDelegate() = default;

void Mp4MuxerDelegate::AddVideoFrame(
    const Muxer::VideoParameters& params,
    scoped_refptr<DecoderBuffer> encoded_data,
    std::optional<VideoEncoder::CodecDescription> codec_description,
    base::TimeTicks timestamp) {
  DVLOG(1) << __func__ << ", " << params.AsHumanReadableString();

  if (!video_track_index_.has_value()) {
    CHECK(codec_description.has_value() || (params.codec != VideoCodec::kH264));
    CHECK(encoded_data->is_key_frame());
    CHECK(start_video_time_.is_null());
    CHECK_NE(params.codec, VideoCodec::kUnknown);

    video_codec_ = params.codec;

    EnsureInitialized();
    last_video_time_ = start_video_time_ = timestamp;

    CHECK_GT(params.frame_rate, 0);
    video_frame_rate_ = params.frame_rate;

    uint32_t timescale = video_frame_rate_ * kMillisecondsTimeScale;

    video_track_index_ = GetNextTrackIndex();
    context_->SetVideoTrack({video_track_index_.value(), timescale});
    DVLOG(1) << __func__ << ", video track timescale:" << timescale;

    BuildMovieVideoTrack(params, *encoded_data, std::move(codec_description));
  }
  last_video_time_ = timestamp;

  AddDataToVideoFragment(std::move(encoded_data));
}

void Mp4MuxerDelegate::BuildMovieVideoTrack(
    const Muxer::VideoParameters& params,
    const DecoderBuffer& encoded_data,
    std::optional<VideoEncoder::CodecDescription> codec_description) {
  DCHECK(video_track_index_.has_value());

  // `stsd`, `avc1`, `avcC`.
  mp4::writable_boxes::SampleDescription description;
  mp4::writable_boxes::VisualSampleEntry visual_sample_entry(video_codec_);

  visual_sample_entry.coded_size = params.visible_rect_size;
  visual_sample_entry.pixel_aspect_ratio =
      mp4::writable_boxes::PixelAspectRatioBox();

  if (video_codec_ == VideoCodec::kH264) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    visual_sample_entry.compressor_name = "AVC1 Coding";

    mp4::writable_boxes::AVCDecoderConfiguration avc_config;
    mp4::AVCDecoderConfigurationRecord avc_config_record;
    bool result = avc_config_record.Parse(codec_description.value().data(),
                                          codec_description.value().size());
    DCHECK(result);

    avc_config.avc_config_record = std::move(avc_config_record);
    visual_sample_entry.avc_decoder_configuration = std::move(avc_config);
#else
    NOTREACHED_IN_MIGRATION();
#endif
  } else if (video_codec_ == VideoCodec::kVP9) {
    visual_sample_entry.compressor_name = "VPC Coding";

    gfx::ColorSpace color_space;
    if (params.color_space) {
      color_space = *params.color_space;
    }

    // DefaultCodecProfile() returns VP9PROFILE_PROFILE0(VP9PROFILE_MIN).
    mp4::writable_boxes::VPCodecConfiguration vp_config(
        video_profile_.value_or(VP9PROFILE_PROFILE0), video_level_.value_or(0),
        color_space);
    visual_sample_entry.vp_decoder_configuration = std::move(vp_config);
  } else if (video_codec_ == VideoCodec::kAV1) {
    CHECK(!codec_description.has_value());

    visual_sample_entry.compressor_name = "AV1 Coding";

    mp4::writable_boxes::AV1CodecConfiguration av1_config;
    size_t config_size = 0;
    auto codec_descriptions = libgav1::ObuParser::GetAV1CodecConfigurationBox(
        encoded_data.data(), encoded_data.size(), &config_size);
    CHECK(codec_descriptions);
    CHECK_GT(config_size, 0u);

    av1_config.av1_decoder_configuration_data.assign(
        &codec_descriptions[0], &codec_descriptions[config_size]);
    visual_sample_entry.av1_decoder_configuration = std::move(av1_config);
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  description.video_sample_entry = std::move(visual_sample_entry);
  description.entry_count = 1;

  BuildTrack(*moov_, video_track_index_.value(), false,
             context_->GetVideoTrack().value().timescale, description);

  mp4::writable_boxes::Track& video_track =
      moov_->tracks[video_track_index_.value()];

  // `tkhd`.
  video_track.header.natural_size = params.visible_rect_size;

  // `hdlr`
  mp4::writable_boxes::MediaHandler media_handler(/*is_audio=*/false);
  media_handler.name = kVideoHandlerName;
  video_track.media.handler = std::move(media_handler);

  // `minf`

  // `vmhd`
  mp4::writable_boxes::VideoMediaHeader video_header;
  video_track.media.information.video_header = std::move(video_header);

  DVLOG(1) << __func__ << ", video track created";
}

void Mp4MuxerDelegate::AddDataToVideoFragment(
    scoped_refptr<DecoderBuffer> encoded_data) {
  DCHECK(video_track_index_.has_value());
  CreateFragmentIfNeeded(false, encoded_data->is_key_frame());

  Mp4MuxerDelegateFragment* fragment = fragments_.back().get();
  if (!fragment) {
    DVLOG(1) << __func__ << ", no valid video fragment exists";
    return;
  }

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (video_codec_ == VideoCodec::kH264) {
    // Convert Annex-B to AVC bitstream.
    encoded_data = ConvertNALUData(std::move(encoded_data));
  }
#endif

  fragment->AddVideoData(std::move(encoded_data), last_video_time_);

  MaybeFlushFileTypeBoxForStartup();
}

void Mp4MuxerDelegate::AddAudioFrame(
    const AudioParameters& params,
    scoped_refptr<DecoderBuffer> encoded_data,
    std::optional<AudioEncoder::CodecDescription> codec_description,
    base::TimeTicks timestamp) {
  if (!audio_track_index_.has_value()) {
    DVLOG(1) << __func__ << ", " << params.AsHumanReadableString();

    CHECK(codec_description.has_value() || (audio_codec_ == AudioCodec::kOpus));
    CHECK(start_audio_time_.is_null());

    EnsureInitialized();
    last_audio_time_ = start_audio_time_ = timestamp;

    CHECK(params.IsValid());
    audio_sample_rate_ = params.sample_rate();
    audio_track_index_ = GetNextTrackIndex();

    context_->SetAudioTrack({audio_track_index_.value(),
                             static_cast<uint32_t>(audio_sample_rate_)});

    BuildMovieAudioTrack(params, *encoded_data, std::move(codec_description));
  }
  last_audio_time_ = timestamp;

  AddDataToAudioFragment(std::move(encoded_data));
}

void Mp4MuxerDelegate::BuildMovieAudioTrack(
    const AudioParameters& params,
    const DecoderBuffer& encoded_data,
    std::optional<AudioEncoder::CodecDescription> codec_description) {
  DCHECK(audio_track_index_.has_value());
  DCHECK(codec_description.has_value() || (audio_codec_ == AudioCodec::kOpus));

  // `stsd`, `mp4a`, `esds`, 'opus', 'dops'.
  mp4::writable_boxes::SampleDescription description;
  mp4::writable_boxes::AudioSampleEntry audio_sample_entry(
      audio_codec_, audio_sample_rate_, params.channels());

  if (audio_codec_ == AudioCodec::kAAC) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    mp4::writable_boxes::ElementaryStreamDescriptor
        elementary_stream_descriptor;
    elementary_stream_descriptor.aac_codec_description =
        std::move(codec_description.value());
    audio_sample_entry.elementary_stream_descriptor =
        std::move(elementary_stream_descriptor);
#else
    NOTREACHED_IN_MIGRATION();
#endif
  } else {
    // TODO(crbug.com/40281463): Ensure the below OpusSpecificBox is correct.
    CHECK_EQ(audio_codec_, AudioCodec::kOpus);
    mp4::writable_boxes::OpusSpecificBox opus_specific_box;
    opus_specific_box.channel_count = audio_sample_entry.channel_count;
    opus_specific_box.sample_rate = audio_sample_entry.sample_rate;
    audio_sample_entry.opus_specific_box = std::move(opus_specific_box);
  }

  description.audio_sample_entry = std::move(audio_sample_entry);
  description.entry_count = 1;

  BuildTrack(*moov_, audio_track_index_.value(), true,
             context_->GetAudioTrack().value().timescale, description);

  mp4::writable_boxes::Track& audio_track =
      moov_->tracks[audio_track_index_.value()];

  // `hdlr`
  mp4::writable_boxes::MediaHandler media_handler(/*is_audio=*/true);
  media_handler.name = kAudioHandlerName;
  audio_track.media.handler = std::move(media_handler);
  // `minf`

  // `smhd`
  mp4::writable_boxes::SoundMediaHeader sound_header;
  audio_track.media.information.sound_header = std::move(sound_header);
  DVLOG(1) << __func__ << ", audio track created";
}

void Mp4MuxerDelegate::AddDataToAudioFragment(
    scoped_refptr<DecoderBuffer> encoded_data) {
  DCHECK(audio_track_index_.has_value());
  CreateFragmentIfNeeded(true, false);

  Mp4MuxerDelegateFragment* fragment = fragments_.back().get();
  if (!fragment) {
    DVLOG(1) << __func__ << ", no valid audio fragment exists";
    return;
  }

  fragment->AddAudioData(std::move(encoded_data), last_audio_time_);
  MaybeFlushFileTypeBoxForStartup();
}

bool Mp4MuxerDelegate::FlushFragment() {
  // `live_mode_` is set to true regardless of `Flush()` failure as
  // `FlushFragment()` is called only when it is a live mode and it
  // will be referenced inside `Flush()` for the `mfra` box.
  live_mode_ = true;

  if (!Flush()) {
    DVLOG(1) << __func__
             << "flush fragment failed, it could be the first video frame";
    return false;
  }

  fragments_.clear();
  return true;
}

bool Mp4MuxerDelegate::Flush() {
  if (!video_track_index_.has_value() && !audio_track_index_.has_value()) {
    return false;
  }

  // File type box write will be called at the first frame arrival.
  size_t written_offset = MaybeFlushFileTypeBoxForStartup();

  // Moov box write could be called at the flush whether it is
  // fragment only by live mode, at the end for file mode.
  written_offset += MaybeFlushMoovBox();

  MaybeFlushMoofAndMfraBoxes(written_offset);

  return true;
}

size_t Mp4MuxerDelegate::MaybeFlushFileTypeBoxForStartup() {
  if (written_file_type_box_size_.has_value()) {
    return *written_file_type_box_size_;
  }

  // Build and write `FTYP` box.
  mp4::writable_boxes::FileType mp4_file_type_box(
      /*major_brand=*/mp4::FOURCC_ISOM, 512);
  BuildFileTypeBox(mp4_file_type_box);
  Mp4FileTypeBoxWriter file_type_box_writer(*context_, mp4_file_type_box);
  written_file_type_box_size_ = file_type_box_writer.WriteAndFlush();
  return *written_file_type_box_size_;
}

size_t Mp4MuxerDelegate::MaybeFlushMoovBox() {
  if (written_mov_box_size_.has_value()) {
    return *written_mov_box_size_;
  }

  BuildMovieBox();

  // Log blob info.
  LogBoxInfo();

  // Write `moov` box and its children.
  Mp4MovieBoxWriter movie_box_writer(*context_, *moov_);
  written_mov_box_size_ = movie_box_writer.WriteAndFlush();
  return *written_mov_box_size_;
}

void Mp4MuxerDelegate::MaybeFlushMoofAndMfraBoxes(size_t written_offset) {
  // Update the last sample timestamp for the fragments.
  for (auto& fragment : fragments_) {
    if (video_track_index_.has_value()) {
      CHECK_NE(video_frame_rate_, 0);
      fragment->AddVideoLastTimestamp(
          base::Milliseconds(kMillisecondsTimeScale / video_frame_rate_));
    }

    if (audio_track_index_.has_value()) {
      CHECK_NE(audio_sample_rate_, 0);
      int audio_frame_rate = audio_sample_rate_ / kAudioSamplesPerFrame;
      fragment->AddAudioLastTimestamp(
          base::Milliseconds(kMillisecondsTimeScale / audio_frame_rate));
    }
  }

  mp4::writable_boxes::TrackFragmentRandomAccess video_track_random_access;
  for (auto& fragment : fragments_) {
    if (video_track_index_.has_value() && !live_mode_) {
      base::TimeTicks start_timestamp = fragment->GetVideoStartTimestamp();
      if (start_timestamp.is_null()) {
        start_timestamp = start_video_time_;
      }
      BuildVideoTrackFragmentRandomAccess(
          start_timestamp, video_track_random_access, written_offset);
    }

    fragment->Finalize(start_audio_time_, start_video_time_);

    // `moof` and `mdat` should use same `BoxByteStream` as `moof`
    // has a dependency of `mdat` offset.
    Mp4MovieFragmentBoxWriter fragment_box_writer(*context_,
                                                  fragment->GetMovieFragment());
    BoxByteStream box_byte_stream;
    fragment_box_writer.Write(box_byte_stream);

    // Write `mdat` box with `moof` boxes writer object.
    Mp4MediaDataBoxWriter mdat_box_writer(*context_, fragment->GetMediaData());

    written_offset += mdat_box_writer.WriteAndFlush(box_byte_stream);
  }

  if (live_mode_) {
    // live mode can not have `mfra` box.
    return;
  }

  // Write `mfra` box as a last box for mp4 file.
  if (video_track_index_.has_value()) {
    video_track_random_access.track_id = video_track_index_.value() + 1;

    mp4::writable_boxes::FragmentRandomAccess fragment_random_access;
    mp4::writable_boxes::TrackFragmentRandomAccess audio_random_access;

    // Add audio random access first as it is 0 index by default.
    fragment_random_access.tracks.emplace_back(std::move(audio_random_access));
    fragment_random_access.tracks.emplace_back(
        std::move(video_track_random_access));
    if (video_track_index_.value() == 0) {
      std::swap(fragment_random_access.tracks[kDefaultAudioIndex],
                fragment_random_access.tracks[kDefaultVideoIndex]);
    }

    // Flush at requested.
    Mp4FragmentRandomAccessBoxWriter fragment_random_access_box_writer(
        *context_, fragment_random_access);
    fragment_random_access_box_writer.WriteAndFlush();
  }

  fragments_.clear();
}

void Mp4MuxerDelegate::BuildFileTypeBox(
    mp4::writable_boxes::FileType& mp4_file_type_box) {
  mp4_file_type_box.compatible_brands.emplace_back(mp4::FOURCC_ISOM);
  mp4_file_type_box.compatible_brands.emplace_back(mp4::FOURCC_ISO6);
  mp4_file_type_box.compatible_brands.emplace_back(mp4::FOURCC_ISO2);
  mp4_file_type_box.compatible_brands.emplace_back(mp4::FOURCC_AVC1);
  mp4_file_type_box.compatible_brands.emplace_back(mp4::FOURCC_MP41);
}

void Mp4MuxerDelegate::BuildMovieBox() {
  // It will be called during Flush time.
  moov_->header.creation_time = base::Time::Now();
  moov_->header.modification_time = moov_->header.creation_time;

  // Milliseconds timescale for movie header.
  moov_->header.timescale = kMillisecondsTimeScale;

  // Use inverse video frame rate just in case when it has a single frame.
  base::TimeDelta video_track_duration = base::Seconds(0);
  if (video_track_index_.has_value()) {
    CHECK_NE(video_frame_rate_, 0);
    base::TimeTicks last_video_time_include_last_sample =
        last_video_time_ +
        base::Milliseconds(kMillisecondsTimeScale / video_frame_rate_);
    video_track_duration =
        last_video_time_include_last_sample - start_video_time_;
    CopyCreationTimeAndDuration(moov_->tracks[video_track_index_.value()],
                                moov_->header, video_track_duration);
  }

  // Use inverse audio sample rate just in case when it has a single frame.
  base::TimeDelta audio_track_duration = base::Seconds(0);
  if (audio_track_index_.has_value()) {
    CHECK_NE(audio_sample_rate_, 0);

    int audio_frame_rate = audio_sample_rate_ / kAudioSamplesPerFrame;
    base::TimeTicks last_audio_time_include_last_sample =
        last_audio_time_ +
        base::Milliseconds(kMillisecondsTimeScale / audio_frame_rate);
    audio_track_duration =
        last_audio_time_include_last_sample - start_audio_time_;
    CopyCreationTimeAndDuration(moov_->tracks[audio_track_index_.value()],
                                moov_->header, audio_track_duration);
  }

  if (live_mode_) {
    // live should not have duration.
    return;
  }

  // Update the track's duration that the longest duration on the track
  // whether it is video or audio.
  moov_->header.duration = std::max(video_track_duration, audio_track_duration);

  moov_->header.next_track_id = next_track_index_ + 1;
}

void Mp4MuxerDelegate::BuildVideoTrackFragmentRandomAccess(
    base::TimeTicks start_video_time,
    mp4::writable_boxes::TrackFragmentRandomAccess&
        fragment_random_access_box_writer,
    size_t written_offset) {
  mp4::writable_boxes::TrackFragmentRandomAccessEntry entry;
  // `time` is a duration since its target track's start until the
  // previous fragment, which is presentation time of the track.
  entry.time = start_video_time - start_video_time_;
  entry.moof_offset = written_offset;
  entry.traf_number = 1;
  entry.trun_number = 1;
  entry.sample_number = 1;
  fragment_random_access_box_writer.entries.emplace_back(std::move(entry));
}

void Mp4MuxerDelegate::CreateFragmentIfNeeded(bool audio, bool is_key_frame) {
  CHECK(video_track_index_.has_value() || audio_track_index_.has_value());
  bool is_audio_only = !video_track_index_.has_value();

  // Create new fragment if the key frame is found or number of audio
  // samples exceeds the limit when the video is not found.
  bool create_fragment = fragments_.empty();

  if (!create_fragment && is_key_frame) {
    // It is video key frame that should create new fragment unless
    // it is the first fragment that already exists by the earlier audio
    // samples.
    Mp4MuxerDelegateFragment* fragment = fragments_.back().get();
    create_fragment = fragment->GetVideoSampleSize() >= 1;
  }

  if (!create_fragment && is_audio_only) {
    // The audio only fragment is created when the audio samples exceed the
    // limit.
    Mp4MuxerDelegateFragment* fragment = fragments_.back().get();
    create_fragment =
        fragment->GetAudioSampleSize() >= audio_sample_count_per_fragment_;
  }

  if (!create_fragment) {
    return;
  }

  int video_track_id =
      video_track_index_.has_value() ? *video_track_index_ + 1 : 2;
  int audio_track_id =
      audio_track_index_.has_value() ? *audio_track_index_ + 1 : 2;
  auto new_fragment = std::make_unique<Mp4MuxerDelegateFragment>(
      *context_, video_track_id, audio_track_id, sequence_number_++);

  fragments_.emplace_back(std::move(new_fragment));
}

void Mp4MuxerDelegate::EnsureInitialized() {
  if (context_) {
    return;
  }

  // `write_callback_` continue to be used even after `Reset`.
  auto output_position_tracker =
      std::make_unique<OutputPositionTracker>(write_callback_);

  context_ =
      std::make_unique<Mp4MuxerContext>(std::move(output_position_tracker));

  moov_ = std::make_unique<mp4::writable_boxes::Movie>();

  // We add two tracks to the moov box, one for video and one for audio, but
  // we don't know which is which yet. The correct fields will be filled in
  // when the first video or audio frame is added.
  moov_->tracks.emplace_back(0, false);
  moov_->tracks.emplace_back(0, false);

  moov_->extends.track_extends.emplace_back(
      mp4::writable_boxes::TrackExtends());
  moov_->extends.track_extends.emplace_back(
      mp4::writable_boxes::TrackExtends());
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
scoped_refptr<DecoderBuffer> Mp4MuxerDelegate::ConvertNALUData(
    scoped_refptr<DecoderBuffer> encoded_data) {
  if (!h264_converter_) {
    h264_converter_ =
        std::make_unique<media::H264AnnexBToAvcBitstreamConverter>();
  }

  bool config_changed = false;
  size_t desired_size = 0;
  std::vector<uint8_t> output_chunk;
  auto status = h264_converter_->ConvertChunk(
      encoded_data->AsSpan(), output_chunk, &config_changed, &desired_size);
  CHECK_EQ(status.code(), media::MP4Status::Codes::kBufferTooSmall);
  output_chunk.resize(desired_size);
  status = h264_converter_->ConvertChunk(encoded_data->AsSpan(), output_chunk,
                                         &config_changed, &desired_size);
  CHECK(status.is_ok());

  auto converted_encoded_data = media::DecoderBuffer::CopyFrom(output_chunk);
  return converted_encoded_data;
}
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

int Mp4MuxerDelegate::GetNextTrackIndex() {
  return next_track_index_++;
}

void Mp4MuxerDelegate::LogBoxInfo() const {
  std::ostringstream s;

  s << "movie timescale:" << moov_->header.timescale
    << ", duration in seconds:" << moov_->header.duration.InSeconds();
  if (video_track_index_.has_value()) {
    mp4::writable_boxes::Track& track =
        moov_->tracks[video_track_index_.value()];
    s << ", video track index:" << video_track_index_.value()
      << ", video track timescale:"
      << context_->GetVideoTrack().value().timescale
      << ", video track duration:" << track.header.duration;
  }

  if (audio_track_index_.has_value()) {
    mp4::writable_boxes::Track& track =
        moov_->tracks[audio_track_index_.value()];
    s << ", audio track index:" << audio_track_index_.value()
      << ", audio track timescale:"
      << context_->GetAudioTrack().value().timescale
      << ", audio track duration:" << track.header.duration;
  }
  s << ", Fragment counts:" << fragments_.size();

  DVLOG(1) << __func__ << ", " << s.str();
}

}  // namespace media.
