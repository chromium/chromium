// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/mp4_stream_parser.h"

#include <stddef.h>

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/numerics/math_constants.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/encryption_pattern.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_tracks.h"
#include "media/base/media_util.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/text_track_config.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_util.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/box_reader.h"
#include "media/formats/mp4/es_descriptor.h"
#include "media/formats/mp4/rcheck.h"
#include "media/formats/mpeg/adts_constants.h"

namespace media {
namespace mp4 {

namespace {

const int kMaxEmptySampleLogs = 20;
const int kMaxInvalidConversionLogs = 20;
const int kMaxVideoKeyframeMismatchLogs = 10;

// Caller should be prepared to handle return of EncryptionScheme::kUnencrypted
// in case of unsupported scheme.
EncryptionScheme GetEncryptionScheme(const ProtectionSchemeInfo& sinf) {
  if (!sinf.HasSupportedScheme())
    return EncryptionScheme::kUnencrypted;
  FourCC fourcc = sinf.type.type;
  switch (fourcc) {
    case FOURCC_CENC:
      return EncryptionScheme::kCenc;
    case FOURCC_CBCS:
      return EncryptionScheme::kCbcs;
    default:
      NOTREACHED();
      break;
  }
  return EncryptionScheme::kUnencrypted;
}
}  // namespace

MP4StreamParser::MP4StreamParser(const std::set<int>& audio_object_types,
                                 bool has_sbr,
                                 bool has_flac)
    : state_(kWaitingForInit),
      moof_head_(0),
      mdat_tail_(0),
      highest_end_offset_(0),
      has_audio_(false),
      has_video_(false),
      audio_object_types_(audio_object_types),
      has_sbr_(has_sbr),
      has_flac_(has_flac),
      num_empty_samples_skipped_(0),
      num_invalid_conversions_(0),
      num_video_keyframe_mismatches_(0) {}

MP4StreamParser::~MP4StreamParser() = default;

void MP4StreamParser::Init(
    InitCB init_cb,
    const NewConfigCB& config_cb,
    const NewBuffersCB& new_buffers_cb,
    bool /* ignore_text_tracks */,
    const EncryptedMediaInitDataCB& encrypted_media_init_data_cb,
    const NewMediaSegmentCB& new_segment_cb,
    const EndMediaSegmentCB& end_of_segment_cb,
    MediaLog* media_log) {
  DCHECK_EQ(state_, kWaitingForInit);
  DCHECK(!init_cb_);
  DCHECK(init_cb);
  DCHECK(config_cb);
  DCHECK(new_buffers_cb);
  DCHECK(encrypted_media_init_data_cb);
  DCHECK(new_segment_cb);
  DCHECK(end_of_segment_cb);

  ChangeState(kParsingBoxes);
  init_cb_ = std::move(init_cb);
  config_cb_ = config_cb;
  new_buffers_cb_ = new_buffers_cb;
  encrypted_media_init_data_cb_ = encrypted_media_init_data_cb;
  new_segment_cb_ = new_segment_cb;
  end_of_segment_cb_ = end_of_segment_cb;
  media_log_ = media_log;
}

void MP4StreamParser::Reset() {
  queue_.Reset();
  runs_.reset();
  moof_head_ = 0;
  mdat_tail_ = 0;
}

void MP4StreamParser::Flush() {
  DCHECK_NE(state_, kWaitingForInit);
  Reset();
  ChangeState(kParsingBoxes);
}

bool MP4StreamParser::GetGenerateTimestampsFlag() const {
  return false;
}

bool MP4StreamParser::Parse(const uint8_t* buf, int size) {
  DCHECK_NE(state_, kWaitingForInit);

  if (state_ == kError)
    return false;

  queue_.Push(buf, size);

  BufferQueueMap buffers;

  // TODO(sandersd): Remove these bools. ParseResult replaced their purpose, but
  // this method needs to be refactored to complete that work.
  bool result = false;
  bool err = false;

  do {
    switch (state_) {
      case kWaitingForInit:
      case kError:
        NOTREACHED();
        return false;

      case kParsingBoxes: {
        ParseResult pr = ParseBox();
        result = pr == ParseResult::kOk;
        err = pr == ParseResult::kError;
        break;
      }

      case kWaitingForSampleData:
        result = HaveEnoughDataToEnqueueSamples();
        if (result)
          ChangeState(kEmittingSamples);
        break;

      case kEmittingSamples: {
        ParseResult pr = EnqueueSample(&buffers);
        result = pr == ParseResult::kOk;
        err = pr == ParseResult::kError;
        if (result) {
          int64_t max_clear = runs_->GetMaxClearOffset() + moof_head_;
          err = !ReadAndDiscardMDATsUntil(max_clear);
        }
        break;
      }
    }
  } while (result && !err);

  if (!err)
    err = !SendAndFlushSamples(&buffers);

  if (err) {
    DLOG(ERROR) << "Error while parsing MP4";
    moov_.reset();
    Reset();
    ChangeState(kError);
    return false;
  }

  return true;
}

ParseResult MP4StreamParser::ParseBox() {
  const uint8_t* buf;
  int size;
  queue_.Peek(&buf, &size);
  if (!size)
    return ParseResult::kNeedMoreData;

  std::unique_ptr<BoxReader> reader;
  ParseResult result =
      BoxReader::ReadTopLevelBox(buf, size, media_log_, &reader);
  if (result != ParseResult::kOk)
    return result;

  DCHECK(reader);
  if (reader->type() == FOURCC_MOOV) {
    if (!ParseMoov(reader.get()))
      return ParseResult::kError;
  } else if (reader->type() == FOURCC_MOOF) {
    moof_head_ = queue_.head();
    if (!ParseMoof(reader.get()))
      return ParseResult::kError;

    // Set up first mdat offset for ReadMDATsUntil().
    mdat_tail_ = queue_.head() + reader->box_size();

    // Return early to avoid evicting 'moof' data from queue. Auxiliary info may
    // be located anywhere in the file, including inside the 'moof' itself.
    // (Since 'default-base-is-moof' is mandated, no data references can come
    // before the head of the 'moof', so keeping this box around is sufficient.)
    return ParseResult::kOk;
  } else {
    // TODO(wolenetz,chcunningham): Enforce more strict adherence to MSE byte
    // stream spec for ftyp and styp. See http://crbug.com/504514.
    DVLOG(2) << "Skipping unrecognized top-level box: "
             << FourCCToString(reader->type());
  }

  queue_.Pop(reader->box_size());
  return ParseResult::kOk;
}

VideoTransformation MP4StreamParser::CalculateRotation(
    const TrackHeader& track,
    const MovieHeader& movie) {
  static_assert(kDisplayMatrixDimension == 9, "Display matrix must be 3x3");
  // 3x3 matrix: [ a b c ]
  //             [ d e f ]
  //             [ x y z ]
  int32_t rotation_matrix[kDisplayMatrixDimension] = {0};

  // Shift values for fixed point multiplications.
  const int32_t shifts[kDisplayMatrixHeight] = {16, 16, 30};

  // Matrix multiplication for
  // track.display_matrix * movie.display_matrix
  // with special consideration taken that entries a-f are 16.16 fixed point
  // decimals and x-z are 2.30 fixed point decimals.
  for (int i = 0; i < kDisplayMatrixWidth; i++) {
    for (int j = 0; j < kDisplayMatrixHeight; j++) {
      for (int e = 0; e < kDisplayMatrixHeight; e++) {
        rotation_matrix[i * kDisplayMatrixHeight + j] +=
            ((int64_t)track.display_matrix[i * kDisplayMatrixHeight + e] *
             movie.display_matrix[e * kDisplayMatrixHeight + j]) >>
            shifts[e];
      }
    }
  }

  int32_t rotation_only[4] = {rotation_matrix[0], rotation_matrix[1],
                              rotation_matrix[3], rotation_matrix[4]};
  return VideoTransformation(rotation_only);
}

bool MP4StreamParser::ParseMoov(BoxReader* reader) {
  moov_.reset(new Movie);
  RCHECK(moov_->Parse(reader));
  runs_.reset();
  audio_track_ids_.clear();
  video_track_ids_.clear();

  has_audio_ = false;
  has_video_ = false;

  std::unique_ptr<MediaTracks> media_tracks(new MediaTracks());
  AudioDecoderConfig audio_config;
  VideoDecoderConfig video_config;
  int detected_audio_track_count = 0;
  int detected_video_track_count = 0;
  int detected_text_track_count = 0;

  for (std::vector<Track>::const_iterator track = moov_->tracks.begin();
       track != moov_->tracks.end(); ++track) {
    const SampleDescription& samp_descr =
        track->media.information.sample_table.description;

    // TODO(wolenetz): When codec reconfigurations are supported, detect and
    // send a codec reconfiguration for fragments using a sample description
    // index different from the previous one. See https://crbug.com/748250.
    size_t desc_idx = 0;
    for (size_t t = 0; t < moov_->extends.tracks.size(); t++) {
      const TrackExtends& trex = moov_->extends.tracks[t];
      if (trex.track_id == track->header.track_id) {
        desc_idx = trex.default_sample_description_index;
        break;
      }
    }
    RCHECK(desc_idx > 0);
    desc_idx -= 1;  // BMFF descriptor index is one-based

    if (track->media.handler.type == kAudio) {
      detected_audio_track_count++;

      RCHECK(!samp_descr.audio_entries.empty());

      // It is not uncommon to find otherwise-valid files with incorrect sample
      // description indices, so we fail gracefully in that case.
      if (desc_idx >= samp_descr.audio_entries.size())
        desc_idx = 0;
      const AudioSampleEntry& entry = samp_descr.audio_entries[desc_idx];

      // For encrypted audio streams entry.format is FOURCC_ENCA and actual
      // format is in entry.sinf.format.format.
      FourCC audio_format = (entry.format == FOURCC_ENCA)
                                ? entry.sinf.format.format
                                : entry.format;

      if (audio_format != FOURCC_OPUS && audio_format != FOURCC_FLAC &&
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
          audio_format != FOURCC_AC3 && audio_format != FOURCC_EAC3 &&
#endif
#if BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)
          audio_format != FOURCC_MHM1 && audio_format != FOURCC_MHA1 &&
#endif
          audio_format != FOURCC_MP4A) {
        MEDIA_LOG(ERROR, media_log_)
            << "Unsupported audio format 0x" << std::hex << entry.format
            << " in stsd box.";
        return false;
      }

      AudioCodec codec = kUnknownAudioCodec;
      ChannelLayout channel_layout = CHANNEL_LAYOUT_NONE;
      int sample_per_second = 0;
      int codec_delay_in_frames = 0;
      base::TimeDelta seek_preroll;
      std::vector<uint8_t> extra_data;
      if (audio_format == FOURCC_OPUS) {
        codec = kCodecOpus;
        channel_layout = GuessChannelLayout(entry.dops.channel_count);
        sample_per_second = entry.dops.sample_rate;
        codec_delay_in_frames = entry.dops.codec_delay_in_frames;
        seek_preroll = entry.dops.seek_preroll;
        extra_data = entry.dops.extradata;
      } else if (audio_format == FOURCC_FLAC) {
        // FLAC-in-ISOBMFF does not use object type indication. |audio_format|
        // is sufficient for identifying FLAC codec.
        if (!has_flac_) {
          MEDIA_LOG(ERROR, media_log_) << "FLAC audio stream detected in MP4, "
                                          "mismatching what is specified in "
                                          "the mimetype.";
          return false;
        }

        codec = kCodecFLAC;
        channel_layout = GuessChannelLayout(entry.channelcount);
        sample_per_second = entry.samplerate;
        extra_data = entry.dfla.stream_info;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)
      } else if (audio_format == FOURCC_MHM1 || audio_format == FOURCC_MHA1) {
        codec = kCodecMpegHAudio;
        channel_layout = CHANNEL_LAYOUT_BITSTREAM;
        sample_per_second = entry.samplerate;
        extra_data = entry.dfla.stream_info;
#endif
      } else {
        uint8_t audio_type = entry.esds.object_type;
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
        if (audio_type == kForbidden) {
          if (audio_format == FOURCC_AC3)
            audio_type = kAC3;
          if (audio_format == FOURCC_EAC3)
            audio_type = kEAC3;
        }
#endif
        DVLOG(1) << "audio_type 0x" << std::hex << static_cast<int>(audio_type);
        if (audio_object_types_.find(audio_type) == audio_object_types_.end()) {
          MEDIA_LOG(ERROR, media_log_)
              << "audio object type 0x" << std::hex
              << static_cast<int>(audio_type)
              << " does not match what is specified in the mimetype.";
          return false;
        }

        // Check if it is MPEG4 AAC defined in ISO 14496 Part 3 or
        // supported MPEG2 AAC varients.
        if (ESDescriptor::IsAAC(audio_type)) {
          const AAC& aac = entry.esds.aac;
          codec = kCodecAAC;
          channel_layout = aac.GetChannelLayout(has_sbr_);
          sample_per_second = aac.GetOutputSamplesPerSecond(has_sbr_);
#if defined(OS_ANDROID)
          extra_data = aac.codec_specific_data();
#endif
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
        } else if (audio_type == kAC3) {
          codec = kCodecAC3;
          channel_layout = GuessChannelLayout(entry.channelcount);
          sample_per_second = entry.samplerate;
        } else if (audio_type == kEAC3) {
          codec = kCodecEAC3;
          channel_layout = GuessChannelLayout(entry.channelcount);
          sample_per_second = entry.samplerate;
#endif
        } else {
          MEDIA_LOG(ERROR, media_log_)
              << "Unsupported audio object type 0x" << std::hex
              << static_cast<int>(audio_type) << " in esds.";
          return false;
        }
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
      }

      SampleFormat sample_format;
      if (entry.samplesize == 8) {
        sample_format = kSampleFormatU8;
      } else if (entry.samplesize == 16) {
        sample_format = kSampleFormatS16;
      } else if (entry.samplesize == 24) {
        sample_format = kSampleFormatS24;
      } else if (entry.samplesize == 32) {
        sample_format = kSampleFormatS32;
      } else {
        LOG(ERROR) << "Unsupported sample size.";
        return false;
      }

      uint32_t audio_track_id = track->header.track_id;
      if (audio_track_ids_.find(audio_track_id) != audio_track_ids_.end()) {
        MEDIA_LOG(ERROR, media_log_)
            << "Audio track with track_id=" << audio_track_id
            << " already present.";
        return false;
      }
      bool is_track_encrypted = entry.sinf.info.track_encryption.is_encrypted;
      EncryptionScheme scheme = EncryptionScheme::kUnencrypted;
      if (is_track_encrypted) {
        scheme = GetEncryptionScheme(entry.sinf);
        if (scheme == EncryptionScheme::kUnencrypted)
          return false;
      }
      audio_config.Initialize(codec, sample_format, channel_layout,
                              sample_per_second, extra_data, scheme,
                              seek_preroll, codec_delay_in_frames);
      if (codec == kCodecAAC)
        audio_config.disable_discard_decoder_delay();

      DVLOG(1) << "audio_track_id=" << audio_track_id
               << " config=" << audio_config.AsHumanReadableString();
      if (!audio_config.IsValidConfig()) {
        MEDIA_LOG(ERROR, media_log_) << "Invalid audio decoder config: "
                                     << audio_config.AsHumanReadableString();
        return false;
      }
      has_audio_ = true;
      audio_track_ids_.insert(audio_track_id);
      const char* track_kind = (audio_track_ids_.size() == 1 ? "main" : "");
      media_tracks->AddAudioTrack(
          audio_config, audio_track_id, MediaTrack::Kind(track_kind),
          MediaTrack::Label(track->media.handler.name),
          MediaTrack::Language(track->media.header.language()));
      continue;
    }

    if (track->media.handler.type == kVideo) {
      detected_video_track_count++;

      RCHECK(!samp_descr.video_entries.empty());
      if (desc_idx >= samp_descr.video_entries.size())
        desc_idx = 0;
      const VideoSampleEntry& entry = samp_descr.video_entries[desc_idx];

      if (!entry.IsFormatValid()) {
        MEDIA_LOG(ERROR, media_log_) << "Unsupported video format 0x"
                                     << std::hex << entry.format
                                     << " in stsd box.";
        return false;
      }

      // TODO(strobe): Recover correct crop box
      gfx::Size coded_size(entry.width, entry.height);
      gfx::Rect visible_rect(coded_size);

      // If PASP is available, use the coded size and PASP to calculate the
      // natural size. Otherwise, use the size in track header for natural size.
      gfx::Size natural_size(visible_rect.size());
      if (entry.pixel_aspect.h_spacing != 1 ||
          entry.pixel_aspect.v_spacing != 1) {
        natural_size =
            GetNaturalSize(visible_rect.size(), entry.pixel_aspect.h_spacing,
                           entry.pixel_aspect.v_spacing);
      } else if (track->header.width && track->header.height) {
        natural_size =
            gfx::Size(track->header.width, track->header.height);
      }

      uint32_t video_track_id = track->header.track_id;
      if (video_track_ids_.find(video_track_id) != video_track_ids_.end()) {
        MEDIA_LOG(ERROR, media_log_)
            << "Video track with track_id=" << video_track_id
            << " already present.";
        return false;
      }
      bool is_track_encrypted = entry.sinf.info.track_encryption.is_encrypted;
      EncryptionScheme scheme = EncryptionScheme::kUnencrypted;
      if (is_track_encrypted) {
        scheme = GetEncryptionScheme(entry.sinf);
        if (scheme == EncryptionScheme::kUnencrypted)
          return false;
      }
      video_config.Initialize(entry.video_codec, entry.video_codec_profile,
                              VideoDecoderConfig::AlphaMode::kIsOpaque,
                              VideoColorSpace::REC709(),
                              CalculateRotation(track->header, moov_->header),
                              coded_size, visible_rect, natural_size,
                              // No decoder-specific buffer needed for AVC;
                              // SPS/PPS are embedded in the video stream
                              EmptyExtraData(), scheme);
      DVLOG(1) << "video_track_id=" << video_track_id
               << " config=" << video_config.AsHumanReadableString();
      if (!video_config.IsValidConfig()) {
        MEDIA_LOG(ERROR, media_log_) << "Invalid video decoder config: "
                                     << video_config.AsHumanReadableString();
        return false;
      }
      has_video_ = true;
      video_track_ids_.insert(video_track_id);
      auto track_kind =
          MediaTrack::Kind(video_track_ids_.size() == 1 ? "main" : "");
      media_tracks->AddVideoTrack(
          video_config, video_track_id, track_kind,
          MediaTrack::Label(track->media.handler.name),
          MediaTrack::Language(track->media.header.language()));
      continue;
    }

    // TODO(wolenetz): Investigate support in MSE and Chrome MSE for CEA 608/708
    // embedded caption data in video track. At time of init segment parsing, we
    // don't have this data (unless maybe by SourceBuffer's mimetype).
    // See https://crbug.com/597073
    if (track->media.handler.type == kText)
      detected_text_track_count++;
  }

