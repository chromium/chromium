// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/mp4/track_run_iterator.h"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "build/chromecast_buildflags.h"
#include "media/base/decrypt_config.h"
#include "media/base/demuxer.h"
#include "media/base/demuxer_memory_limit.h"
#include "media/base/encryption_pattern.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_util.h"
#include "media/base/timestamp_constants.h"
#include "media/formats/mp4/rcheck.h"
#include "media/formats/mp4/sample_to_group_iterator.h"
#include "media/media_buildflags.h"

namespace media {
namespace mp4 {

struct SampleInfo {
  uint32_t size;
  uint32_t duration;
  int64_t cts_offset;
  bool is_keyframe;
  uint32_t cenc_group_description_index;
};

struct TrackRunInfo {
  uint32_t track_id;
  std::vector<SampleInfo> samples;
  int64_t timescale;
  int64_t start_dts;
  int64_t sample_start_offset;

  bool is_audio;
  raw_ptr<const AudioSampleEntry, DanglingUntriaged> audio_description;
  raw_ptr<const VideoSampleEntry, DanglingUntriaged> video_description;
  raw_ptr<const SampleGroupDescription, DanglingUntriaged>
      track_sample_encryption_group;

  // Stores sample encryption entries, which is populated from 'senc' box if it
  // is available, otherwise will try to load from cenc auxiliary information.
  std::vector<SampleEncryptionEntry> sample_encryption_entries;

  // These variables are useful to load |sample_encryption_entries| from cenc
  // auxiliary information when 'senc' box is not available.
  int64_t aux_info_start_offset;  // Only valid if aux_info_total_size > 0.
  int aux_info_default_size;
  std::vector<uint8_t> aux_info_sizes;  // Populated if default_size == 0.
  int aux_info_total_size;

  EncryptionScheme encryption_scheme = EncryptionScheme::kUnencrypted;
  EncryptionPattern encryption_pattern;

  std::vector<CencSampleEncryptionInfoEntry> fragment_sample_encryption_info;

