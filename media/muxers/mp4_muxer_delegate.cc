// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/mp4_muxer_delegate.h"

#include "components/version_info/version_info.h"
#include "media/base/audio_parameters.h"
#include "media/formats/mp4/avc.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/muxers/box_byte_stream.h"
#include "media/muxers/mp4_box_writer.h"
#include "media/muxers/mp4_fragment_box_writer.h"
#include "media/muxers/mp4_movie_box_writer.h"
#include "media/muxers/output_position_tracker.h"

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/formats/mp4/h264_annex_b_to_avc_bitstream_converter.h"
#endif

namespace media {

namespace {

using mp4::writable_boxes::FragmentSampleFlags;
using mp4::writable_boxes::TrackFragmentHeaderFlags;
using mp4::writable_boxes::TrackFragmentRunFlags;

constexpr char kVideoHandlerName[] = "VideoHandler";
constexpr char kUndefinedLanguageName[] = "und";

// Milliseconds time scale is set in the Movie header and it will
// be the base for the duration.
constexpr uint32_t kMillisecondsTimeScale = 1000u;

template <typename T>
uint32_t BuildFlags(const std::vector<T>& build_flags) {
  uint32_t flags = 0;
  for (auto flag : build_flags) {
    flags |= static_cast<uint32_t>(flag);
  }

  return flags;
}

}  // namespace

Mp4MuxerDelegate::Mp4MuxerDelegate(Muxer::WriteDataCB write_callback)
    : write_callback_(std::move(write_callback)) {}

Mp4MuxerDelegate::~Mp4MuxerDelegate() = default;

void Mp4MuxerDelegate::AddVideoFrame(
    const Muxer::VideoParameters& params,
    base::StringPiece encoded_data,
    absl::optional<VideoEncoder::CodecDescription> codec_description,
    base::TimeTicks timestamp,
    bool is_key_frame) {
  if (!video_track_index_.has_value()) {
    CHECK(codec_description.has_value());
    CHECK(is_key_frame);
    CHECK(start_video_time_.is_null());

    EnsureInitialized();
    last_video_time_ = start_video_time_ = timestamp;

    CHECK_GT(params.frame_rate, 0);
    video_frame_rate_ = params.frame_rate;

    video_track_index_ = GetNextTrackIndex();
    context_->SetVideoIndex(video_track_index_.value());

    mp4::writable_boxes::Track track;
    moov_->tracks.emplace_back(std::move(track));

    mp4::writable_boxes::TrackExtends trex;
    moov_->extends.track_extends.emplace_back(std::move(trex));

    BuildVideoTrackWithKeyframe(params, encoded_data,
                                codec_description.value());
  }
  last_video_time_ = timestamp;

  BuildVideoFragment(params, encoded_data, timestamp, is_key_frame);
}

void Mp4MuxerDelegate::AddAudioFrame(
    const AudioParameters& params,
    base::StringPiece encoded_data,
    absl::optional<AudioEncoder::CodecDescription> codec_description,
    base::TimeTicks timestamp) {
  NOTIMPLEMENTED();
}

void Mp4MuxerDelegate::Flush() {
  BuildMovieBox();

  // Write `moov` box and its children.
  Mp4MovieBoxWriter box_writer(*context_, *moov_);
  box_writer.WriteAndFlush();

  AddLastVideoSampleTimestamp();

  // Write `moof` box and its children as well as `mdat` box.
  for (auto& fragment : fragments_) {
    // `moof` and `mdat` should use same `BoxByteStream` as `moof`
    // has a dependency of `mdat` offset.
    Mp4MovieFragmentBoxWriter fragment_box_writer(*context_, fragment->moof);
    BoxByteStream box_byte_stream;
    fragment_box_writer.Write(box_byte_stream);

    // Write `mdat` box with `moof` boxes writer object.
    Mp4MediaDataBoxWriter mdat_box_writer(*context_, fragment->mdat);

    mdat_box_writer.WriteAndFlush(box_byte_stream);
  }

  Reset();
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
}

void Mp4MuxerDelegate::Reset() {
  context_.reset();
  moov_.reset();
  fragments_.clear();

  video_track_index_.reset();
  next_track_index_ = 0;
  start_video_time_ = base::TimeTicks();
  last_video_time_ = base::TimeTicks();
}

void Mp4MuxerDelegate::BuildMovieBox() {
  // It will be called during Flush time.
  moov_->header.creation_time = base::Time::Now();
  moov_->header.modification_time = moov_->header.creation_time;

  // Milliseconds timescale for movie header.
  moov_->header.timescale = kMillisecondsTimeScale;

  base::TimeDelta longest_track_duration;
  if (video_track_index_.has_value()) {
    auto& track = moov_->tracks[*video_track_index_];
    track.header.creation_time = moov_->header.creation_time;
    track.header.modification_time = moov_->header.modification_time;

    track.media.header.creation_time = moov_->header.creation_time;
    track.media.header.modification_time = moov_->header.modification_time;

    // video track duration on the `tkhd` and `mdhd`.

    // Use inverse frame_rate just in case when it has a single frame.
    longest_track_duration = std::max(base::Seconds(1 / video_frame_rate_),
                                      last_video_time_ - start_video_time_);
    track.header.duration = longest_track_duration;
    track.media.header.duration = longest_track_duration;
  }

  // Update the track's duration that the longest duration on the track
  // whether it is video or audio.
  moov_->header.duration = longest_track_duration;

  // next_track_id indicates a value to use for the track ID of the next
  // track to be added to this presentation.
  moov_->header.next_track_id = GetNextTrackIndex() + 1;
}

void Mp4MuxerDelegate::BuildVideoTrackWithKeyframe(
    const Muxer::VideoParameters& params,
    base::StringPiece encoded_data,
    VideoEncoder::CodecDescription codec_description) {
  DCHECK(video_track_index_.has_value());

  mp4::writable_boxes::Track& video_track = moov_->tracks[*video_track_index_];

  // `tkhd`.
  video_track.header.track_id = static_cast<uint32_t>(*video_track_index_ + 1);
  video_track.header.is_audio = false;

  video_track.header.natural_size = params.visible_rect_size;

  // `mdhd`
  video_track.media.header.timescale =
      video_frame_rate_ * kMillisecondsTimeScale;
  video_track.media.header.language =
      kUndefinedLanguageName;  // use 'und' as default at this time.

  // `hdlr`
  video_track.media.handler.handler_type = media::mp4::FOURCC_VIDE;
  video_track.media.handler.name = kVideoHandlerName;

  // `minf`

  // `vmhd`
  mp4::writable_boxes::VideoMediaHeader video_header = {};
  video_track.media.information.video_header = std::move(video_header);

  // `dinf`, `dref`, `url`.
  mp4::writable_boxes::DataInformation data_info;
  mp4::writable_boxes::DataUrlEntry url;
  data_info.data_reference.entries.emplace_back(std::move(url));
  video_track.media.information.data_information = std::move(data_info);

  // `stbl`, `stco`, `stsz`, `stts`, `stsc'.
  mp4::writable_boxes::SampleTable sample_table = {};
  sample_table.sample_to_chunk = mp4::writable_boxes::SampleToChunk{};
  sample_table.decoding_time_to_sample =
      mp4::writable_boxes::DecodingTimeToSample();
  sample_table.sample_size = mp4::writable_boxes::SampleSize();
  sample_table.sample_chunk_offset = mp4::writable_boxes::SampleChunkOffset();

  // `stsd`, `avc1`, `avcC`.
  mp4::writable_boxes::SampleDescription description = {};

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  mp4::writable_boxes::VisualSampleEntry visual_entry = {};

  visual_entry.coded_size = params.visible_rect_size;

  visual_entry.compressor_name = version_info::GetProductName();

  mp4::AVCDecoderConfigurationRecord avc_config;
  bool result =
      avc_config.Parse(codec_description.data(), codec_description.size());
  DCHECK(result);

  visual_entry.avc_decoder_configuration.avc_config_record =
      std::move(avc_config);
  visual_entry.pixel_aspect_ratio = mp4::writable_boxes::PixelAspectRatioBox();
  description.visual_sample_entry = std::move(visual_entry);
#endif

  sample_table.sample_description = std::move(description);
  video_track.media.information.sample_table = std::move(sample_table);

  // `trex`.
  mp4::writable_boxes::TrackExtends& video_extends =
      moov_->extends.track_extends[*video_track_index_];
  video_extends.track_id = *video_track_index_ + 1;

  // TODO(crbug.com/1464063): Various MP4 samples doesn't need
  // default_sample_duration, default_sample_size, default_sample_flags. We need
  // to investigate it further though whether we need to set these fields.
  video_extends.default_sample_description_index = 1;
  video_extends.default_sample_size = 0;
  video_extends.default_sample_duration = base::Milliseconds(0);
  video_extends.default_sample_flags = 0;
}

void Mp4MuxerDelegate::BuildVideoFragment(const Muxer::VideoParameters& params,
                                          base::StringPiece encoded_data,
                                          base::TimeTicks timestamp,
                                          bool is_key_frame) {
  DCHECK(video_track_index_.has_value());
  bool add_new_fragment = false;

  // Create new fragment only when it is key frame && no fragment exists
  // && the previous fragment has an entry.
  if (is_key_frame && ((fragments_.size() == 0 ||
                        (fragments_.back()
                             ->moof.track_fragments[*video_track_index_]
                             .header.track_id != -1u)))) {
    auto fragment = std::make_unique<Fragment>();

    mp4::writable_boxes::TrackFragment track_frag;
    fragment->moof.track_fragments.emplace_back(
        std::move(track_frag));  // video track.

    fragment->mdat.track_data.emplace_back();

    if (!fragments_.empty()) {
      mp4::writable_boxes::TrackFragmentRun& prior_video_trun =
          fragments_.back()->moof.track_fragments[*video_track_index_].run;

      // Add additional timestamp on the previous fragment so that it can
      // get sample duration during box writer for last sample.
      prior_video_trun.sample_timestamps.emplace_back(timestamp);
    }

    fragments_.emplace_back(std::move(fragment));
    add_new_fragment = true;
  }

  Fragment* fragment = fragments_.back().get();
  if (!fragment) {
    // Don't add if the first frame does not have SPS/PPS.
    return;
  }

  if (add_new_fragment) {
    AddNewVideoFragment(*fragment);
  }

  // Add sample.
  mp4::writable_boxes::TrackFragmentRun& video_trun =
      fragment->moof.track_fragments[*video_track_index_].run;

  // Additional entries may exist in various sample vectors, such as durations,
  // hence the use of 'sample_count' to ensure an accurate count of valid
  // samples.
  video_trun.sample_count += 1;

  // Add sample size, which is required.
  video_trun.sample_sizes.emplace_back(encoded_data.size());

  // Add sample timestamp.
  video_trun.sample_timestamps.emplace_back(last_video_time_);

  // Add sample data to the data box.
  AddDataToMdat(*fragment, encoded_data);
}

void Mp4MuxerDelegate::AddNewVideoFragment(
    Mp4MuxerDelegate::Fragment& fragment) {
  // `tfhd` fields.
  fragment.moof.header.sequence_number = fragments_.size();

  mp4::writable_boxes::TrackFragment& video_track_fragment =
      fragment.moof.track_fragments[*video_track_index_];

  // `traf`.
  video_track_fragment.header.track_id = *video_track_index_ + 1;

  std::vector<mp4::writable_boxes::FragmentSampleFlags> sample_flags = {
      FragmentSampleFlags::kSampleFlagIsNonSync,
      FragmentSampleFlags::kSampleFlagDependsYes};
  video_track_fragment.header.default_sample_flags =
      BuildFlags<mp4::writable_boxes::FragmentSampleFlags>(sample_flags);

  video_track_fragment.header.default_sample_duration = base::TimeDelta();
  video_track_fragment.header.default_sample_size = 0;

  std::vector<mp4::writable_boxes::TrackFragmentHeaderFlags>
      fragment_header_flags = {
          TrackFragmentHeaderFlags::kDefaultBaseIsMoof,
          TrackFragmentHeaderFlags::kDefaultSampleDurationPresent,
          TrackFragmentHeaderFlags::kkDefaultSampleFlagsPresent};
  video_track_fragment.header.flags =
      BuildFlags<mp4::writable_boxes::TrackFragmentHeaderFlags>(
          fragment_header_flags);

  // `trun`.
  video_track_fragment.run = {};
  video_track_fragment.run.sample_count = 0;

  std::vector<mp4::writable_boxes::TrackFragmentRunFlags> fragment_run_flags = {
      TrackFragmentRunFlags::kDataOffsetPresent,
      TrackFragmentRunFlags::kFirstSampleFlagsPresent,
      TrackFragmentRunFlags::kSampleDurationPresent,
      TrackFragmentRunFlags::kSampleSizePresent};
  video_track_fragment.run.flags =
      BuildFlags<mp4::writable_boxes::TrackFragmentRunFlags>(
          fragment_run_flags);

  // The first sample in the `trun` uses the `first_sample_flags` and
  // other sample will use `default_sample_flags`.
  video_track_fragment.run.first_sample_flags = static_cast<uint32_t>(
      mp4::writable_boxes::FragmentSampleFlags::kSampleFlagDependsNo);

  // `tfdt`.
  video_track_fragment.decode_time.base_media_decode_time =
      last_video_time_ - start_video_time_;
}

void Mp4MuxerDelegate::AddDataToMdat(Mp4MuxerDelegate::Fragment& fragment,
                                     base::StringPiece encoded_data) {
  DCHECK(video_track_index_.has_value());

  // The parameter sets are supplied in-band at the sync samples.
  // It is a default on encoded stream, see
  // `VideoEncoder::produce_annexb=false`.

  // Copy the data to the mdat.
  // TODO(crbug.com/1458518): We'll want to store the data as a vector of
  // encoded buffers instead of a single block so you don't have to resize
  // a giant blob of memory to hold them all. We should only have one
  // copy into the final muxed output buffer in an ideal world.
  std::vector<uint8_t>& video_data =
      fragment.mdat.track_data[*video_track_index_];
  size_t current_size = video_data.size();
  if (current_size + encoded_data.size() > video_data.capacity()) {
    video_data.reserve((current_size + encoded_data.size()) * 1.5);
  }

  // TODO(crbug.com/1458518): encoded stream needs to be movable container.
  video_data.resize(current_size + encoded_data.size());
  memcpy(&video_data[current_size], encoded_data.data(), encoded_data.size());
}

void Mp4MuxerDelegate::AddLastVideoSampleTimestamp() {
  CHECK(!fragments_.empty());

  // Add last timestamp to the sample_timestamps on the last fragment.
  mp4::writable_boxes::TrackFragmentRun& last_video_trun =
      fragments_.back()->moof.track_fragments[*video_track_index_].run;

  // Use duration based on the frame rate for the last duration of the
  // last fragment.
  base::TimeTicks last_video_timestamp =
      last_video_trun.sample_timestamps.back();

  last_video_trun.sample_timestamps.emplace_back(
      last_video_timestamp +
      base::Milliseconds(kMillisecondsTimeScale / video_frame_rate_));
}

int Mp4MuxerDelegate::GetNextTrackIndex() {
  return next_track_index_++;
}

Mp4MuxerDelegate::Fragment::Fragment() = default;

}  // namespace media.