  if (!moov_->pssh.empty())
    OnEncryptedMediaInitData(moov_->pssh);

  RCHECK(config_cb_.Run(std::move(media_tracks), TextTrackConfigMap()));

  StreamParser::InitParameters params(kInfiniteDuration);
  if (moov_->extends.header.fragment_duration > 0) {
    params.duration = TimeDeltaFromRational(
        moov_->extends.header.fragment_duration, moov_->header.timescale);
    if (params.duration == kNoTimestamp) {
      MEDIA_LOG(ERROR, media_log_) << "Fragment duration exceeds representable "
                                   << "limit";
      return false;
    }
    params.liveness = DemuxerStream::LIVENESS_RECORDED;
  } else if (moov_->header.duration > 0 &&
             ((moov_->header.version == 0 &&
               moov_->header.duration !=
                   std::numeric_limits<uint32_t>::max()) ||
              (moov_->header.version == 1 &&
               moov_->header.duration !=
                   std::numeric_limits<uint64_t>::max()))) {
    // In ISO/IEC 14496-12:2012, 8.2.2.3: "If the duration cannot be determined
    // then duration is set to all 1s."
    // The duration field is either 32-bit or 64-bit depending on the version in
    // MovieHeaderBox. We interpret not 0 and not all 1's here as "known
    // duration".
    params.duration =
        TimeDeltaFromRational(moov_->header.duration, moov_->header.timescale);
    if (params.duration == kNoTimestamp) {
      MEDIA_LOG(ERROR, media_log_) << "Movie duration exceeds representable "
                                   << "limit";
      return false;
    }
    params.liveness = DemuxerStream::LIVENESS_RECORDED;
  } else {
    // In ISO/IEC 14496-12:2005(E), 8.30.2: ".. If an MP4 file is created in
    // real-time, such as used in live streaming, it is not likely that the
    // fragment_duration is known in advance and this (mehd) box may be
    // omitted."

    // We have an unknown duration (neither any mvex fragment_duration nor moov
    // duration value indicated a known duration, above.)

    // TODO(wolenetz): Investigate gating liveness detection on timeline_offset
    // when it's populated. See http://crbug.com/312699
    params.liveness = DemuxerStream::LIVENESS_LIVE;
  }