  TrackRunInfo();
  ~TrackRunInfo();
};

TrackRunInfo::TrackRunInfo()
    : track_id(0),
      timescale(-1),
      start_dts(-1),
      sample_start_offset(-1),
      is_audio(false),
      aux_info_start_offset(-1),
      aux_info_default_size(-1),
      aux_info_total_size(-1) {
}
TrackRunInfo::~TrackRunInfo() = default;

base::TimeDelta TimeDeltaFromRational(int64_t numer, int64_t denom) {
  // TODO(sandersd): Change all callers to pass a |denom| as a uint32_t. This is
  // the correct (and sufficient) type in all cases, but some intermediaries
  // currently store -1 as a default value.
  // TODO(sandersd): Change all callers to pass |numer| as a uint64_t. The few
  // cases that could theoretically be negative would result in negative PTS
  // anyway, and there are cases where an int64_t is not sufficient to store the
  // entire representable range.
  DCHECK_GT(denom, 0);
  DCHECK_LE(denom, std::numeric_limits<uint32_t>::max());

  // The maximum number of seconds that a TimeDelta can hold (about 300,000
  // years worth). There is a (t ~= 0.775)-second fraction that is ignored.
  const int64_t max_seconds =
      std::numeric_limits<int64_t>::max() / base::Time::kMicrosecondsPerSecond;

  // The integer part of the result, in seconds. There is a (0 <= f < 1)-second
  // fraction that is not computed. (Also true for negative |numer|, since
  // rounding of integer division is towards zero in C++.)
  const int64_t result_seconds = numer / denom;

  // Reject |actual_seconds == max_seconds| under the assumption that f > t.
  // This rejects valid times that are within t seconds of the limit.
  if (result_seconds >= max_seconds || result_seconds <= -max_seconds)
    return kNoTimestamp;

  // Since (denom <= 2 ** 32), the multiplication fits in 52 bits.
  // Note: When |numer| is negative, (numer % denom) is also negative. C++
  // guarantees that ((numer / denom) * denom + (numer % denom) == numer).
  // TODO(sandersd): Is round-toward-zero the best possible computation here?
  const int64_t result_microseconds =
      base::Time::kMicrosecondsPerSecond * (numer % denom) / denom;

  const int64_t total_microseconds =
      base::Time::kMicrosecondsPerSecond * result_seconds + result_microseconds;
  return base::Microseconds(total_microseconds);
}

DecodeTimestamp DecodeTimestampFromRational(int64_t numer, int64_t denom) {
  return DecodeTimestamp::FromPresentationTime(
      TimeDeltaFromRational(numer, denom));
}

TrackRunIterator::TrackRunIterator(const Movie* moov, MediaLog* media_log)
    : moov_(moov),
      media_log_(media_log),
      sample_dts_(0),
      sample_cts_(0),
      sample_offset_(0) {
  CHECK(moov);
}

TrackRunIterator::~TrackRunIterator() = default;

static std::string HexFlags(uint32_t flags) {
  std::stringstream stream;
  stream << std::setfill('0') << std::setw(sizeof(flags)*2) << std::hex
         << flags;
  return stream.str();
}

static bool PopulateSampleInfo(const TrackExtends& trex,
                               const TrackFragmentHeader& tfhd,
                               const TrackFragmentRun& trun,
                               const int64_t edit_list_offset,
                               const uint32_t i,
                               SampleInfo* sample_info,
                               const SampleDependsOn sdtp_sample_depends_on,
                               bool is_audio,
                               MediaLog* media_log) {
  if (i < trun.sample_sizes.size()) {
    sample_info->size = trun.sample_sizes[i];
  } else if (tfhd.default_sample_size > 0) {
    sample_info->size = tfhd.default_sample_size;
  } else {
    sample_info->size = trex.default_sample_size;
  }

  if (i < trun.sample_durations.size()) {
    sample_info->duration = trun.sample_durations[i];
  } else if (tfhd.default_sample_duration > 0) {
    sample_info->duration = tfhd.default_sample_duration;
  } else {
    sample_info->duration = trex.default_sample_duration;
  }

  auto cts_offset = -base::CheckedNumeric<int64_t>(edit_list_offset);
  if (i < trun.sample_composition_time_offsets.size())
    cts_offset += trun.sample_composition_time_offsets[i];
  if (!cts_offset.AssignIfValid(&sample_info->cts_offset)) {
    MEDIA_LOG(ERROR, media_log) << "PTS offset exceeds representable range.";
    return false;
  }

  uint32_t flags;
  if (i < trun.sample_flags.size()) {
    flags = trun.sample_flags[i];
    DVLOG(4) << __func__ << " trun sample flags " << HexFlags(flags);
  } else if (tfhd.has_default_sample_flags) {
    flags = tfhd.default_sample_flags;
    DVLOG(4) << __func__ << " tfhd sample flags " << HexFlags(flags);
  } else {
    flags = trex.default_sample_flags;
    DVLOG(4) << __func__ << " trex sample flags " << HexFlags(flags);
  }

  SampleDependsOn sample_depends_on =
      static_cast<SampleDependsOn>((flags >> 24) & 0x3);
  if (sample_depends_on == kSampleDependsOnUnknown) {
    sample_depends_on = sdtp_sample_depends_on;
  }
  DVLOG(4) << __func__ << " sample_depends_on " << sample_depends_on;
  if (sample_depends_on == kSampleDependsOnReserved) {
    MEDIA_LOG(ERROR, media_log) << "Reserved value used in sample dependency"
                                   " info.";
    return false;
  }

  // Per spec (ISO 14496-12:2012), the definition for a "sync sample" is
  // equivalent to the downstream code's "is keyframe" concept. But media exists
  // that marks non-key video frames as sync samples (http://crbug.com/507916
  // and http://crbug.com/310712). Hence, for video we additionally check that
  // the sample does not depend on others (FFmpeg does too, see mov_read_trun).
  // Sample dependency is ignored for audio because encoded audio samples can
  // depend on other samples and still be used for random access. Generally all
  // audio samples are expected to be sync samples, but we  prefer to check the
  // flags to catch badly muxed audio (for now anyway ;P). History of attempts
  // to get this right discussed in http://crrev.com/1319813002
  bool sample_is_sync_sample = !(flags & kSampleIsNonSyncSample);
  bool sample_depends_on_others = sample_depends_on == kSampleDependsOnOthers;
  sample_info->is_keyframe = sample_is_sync_sample &&
                             (!sample_depends_on_others || is_audio);

  DVLOG(4) << __func__ << " is_kf:" << sample_info->is_keyframe
           << " is_sync:" << sample_is_sync_sample
           << " deps:" << sample_depends_on_others << " audio:" << is_audio;

  return true;
}

static const CencSampleEncryptionInfoEntry* GetSampleEncryptionInfoEntry(
    const TrackRunInfo& run_info,
    uint32_t group_description_index) {
  const std::vector<CencSampleEncryptionInfoEntry>* entries = nullptr;

  // ISO-14496-12 Section 8.9.2.3 and 8.9.4 : group description index
  // (1) ranges from 1 to the number of sample group entries in the track
  // level SampleGroupDescription Box, or (2) takes the value 0 to
  // indicate that this sample is a member of no group, in this case, the
  // sample is associated with the default values specified in
  // TrackEncryption Box, or (3) starts at 0x10001, i.e. the index value
  // 1, with the value 1 in the top 16 bits, to reference fragment-local
  // SampleGroupDescription Box.
  // Case (2) is not supported here. The caller must handle it externally
  // before invoking this function.
  DCHECK_NE(group_description_index, 0u);
  if (group_description_index >
      SampleToGroupEntry::kFragmentGroupDescriptionIndexBase) {
    group_description_index -=
        SampleToGroupEntry::kFragmentGroupDescriptionIndexBase;
    entries = &run_info.fragment_sample_encryption_info;
  } else {
    entries = &run_info.track_sample_encryption_group->entries;
  }

  // |group_description_index| is 1-based.
  return (group_description_index > entries->size())
             ? nullptr
             : &(*entries)[group_description_index - 1];
}

// In well-structured encrypted media, each track run will be immediately
// preceded by its auxiliary information; this is the only optimal storage
// pattern in terms of minimum number of bytes from a serial stream needed to
// begin playback. It also allows us to optimize caching on memory-constrained
// architectures, because we can cache the relatively small auxiliary
// information for an entire run and then discard data from the input stream,
// instead of retaining the entire 'mdat' box.
//
// We optimize for this situation (with no loss of generality) by sorting track
// runs during iteration in order of their first data offset (either sample data
// or auxiliary data).
class CompareMinTrackRunDataOffset {
 public:
  bool operator()(const TrackRunInfo& a, const TrackRunInfo& b) {
    int64_t a_aux = a.aux_info_total_size ? a.aux_info_start_offset
                                          : std::numeric_limits<int64_t>::max();
    int64_t b_aux = b.aux_info_total_size ? b.aux_info_start_offset
                                          : std::numeric_limits<int64_t>::max();

    int64_t a_lesser = std::min(a_aux, a.sample_start_offset);
    int64_t a_greater = std::max(a_aux, a.sample_start_offset);
    int64_t b_lesser = std::min(b_aux, b.sample_start_offset);
    int64_t b_greater = std::max(b_aux, b.sample_start_offset);

    if (a_lesser == b_lesser) return a_greater < b_greater;
    return a_lesser < b_lesser;
  }
};

bool TrackRunIterator::Init(const MovieFragment& moof) {
  runs_.clear();

  for (size_t i = 0; i < moof.tracks.size(); i++) {
    const TrackFragment& traf = moof.tracks[i];

    const Track* trak = NULL;
    for (size_t t = 0; t < moov_->tracks.size(); t++) {
      if (moov_->tracks[t].header.track_id == traf.header.track_id)
        trak = &moov_->tracks[t];
    }
    RCHECK(trak);

    const TrackExtends* trex = NULL;
    for (size_t t = 0; t < moov_->extends.tracks.size(); t++) {
      if (moov_->extends.tracks[t].track_id == traf.header.track_id)
        trex = &moov_->extends.tracks[t];
    }
    RCHECK(trex);

    const SampleDescription& stsd =
        trak->media.information.sample_table.description;
    if (stsd.type != kAudio && stsd.type != kVideo) {
      DVLOG(1) << "Skipping unhandled track type";
      continue;
    }
    size_t desc_idx = traf.header.sample_description_index;
    if (!desc_idx) desc_idx = trex->default_sample_description_index;
    RCHECK(desc_idx > 0);  // Descriptions are one-indexed in the file
    desc_idx -= 1;

    const std::vector<uint8_t>& sample_encryption_data =
        traf.sample_encryption.sample_encryption_data;
    std::unique_ptr<BufferReader> sample_encryption_reader;
    uint32_t sample_encryption_entries_count = 0;
    if (!sample_encryption_data.empty()) {
      sample_encryption_reader = std::make_unique<BufferReader>(
          sample_encryption_data.data(), sample_encryption_data.size());
      RCHECK(sample_encryption_reader->Read4(&sample_encryption_entries_count));
    }

    // Process edit list to remove CTS offset introduced in the presence of
    // B-frames (those that contain a single edit with a nonnegative media
    // time). Other uses of edit lists are not supported, as they are
    // both uncommon and better served by higher-level protocols.
    int64_t edit_list_offset = 0;
    const std::vector<EditListEntry>& edits = trak->edit.list.edits;
    if (!edits.empty()) {
      if (edits.size() > 1)
        DVLOG(1) << "Multi-entry edit box detected; some components ignored.";

      if (edits[0].media_time < 0) {
        DVLOG(1) << "Empty edit list entry ignored.";
      } else {
        edit_list_offset = edits[0].media_time;
      }
    }

    SampleToGroupIterator sample_to_group_itr(traf.sample_to_group);
    bool is_sample_to_group_valid = sample_to_group_itr.IsValid();

    int64_t run_start_dts = traf.decode_time.decode_time;
    uint64_t sample_count_sum = 0;
    for (size_t j = 0; j < traf.runs.size(); j++) {
      const TrackFragmentRun& trun = traf.runs[j];
      TrackRunInfo tri;
      tri.track_id = traf.header.track_id;
      tri.timescale = trak->media.header.timescale;
      tri.start_dts = run_start_dts;
      tri.sample_start_offset = trun.data_offset;
      tri.track_sample_encryption_group =
          &trak->media.information.sample_table.sample_group_description;
      tri.fragment_sample_encryption_info =
          traf.sample_group_description.entries;

      const TrackEncryption* track_encryption;
      const ProtectionSchemeInfo* sinf;
      tri.is_audio = (stsd.type == kAudio);
      if (tri.is_audio) {
        RCHECK(!stsd.audio_entries.empty());
        if (desc_idx >= stsd.audio_entries.size())
          desc_idx = 0;
        tri.audio_description = &stsd.audio_entries[desc_idx];
        sinf = &tri.audio_description->sinf;
        track_encryption = &tri.audio_description->sinf.info.track_encryption;
      } else {
        RCHECK(!stsd.video_entries.empty());
        if (desc_idx >= stsd.video_entries.size())
          desc_idx = 0;
        tri.video_description = &stsd.video_entries[desc_idx];
        sinf = &tri.video_description->sinf;
        track_encryption = &tri.video_description->sinf.info.track_encryption;
      }

      if (!sinf->HasSupportedScheme()) {
        tri.encryption_scheme = EncryptionScheme::kUnencrypted;
      } else {
        tri.encryption_scheme = sinf->IsCbcsEncryptionScheme()
                                    ? EncryptionScheme::kCbcs
                                    : EncryptionScheme::kCenc;
        tri.encryption_pattern =
            EncryptionPattern(track_encryption->default_crypt_byte_block,
                              track_encryption->default_skip_byte_block);
      }

      // Initialize aux_info variables only if no sample encryption entries.
      if (sample_encryption_entries_count == 0 &&
          traf.auxiliary_offset.offsets.size() > j) {
        // Collect information from the auxiliary_offset entry with the same
        // index in the 'saiz' container as the current run's index in the
        // 'trun' container, if it is present.
        // There should be an auxiliary info entry corresponding to each sample
        // in the auxiliary offset entry's corresponding track run.
        RCHECK(traf.auxiliary_size.sample_count >=
               sample_count_sum + trun.sample_count);
        tri.aux_info_start_offset = traf.auxiliary_offset.offsets[j];
        tri.aux_info_default_size =
            traf.auxiliary_size.default_sample_info_size;
        if (tri.aux_info_default_size == 0) {
          const std::vector<uint8_t>& sizes =
              traf.auxiliary_size.sample_info_sizes;
          tri.aux_info_sizes.insert(
              tri.aux_info_sizes.begin(), sizes.begin() + sample_count_sum,
              sizes.begin() + sample_count_sum + trun.sample_count);
        }

        // If the default info size is positive, find the total size of the aux
        // info block from it, otherwise sum over the individual sizes of each
        // aux info entry in the aux_offset entry.
        if (tri.aux_info_default_size) {
          tri.aux_info_total_size =
              tri.aux_info_default_size * trun.sample_count;
        } else {
          tri.aux_info_total_size = 0;
          for (size_t k = 0; k < trun.sample_count; k++) {
            tri.aux_info_total_size += tri.aux_info_sizes[k];
          }
        }
      } else {
        tri.aux_info_start_offset = -1;
        tri.aux_info_total_size = 0;
      }

      // Avoid allocating insane sample counts for invalid media.
      size_t max_sample_count =
          GetDemuxerMemoryLimit(Demuxer::DemuxerTypes::kChunkDemuxer) /
          sizeof(decltype(tri.samples)::value_type);

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
      // The fuzzer frequently gets stuck running out of memory on long useless
      // chains of empty TRUN values. Histogram analysis shows large in the wild
      // sample counts, so we can't limit more than the memory limit above.
      max_sample_count = std::min(size_t{10000}, max_sample_count);
#endif

      RCHECK_MEDIA_LOGGED(
          base::strict_cast<size_t>(trun.sample_count) <= max_sample_count,
          media_log_, "Metadata overhead exceeds storage limit.");
      tri.samples.resize(trun.sample_count);

      for (size_t k = 0; k < trun.sample_count; k++) {
        if (!PopulateSampleInfo(*trex, traf.header, trun, edit_list_offset, k,
                                &tri.samples[k], traf.sdtp.sample_depends_on(k),
                                tri.is_audio, media_log_)) {
          return false;
        }

        RCHECK(std::numeric_limits<int64_t>::max() - tri.samples[k].duration >
               run_start_dts);

        run_start_dts += tri.samples[k].duration;

        if (!is_sample_to_group_valid) {
          // Set group description index to 0 to read encryption information
          // from TrackEncryption Box.
          tri.samples[k].cenc_group_description_index = 0;
          continue;
        }

        uint32_t index = sample_to_group_itr.group_description_index();
        tri.samples[k].cenc_group_description_index = index;
        if (index != 0)
          RCHECK(GetSampleEncryptionInfoEntry(tri, index));
        is_sample_to_group_valid = sample_to_group_itr.Advance();
      }

      if (sample_encryption_entries_count > 0) {
        RCHECK(sample_encryption_entries_count >=
               sample_count_sum + trun.sample_count);
        tri.sample_encryption_entries.resize(trun.sample_count);
        for (size_t k = 0; k < trun.sample_count; k++) {
          uint32_t index = tri.samples[k].cenc_group_description_index;
          const CencSampleEncryptionInfoEntry* info_entry =
              index == 0 ? nullptr : GetSampleEncryptionInfoEntry(tri, index);
          const uint8_t iv_size = index == 0 ? track_encryption->default_iv_size
                                             : info_entry->iv_size;
          SampleEncryptionEntry& entry = tri.sample_encryption_entries[k];
          RCHECK(entry.Parse(sample_encryption_reader.get(), iv_size,
                             traf.sample_encryption.use_subsample_encryption));
          // If we don't have a per-sample IV, get the constant IV.
          bool is_encrypted = index == 0 ? track_encryption->is_encrypted
                                         : info_entry->is_encrypted;

          // TODO(crbug.com/1336055): Investigate if this is a hardware or
          // cast-related limitation.
#if BUILDFLAG(IS_CASTOS)
          // On Chromecast, we only support setting the pattern values in the
          // 'tenc' box for the track (not varying on per sample group basis).
          // Thus we need to verify that the settings in the sample group
          // match those in the 'tenc'.
          if (is_encrypted && index != 0) {
            RCHECK_MEDIA_LOGGED(info_entry->crypt_byte_block ==
                                    track_encryption->default_crypt_byte_block,
                                media_log_,
                                "Pattern value (crypt byte block) for the "
                                "sample group does not match that in the tenc "
                                "box . This is not currently supported.");
            RCHECK_MEDIA_LOGGED(info_entry->skip_byte_block ==
                                    track_encryption->default_skip_byte_block,
                                media_log_,
                                "Pattern value (skip byte block) for the "
                                "sample group does not match that in the tenc "
                                "box . This is not currently supported.");
          }
#endif  // BUILDFLAG(IS_CASTOS)
          if (is_encrypted && !iv_size) {
            const uint8_t constant_iv_size =
                index == 0 ? track_encryption->default_constant_iv_size
                           : info_entry->constant_iv_size;
            RCHECK(constant_iv_size != 0);
            const uint8_t* constant_iv =
                index == 0 ? track_encryption->default_constant_iv
                           : info_entry->constant_iv;
            memcpy(entry.initialization_vector, constant_iv, constant_iv_size);
          }
        }
      }
      runs_.push_back(tri);
      sample_count_sum += trun.sample_count;
    }

    // We should have iterated through all samples in SampleToGroup Box.
    RCHECK(!sample_to_group_itr.IsValid());
  }

  std::sort(runs_.begin(), runs_.end(), CompareMinTrackRunDataOffset());
  run_itr_ = runs_.begin();
  return ResetRun();
}

bool TrackRunIterator::UpdateCts() {
  // TODO(sandersd): Should |sample_cts_| be cleared in this case?
  if (!IsSampleValid())
    return true;
  auto cts = base::CheckAdd(sample_dts_, sample_itr_->cts_offset);
  if (!cts.AssignIfValid(&sample_cts_)) {
    MEDIA_LOG(ERROR, media_log_) << "Sample PTS exceeds representable range.";
    return false;
  }
  return true;
}

bool TrackRunIterator::AdvanceRun() {
  ++run_itr_;
  return ResetRun();
}

bool TrackRunIterator::ResetRun() {
  // TODO(sandersd): Should we clear all the values if the run is not valid?
  if (!IsRunValid())
    return true;
  sample_dts_ = run_itr_->start_dts;
  sample_offset_ = run_itr_->sample_start_offset;
  sample_itr_ = run_itr_->samples.begin();
  // UpdateCts() must run after |sample_itr_| is updated to the current run.
  return UpdateCts();
}

bool TrackRunIterator::AdvanceSample() {
  DCHECK(IsSampleValid());
  auto dts = base::CheckAdd(sample_dts_, sample_itr_->duration);
  if (!dts.AssignIfValid(&sample_dts_)) {
    MEDIA_LOG(ERROR, media_log_) << "Sample DTS exceeds representable range.";
    return false;
  }
  sample_offset_ += sample_itr_->size;
  ++sample_itr_;
  // UpdateCts() must run after |sample_itr_| is updated to the current sample.
  return UpdateCts();
}

// This implementation only indicates a need for caching if CENC auxiliary
// info is available in the stream.
bool TrackRunIterator::AuxInfoNeedsToBeCached() {
  DCHECK(IsRunValid());
  return is_encrypted() && aux_info_size() > 0 &&
         run_itr_->sample_encryption_entries.size() == 0;
}

// This implementation currently only caches CENC auxiliary info.
bool TrackRunIterator::CacheAuxInfo(const uint8_t* buf, int buf_size) {
  RCHECK(AuxInfoNeedsToBeCached() && buf_size >= aux_info_size());

  std::vector<SampleEncryptionEntry>& sample_encryption_entries =
      runs_[run_itr_ - runs_.begin()].sample_encryption_entries;
  sample_encryption_entries.resize(run_itr_->samples.size());
  int64_t pos = 0;
  for (size_t i = 0; i < run_itr_->samples.size(); i++) {
    int info_size = run_itr_->aux_info_default_size;
    if (!info_size)
      info_size = run_itr_->aux_info_sizes[i];

    if (IsSampleEncrypted(i)) {
      BufferReader reader(buf + pos, info_size);
      const uint8_t iv_size = GetIvSize(i);
      const bool has_subsamples = info_size > iv_size;
      SampleEncryptionEntry& entry = sample_encryption_entries[i];
      RCHECK_MEDIA_LOGGED(
          entry.Parse(&reader, iv_size, has_subsamples), media_log_,
          "SampleEncryptionEntry parse failed when caching aux info");
      // if we don't have a per-sample IV, get the constant IV.
      if (!iv_size) {
        RCHECK(ApplyConstantIv(i, &entry));
      }
    }
    pos += info_size;
  }

  return true;
}

bool TrackRunIterator::IsRunValid() const {
  return run_itr_ != runs_.end();
}

bool TrackRunIterator::IsSampleValid() const {
  return IsRunValid() && (sample_itr_ != run_itr_->samples.end());
}

// Because tracks are in sorted order and auxiliary information is cached when
// returning samples, it is guaranteed that no data will be required before the
// lesser of the minimum data offset of this track and the next in sequence.
// (The stronger condition - that no data is required before the minimum data
// offset of this track alone - is not guaranteed, because the BMFF spec does
// not have any inter-run ordering restrictions.)
int64_t TrackRunIterator::GetMaxClearOffset() {
  int64_t offset = std::numeric_limits<int64_t>::max();

  if (IsSampleValid()) {
    offset = std::min(offset, sample_offset_);
    if (AuxInfoNeedsToBeCached())
      offset = std::min(offset, aux_info_offset());
  }
  if (run_itr_ != runs_.end()) {
    auto next_run = run_itr_ + 1;
    if (next_run != runs_.end()) {
      offset = std::min(offset, next_run->sample_start_offset);
      if (next_run->aux_info_total_size)
        offset = std::min(offset, next_run->aux_info_start_offset);
    }
  }
  if (offset == std::numeric_limits<int64_t>::max())
    return 0;
  return offset;
}

uint32_t TrackRunIterator::track_id() const {
  DCHECK(IsRunValid());
  return run_itr_->track_id;
}

bool TrackRunIterator::is_encrypted() const {
  DCHECK(IsSampleValid());
  return IsSampleEncrypted(sample_itr_ - run_itr_->samples.begin());
}

int64_t TrackRunIterator::aux_info_offset() const {
  return run_itr_->aux_info_start_offset;
}

int TrackRunIterator::aux_info_size() const {
  return run_itr_->aux_info_total_size;
}

bool TrackRunIterator::is_audio() const {
  DCHECK(IsRunValid());
  return run_itr_->is_audio;
}

const AudioSampleEntry& TrackRunIterator::audio_description() const {
  DCHECK(is_audio());
  DCHECK(run_itr_->audio_description);
  return *run_itr_->audio_description;
}

const VideoSampleEntry& TrackRunIterator::video_description() const {
  DCHECK(!is_audio());
  DCHECK(run_itr_->video_description);
  return *run_itr_->video_description;
}

int64_t TrackRunIterator::sample_offset() const {
  DCHECK(IsSampleValid());
  return sample_offset_;
}

uint32_t TrackRunIterator::sample_size() const {
  DCHECK(IsSampleValid());
  return sample_itr_->size;
}

DecodeTimestamp TrackRunIterator::dts() const {
  DCHECK(IsSampleValid());
  return DecodeTimestampFromRational(sample_dts_, run_itr_->timescale);
}

base::TimeDelta TrackRunIterator::cts() const {
  DCHECK(IsSampleValid());
  return TimeDeltaFromRational(sample_cts_, run_itr_->timescale);
}

base::TimeDelta TrackRunIterator::duration() const {
  DCHECK(IsSampleValid());
  return TimeDeltaFromRational(sample_itr_->duration, run_itr_->timescale);
}

bool TrackRunIterator::is_keyframe() const {
  DCHECK(IsSampleValid());
  return sample_itr_->is_keyframe;
}

const ProtectionSchemeInfo& TrackRunIterator::protection_scheme_info() const {
  if (is_audio())
    return audio_description().sinf;
  return video_description().sinf;
}

const TrackEncryption& TrackRunIterator::track_encryption() const {
  return protection_scheme_info().info.track_encryption;
}

std::unique_ptr<DecryptConfig> TrackRunIterator::GetDecryptConfig() {
  DCHECK(is_encrypted());
  size_t sample_idx = sample_itr_ - run_itr_->samples.begin();
  const std::vector<uint8_t>& kid = GetKeyId(sample_idx);
  std::string key_id(kid.begin(), kid.end());

  if (run_itr_->sample_encryption_entries.empty()) {
    DCHECK_EQ(0, aux_info_size());
    // The 'cbcs' scheme allows empty aux info when a constant IV is in use
    // with full sample encryption. That case will fall through to here.
    SampleEncryptionEntry sample_encryption_entry;
    if (ApplyConstantIv(sample_idx, &sample_encryption_entry)) {
      std::string iv(reinterpret_cast<const char*>(
                         sample_encryption_entry.initialization_vector),
                     std::size(sample_encryption_entry.initialization_vector));
      switch (run_itr_->encryption_scheme) {
        case EncryptionScheme::kUnencrypted:
          return nullptr;
        case EncryptionScheme::kCenc:
          return DecryptConfig::CreateCencConfig(
              key_id, iv, sample_encryption_entry.subsamples);
        case EncryptionScheme::kCbcs:
          return DecryptConfig::CreateCbcsConfig(
              key_id, iv, sample_encryption_entry.subsamples,
              run_itr_->encryption_pattern);
      }
    }
    MEDIA_LOG(ERROR, media_log_) << "Sample encryption info is not available.";
    return nullptr;
  }

  DCHECK_LT(sample_idx, run_itr_->sample_encryption_entries.size());
  const SampleEncryptionEntry& sample_encryption_entry =
      run_itr_->sample_encryption_entries[sample_idx];
  std::string iv(reinterpret_cast<const char*>(
                     sample_encryption_entry.initialization_vector),
                 std::size(sample_encryption_entry.initialization_vector));

  size_t total_size = 0;
  if (!sample_encryption_entry.subsamples.empty() &&
      (!sample_encryption_entry.GetTotalSizeOfSubsamples(&total_size) ||
       total_size != static_cast<size_t>(sample_size()))) {
    MEDIA_LOG(ERROR, media_log_) << "Incorrect CENC subsample size.";
    return nullptr;
  }

  if (protection_scheme_info().IsCbcsEncryptionScheme()) {
    uint32_t index = GetGroupDescriptionIndex(sample_idx);
    uint32_t encrypt_blocks =
        (index == 0)
            ? track_encryption().default_crypt_byte_block
            : GetSampleEncryptionInfoEntry(*run_itr_, index)->crypt_byte_block;
    uint32_t skip_blocks =
        (index == 0)
            ? track_encryption().default_skip_byte_block
            : GetSampleEncryptionInfoEntry(*run_itr_, index)->skip_byte_block;
    return DecryptConfig::CreateCbcsConfig(
        key_id, iv, sample_encryption_entry.subsamples,
        EncryptionPattern(encrypt_blocks, skip_blocks));
  }

  return DecryptConfig::CreateCencConfig(key_id, iv,
                                         sample_encryption_entry.subsamples);
}

uint32_t TrackRunIterator::GetGroupDescriptionIndex(
    uint32_t sample_index) const {
  DCHECK(IsRunValid());
  DCHECK_LT(sample_index, run_itr_->samples.size());
  return run_itr_->samples[sample_index].cenc_group_description_index;
}

bool TrackRunIterator::IsSampleEncrypted(size_t sample_index) const {
  uint32_t index = GetGroupDescriptionIndex(sample_index);
  return (index == 0)
             ? track_encryption().is_encrypted
             : GetSampleEncryptionInfoEntry(*run_itr_, index)->is_encrypted;
}

const std::vector<uint8_t>& TrackRunIterator::GetKeyId(
    size_t sample_index) const {
  uint32_t index = GetGroupDescriptionIndex(sample_index);
  return (index == 0) ? track_encryption().default_kid
                      : GetSampleEncryptionInfoEntry(*run_itr_, index)->key_id;
}

uint8_t TrackRunIterator::GetIvSize(size_t sample_index) const {
  uint32_t index = GetGroupDescriptionIndex(sample_index);
  return (index == 0) ? track_encryption().default_iv_size
                      : GetSampleEncryptionInfoEntry(*run_itr_, index)->iv_size;
}

bool TrackRunIterator::ApplyConstantIv(size_t sample_index,
                                       SampleEncryptionEntry* entry) const {
  DCHECK(IsSampleEncrypted(sample_index));
  uint32_t index = GetGroupDescriptionIndex(sample_index);
  const uint8_t constant_iv_size =
      index == 0
          ? track_encryption().default_constant_iv_size
          : GetSampleEncryptionInfoEntry(*run_itr_, index)->constant_iv_size;
  RCHECK(constant_iv_size != 0);
  const uint8_t* constant_iv =
      index == 0 ? track_encryption().default_constant_iv
                 : GetSampleEncryptionInfoEntry(*run_itr_, index)->constant_iv;
  RCHECK(constant_iv != nullptr);
  memcpy(entry->initialization_vector, constant_iv, kInitializationVectorSize);
  return true;
}

}  // namespace mp4
}  // namespace media