  DVLOG(1) << "liveness: " << params.liveness;

  if (init_cb_) {
    params.detected_audio_track_count = detected_audio_track_count;
    params.detected_video_track_count = detected_video_track_count;
    params.detected_text_track_count = detected_text_track_count;
    std::move(init_cb_).Run(params);
  }

  return true;
}

bool MP4StreamParser::ParseMoof(BoxReader* reader) {
  RCHECK(moov_.get());  // Must already have initialization segment
  MovieFragment moof;
  RCHECK(moof.Parse(reader));
  if (!runs_)
    runs_.reset(new TrackRunIterator(moov_.get(), media_log_));
  RCHECK(runs_->Init(moof));
  RCHECK(ComputeHighestEndOffset(moof));

  if (!moof.pssh.empty())
    OnEncryptedMediaInitData(moof.pssh);

  new_segment_cb_.Run();
  ChangeState(kWaitingForSampleData);
  return true;
}

void MP4StreamParser::OnEncryptedMediaInitData(
    const std::vector<ProtectionSystemSpecificHeader>& headers) {
  // TODO(strobe): ensure that the value of init_data (all PSSH headers
  // concatenated in arbitrary order) matches the EME spec.
  // See https://www.w3.org/Bugs/Public/show_bug.cgi?id=17673.
  size_t total_size = 0;
  for (size_t i = 0; i < headers.size(); i++)
    total_size += headers[i].raw_box.size();

  std::vector<uint8_t> init_data(total_size);
  size_t pos = 0;
  for (size_t i = 0; i < headers.size(); i++) {
    memcpy(&init_data[pos], &headers[i].raw_box[0],
           headers[i].raw_box.size());
    pos += headers[i].raw_box.size();
  }
  encrypted_media_init_data_cb_.Run(EmeInitDataType::CENC, init_data);
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
bool MP4StreamParser::PrepareAACBuffer(
    const AAC& aac_config,
    std::vector<uint8_t>* frame_buf,
    std::vector<SubsampleEntry>* subsamples) const {
  // Append an ADTS header to every audio sample.
  RCHECK(aac_config.ConvertEsdsToADTS(frame_buf));

  // As above, adjust subsample information to account for the headers. AAC is
  // not required to use subsample encryption, so we may need to add an entry.
  if (subsamples->empty()) {
    subsamples->push_back(SubsampleEntry(
        kADTSHeaderMinSize, frame_buf->size() - kADTSHeaderMinSize));
  } else {
    (*subsamples)[0].clear_bytes += kADTSHeaderMinSize;
  }
  return true;
}
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

ParseResult MP4StreamParser::EnqueueSample(BufferQueueMap* buffers) {
  DCHECK_EQ(state_, kEmittingSamples);

  if (!runs_->IsRunValid()) {
    // Flush any buffers we've gotten in this chunk so that buffers don't
    // cross |new_segment_cb_| calls
    if (!SendAndFlushSamples(buffers))
      return ParseResult::kError;

    // Remain in kEmittingSamples state, discarding data, until the end of
    // the current 'mdat' box has been appended to the queue.
    // TODO(sandersd): As I understand it, this Trim() will always succeed,
    // since |mdat_tail_| is never outside of the queue. It's also plausible
    // that this Trim() is always a no-op, but perhaps if all runs are empty
    // this still does something?
    if (!queue_.Trim(mdat_tail_))
      return ParseResult::kNeedMoreData;

    ChangeState(kParsingBoxes);
    end_of_segment_cb_.Run();
    return ParseResult::kOk;
  }

  if (!runs_->IsSampleValid()) {
    if (!runs_->AdvanceRun())
      return ParseResult::kError;
    return ParseResult::kOk;
  }

  const uint8_t* buf;
  int buf_size;
  queue_.Peek(&buf, &buf_size);
  if (!buf_size)
    return ParseResult::kNeedMoreData;

  bool audio =
      audio_track_ids_.find(runs_->track_id()) != audio_track_ids_.end();
  bool video =
      video_track_ids_.find(runs_->track_id()) != video_track_ids_.end();

  // Skip this entire track if it's not one we're interested in
  if (!audio && !video) {
    if (!runs_->AdvanceRun())
      return ParseResult::kError;
    return ParseResult::kOk;
  }

  // Attempt to cache the auxiliary information first. Aux info is usually
  // placed in a contiguous block before the sample data, rather than being
  // interleaved. If we didn't cache it, this would require that we retain the
  // start of the segment buffer while reading samples. Aux info is typically
  // quite small compared to sample data, so this pattern is useful on
  // memory-constrained devices where the source buffer consumes a substantial
  // portion of the total system memory.
  if (runs_->AuxInfoNeedsToBeCached()) {
    queue_.PeekAt(runs_->aux_info_offset() + moof_head_, &buf, &buf_size);
    if (buf_size < runs_->aux_info_size())
      return ParseResult::kNeedMoreData;
    if (!runs_->CacheAuxInfo(buf, buf_size))
      return ParseResult::kError;
    return ParseResult::kOk;
  }

  queue_.PeekAt(runs_->sample_offset() + moof_head_, &buf, &buf_size);

  if (runs_->sample_size() >
      static_cast<uint32_t>(std::numeric_limits<int>::max())) {
    MEDIA_LOG(ERROR, media_log_) << "Sample size is too large";
    return ParseResult::kError;
  }

  int sample_size = base::checked_cast<int>(runs_->sample_size());

  if (buf_size < sample_size)
    return ParseResult::kNeedMoreData;

  if (sample_size == 0) {
    // Generally not expected, but spec allows it. Code below this block assumes
    // the current sample is not empty.
    LIMITED_MEDIA_LOG(DEBUG, media_log_, num_empty_samples_skipped_,
                      kMaxEmptySampleLogs)
        << "Skipping 'trun' sample with size of 0.";
    if (!runs_->AdvanceSample())
      return ParseResult::kError;
    return ParseResult::kOk;
  }

  std::unique_ptr<DecryptConfig> decrypt_config;
  std::vector<SubsampleEntry> subsamples;
  if (runs_->is_encrypted()) {
    decrypt_config = runs_->GetDecryptConfig();
    if (!decrypt_config)
      return ParseResult::kError;
    subsamples = decrypt_config->subsamples();
  }

  // This may change if analysis results indicate runs_->is_keyframe() is
  // opposite of what the coded frame contains.
  bool is_keyframe = runs_->is_keyframe();

  std::vector<uint8_t> frame_buf(buf, buf + sample_size);
  if (video) {
    if (runs_->video_description().video_codec == kCodecH264 ||
        runs_->video_description().video_codec == kCodecHEVC ||
        runs_->video_description().video_codec == kCodecDolbyVision) {
      DCHECK(runs_->video_description().frame_bitstream_converter);
      BitstreamConverter::AnalysisResult analysis;
      if (!runs_->video_description()
               .frame_bitstream_converter->ConvertAndAnalyzeFrame(
                   &frame_buf, is_keyframe, &subsamples, &analysis)) {
        MEDIA_LOG(ERROR, media_log_)
            << "Failed to prepare video sample for decode";
        return ParseResult::kError;
      }

      // If conformance analysis was not actually performed, assume the frame is
      // conformant.  If it was performed and found to be non-conformant, log
      // it.
      if (!analysis.is_conformant.value_or(true)) {
        LIMITED_MEDIA_LOG(DEBUG, media_log_, num_invalid_conversions_,
                          kMaxInvalidConversionLogs)
            << "Prepared video sample is not conformant";
      }

      // Use |analysis.is_keyframe|, if it was actually determined, for logging
      // if the analysis mismatches the container's keyframe metadata for
      // |frame_buf|.
      if (analysis.is_keyframe.has_value() &&
          is_keyframe != analysis.is_keyframe.value()) {
        LIMITED_MEDIA_LOG(DEBUG, media_log_, num_video_keyframe_mismatches_,
                          kMaxVideoKeyframeMismatchLogs)
            << "ISO-BMFF container metadata for video frame indicates that the "
               "frame is "
            << (is_keyframe ? "" : "not ")
            << "a keyframe, but the video frame contents indicate the "
               "opposite.";
        // As of September 2018, it appears that all of Edge, Firefox, Safari
        // work with content that marks non-avc-keyframes as a keyframe in the
        // container. Encoders/muxers/old streams still exist that produce
        // all-keyframe mp4 video tracks, though many of the coded frames are
        // not keyframes (likely workaround due to the impact on low-latency
        // live streams until https://crbug.com/229412 was fixed).  We'll trust
        // the AVC frame's keyframe-ness over the mp4 container's metadata if
        // they mismatch. If other out-of-order codecs in mp4 (e.g. HEVC, DV)
        // implement keyframe analysis in their frame_bitstream_converter, we'll
        // similarly trust that analysis instead of the mp4.
        is_keyframe = analysis.is_keyframe.value();
      }
    }
  }

  if (audio) {
    if (ESDescriptor::IsAAC(runs_->audio_description().esds.object_type)) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      if (!PrepareAACBuffer(runs_->audio_description().esds.aac, &frame_buf,
                            &subsamples)) {
        MEDIA_LOG(ERROR, media_log_)
            << "Failed to prepare AAC sample for decode";
        return ParseResult::kError;
      }
#else
      return ParseResult::kError;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
    }
  }

  if (decrypt_config) {
    if (!subsamples.empty()) {
      // Create a new config with the updated subsamples.
      decrypt_config.reset(
          new DecryptConfig(decrypt_config->encryption_scheme(),
                            decrypt_config->key_id(), decrypt_config->iv(),
                            subsamples, decrypt_config->encryption_pattern()));
    }
    // else, use the existing config.
  }

  StreamParserBuffer::Type buffer_type = audio ? DemuxerStream::AUDIO :
      DemuxerStream::VIDEO;

  scoped_refptr<StreamParserBuffer> stream_buf =
      StreamParserBuffer::CopyFrom(&frame_buf[0], frame_buf.size(), is_keyframe,
                                   buffer_type, runs_->track_id());

  if (decrypt_config)
    stream_buf->set_decrypt_config(std::move(decrypt_config));

  if (runs_->duration() != kNoTimestamp) {
    stream_buf->set_duration(runs_->duration());
  } else {
    MEDIA_LOG(ERROR, media_log_) << "Frame duration exceeds representable "
                                 << "limit";
    return ParseResult::kError;
  }

  if (runs_->cts() != kNoTimestamp) {
    stream_buf->set_timestamp(runs_->cts());
  } else {
    MEDIA_LOG(ERROR, media_log_) << "Frame PTS exceeds representable limit";
    return ParseResult::kError;
  }

  if (runs_->dts() != kNoDecodeTimestamp()) {
    stream_buf->SetDecodeTimestamp(runs_->dts());
  } else {
    MEDIA_LOG(ERROR, media_log_) << "Frame DTS exceeds representable limit";
    return ParseResult::kError;
  }

  DVLOG(3) << "Emit " << (audio ? "audio" : "video") << " frame: "
           << " track_id=" << runs_->track_id() << ", key=" << is_keyframe
           << ", dur=" << runs_->duration().InMilliseconds()
           << ", dts=" << runs_->dts().InMilliseconds()
           << ", cts=" << runs_->cts().InMilliseconds()
           << ", size=" << sample_size;

  (*buffers)[runs_->track_id()].push_back(stream_buf);
  if (!runs_->AdvanceSample())
    return ParseResult::kError;
  return ParseResult::kOk;
}

bool MP4StreamParser::SendAndFlushSamples(BufferQueueMap* buffers) {
  if (buffers->empty())
    return true;
  bool success = new_buffers_cb_.Run(*buffers);
  buffers->clear();
  return success;
}

bool MP4StreamParser::ReadAndDiscardMDATsUntil(int64_t max_clear_offset) {
  ParseResult result = ParseResult::kOk;
  int64_t upper_bound = std::min(max_clear_offset, queue_.tail());
  while (mdat_tail_ < upper_bound) {
    const uint8_t* buf = NULL;
    int size = 0;
    queue_.PeekAt(mdat_tail_, &buf, &size);

    FourCC type;
    size_t box_sz;
    result = BoxReader::StartTopLevelBox(buf, size, media_log_, &type, &box_sz);
    if (result != ParseResult::kOk)
      break;

    if (type != FOURCC_MDAT) {
      MEDIA_LOG(DEBUG, media_log_)
          << "Unexpected box type while parsing MDATs: "
          << FourCCToString(type);
    }
    // TODO(chcunningham): Fix mdat_tail_ and ByteQueue classes to use size_t.
    // TODO(sandersd): The whole |mdat_tail_| mechanism appears to be pointless
    // because StartTopLevelBox() only succeeds for complete boxes. Either
    // remove |mdat_tail_| throughout this class or implement the ability to
    // discard partial mdats.
    mdat_tail_ += base::checked_cast<int64_t>(box_sz);
  }
  queue_.Trim(std::min(mdat_tail_, upper_bound));
  return result != ParseResult::kError;
}

void MP4StreamParser::ChangeState(State new_state) {
  DVLOG(2) << "Changing state: " << new_state;
  state_ = new_state;
}

bool MP4StreamParser::HaveEnoughDataToEnqueueSamples() {
  DCHECK_EQ(state_, kWaitingForSampleData);
  // For muxed content, make sure we have data up to |highest_end_offset_|
  // so we can ensure proper enqueuing behavior. Otherwise assume we have enough
  // data and allow per sample offset checks to meter sample enqueuing.
  // TODO(acolwell): Fix trun box handling so we don't have to special case
  // muxed content.
  return !(has_audio_ && has_video_ &&
           queue_.tail() < highest_end_offset_ + moof_head_);
}

bool MP4StreamParser::ComputeHighestEndOffset(const MovieFragment& moof) {
  highest_end_offset_ = 0;

  TrackRunIterator runs(moov_.get(), media_log_);
  RCHECK(runs.Init(moof));

  while (runs.IsRunValid()) {
    int64_t aux_info_end_offset = runs.aux_info_offset() + runs.aux_info_size();
    if (aux_info_end_offset > highest_end_offset_)
      highest_end_offset_ = aux_info_end_offset;

    while (runs.IsSampleValid()) {
      int64_t sample_end_offset = runs.sample_offset() + runs.sample_size();
      if (sample_end_offset > highest_end_offset_)
        highest_end_offset_ = sample_end_offset;
      if (!runs.AdvanceSample())
        return false;
    }
    if (!runs.AdvanceRun())
      return false;
  }

  return true;
}

}  // namespace mp4
}  // namespace media
