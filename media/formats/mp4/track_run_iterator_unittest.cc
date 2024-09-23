// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/mp4/track_run_iterator.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "media/base/mock_media_log.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/rcheck.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrictMock;

namespace {

// The sum of the elements in a vector initialized with SumAscending,
// less the value of the last element.
const int kSumAscending1 = 45;

const int kAudioScale = 48000;
const int kVideoScale = 25;

const uint8_t kAuxInfo[] = {
    0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x31, 0x41, 0x54,
    0x65, 0x73, 0x74, 0x49, 0x76, 0x32, 0x00, 0x02, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
};

// Sample encryption data for two samples, one with 8 byte IV, one with 16 byte
// IV. This data is generated for testing. It should be very unlikely to see
// IV of mixed size in actual media files, though it is permitted by spec.
const uint8_t kSampleEncryptionDataWithSubsamples[] = {
    // Sample count.
    0x00, 0x00, 0x00, 0x02,
    // Sample 1: IV (8 Bytes).
    0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x31,
    // Sample 1: Subsample count.
    0x00, 0x01,
    // Sample 1: Subsample 1.
    0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
    // Sample 2: IV (16 bytes).
    0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x32, 0x41, 0x42, 0x43, 0x44,
    0x45, 0x46, 0x47, 0x48,
    // Sample 2: Subsample count.
    0x00, 0x02,
    // Sample 2: Subsample 1.
    0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
    // Sample 2: Subsample 2.
    0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
};

const uint8_t kSampleEncryptionDataWithoutSubsamples[] = {
    // Sample count.
    0x00, 0x00, 0x00, 0x02,
    // Sample 1: IV.
    0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x31,
    // Sample 2: IV.
    0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x32,
};

// Size of these two IVs are 8 bytes. They are padded with 0 to 16 bytes.
const char kIv1[] = {
    0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x31,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const char kIv2[] = {
    0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x32,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// Size of this IV is 16 bytes.
const char kIv3[] = {
    0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x32,
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
};

const uint8_t kKeyId[] = {
    0x41, 0x47, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x54,
    0x65, 0x73, 0x74, 0x4b, 0x65, 0x79, 0x49, 0x44,
};

const uint8_t kTrackCencSampleGroupKeyId[] = {
    0x46, 0x72, 0x61, 0x67, 0x53, 0x61, 0x6d, 0x70,
    0x6c, 0x65, 0x47, 0x72, 0x6f, 0x75, 0x70, 0x4b,
};

const uint8_t kFragmentCencSampleGroupKeyId[] = {
    0x6b, 0x46, 0x72, 0x61, 0x67, 0x6d, 0x65, 0x6e,
    0x74, 0x43, 0x65, 0x6e, 0x63, 0x53, 0x61, 0x6d,
};

// Sample encryption data for two samples, using constant IV (defined by 'tenc'
// or sample group entry).
const uint8_t kSampleEncryptionDataWithSubsamplesAndConstantIv[] = {
    // Sample count.
    0x00, 0x00, 0x00, 0x05,
    // Sample 1: Subsample count.
    0x00, 0x01,
    // Sample 1: Subsample 1.
    0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
    // Sample 2: Subsample count.
    0x00, 0x02,
    // Sample 2: Subsample 1.
    0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
    // Sample 2: Subsample 2.
    0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
    // Sample 3: Subsample count.
    0x00, 0x01,
    // Sample 3: Subsample 1.
    0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
    // Sample 4: Subsample count.
    0x00, 0x01,
    // Sample 4: Subsample 1.
    0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
    // Sample 5: Subsample count.
    0x00, 0x01,
    // Sample 5: Subsample 1.
    0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
};

// Size of these IVs are 16 bytes.
const char kIv4[] = {
    0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x34,
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
};

const char kIv5[] = {
    0x41, 0x54, 0x65, 0x73, 0x74, 0x49, 0x76, 0x35,
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
};

}  // namespace

namespace media {
namespace mp4 {

MATCHER(ReservedValueInSampleDependencyInfo, "") {
  return CONTAINS_STRING(arg, "Reserved value used in sample dependency info.");
}

TEST(TimeDeltaFromRationalTest, RoundsTowardZero) {
  // In each case, 1.5us should round to 1us.
  base::TimeDelta expected = base::Microseconds(1);
  EXPECT_EQ(TimeDeltaFromRational(3, 2000000), expected);
  EXPECT_EQ(TimeDeltaFromRational(-3, 2000000), -expected);
}

TEST(TimeDeltaFromRationalTest, HandlesLargeValues) {
  int64_t max_seconds =
      std::numeric_limits<int64_t>::max() / base::Time::kMicrosecondsPerSecond;
  // The current implementation rejects |max_seconds|.
  // Note: kNoTimestamp is printed as "9.22337e+12 s", which is visually
  // indistinguishable from |expected|.
  int64_t seconds = max_seconds - 1;
  base::TimeDelta expected = base::Seconds(seconds);
  EXPECT_EQ(TimeDeltaFromRational(seconds, 1), expected);
  EXPECT_EQ(TimeDeltaFromRational(-seconds, 1), -expected);
}

TEST(TimeDeltaFromRationalTest, HandlesOverflow) {
  int64_t max_seconds =
      std::numeric_limits<int64_t>::max() / base::Time::kMicrosecondsPerSecond;
  int64_t seconds = max_seconds + 1;
  EXPECT_EQ(TimeDeltaFromRational(seconds, 1), kNoTimestamp);
  EXPECT_EQ(TimeDeltaFromRational(-seconds, 1), kNoTimestamp);
}

class TrackRunIteratorTest : public testing::Test {
 public:
  TrackRunIteratorTest() { CreateMovie(); }

 protected:
  StrictMock<MockMediaLog> media_log_;
  Movie moov_;
  std::unique_ptr<TrackRunIterator> iter_;

  void CreateMovie() {
    moov_.header.timescale = 1000;
    moov_.tracks.resize(3);
    moov_.extends.tracks.resize(2);
    moov_.tracks[0].header.track_id = 1;
    moov_.tracks[0].media.header.timescale = kAudioScale;
    SampleDescription& desc1 =
        moov_.tracks[0].media.information.sample_table.description;
    AudioSampleEntry aud_desc;
    aud_desc.format = FOURCC_MP4A;
    aud_desc.sinf.info.track_encryption.is_encrypted = false;
    desc1.type = kAudio;
    desc1.audio_entries.push_back(aud_desc);
    moov_.extends.tracks[0].track_id = 1;
    moov_.extends.tracks[0].default_sample_description_index = 1;
    moov_.tracks[1].header.track_id = 2;
    moov_.tracks[1].media.header.timescale = kVideoScale;
    SampleDescription& desc2 =
        moov_.tracks[1].media.information.sample_table.description;
    VideoSampleEntry vid_desc;
    vid_desc.format = FOURCC_AVC1;
    vid_desc.sinf.info.track_encryption.is_encrypted = false;
    desc2.type = kVideo;
    desc2.video_entries.push_back(vid_desc);
    moov_.extends.tracks[1].track_id = 2;
    moov_.extends.tracks[1].default_sample_description_index = 1;

    moov_.tracks[2].header.track_id = 3;
    moov_.tracks[2].media.information.sample_table.description.type = kHint;
  }

  uint32_t ToSampleFlags(const std::string& str) {
    CHECK_EQ(str.length(), 2u);

    SampleDependsOn sample_depends_on = kSampleDependsOnReserved;
    bool is_non_sync_sample = false;
    switch(str[0]) {
      case 'U':
        sample_depends_on = kSampleDependsOnUnknown;
        break;
      case 'O':
        sample_depends_on = kSampleDependsOnOthers;
        break;
      case 'N':
        sample_depends_on = kSampleDependsOnNoOther;
        break;
      case 'R':
        sample_depends_on = kSampleDependsOnReserved;
        break;
      default:
        CHECK(false) << "Invalid sample dependency character '"
                     << str[0] << "'";
        break;
    }

    switch(str[1]) {
      case 'S':
        is_non_sync_sample = false;
        break;
      case 'N':
        is_non_sync_sample = true;
        break;
      default:
        CHECK(false) << "Invalid sync sample character '"
                     << str[1] << "'";
        break;
    }
    uint32_t flags = static_cast<uint32_t>(sample_depends_on) << 24;
    if (is_non_sync_sample)
      flags |= kSampleIsNonSyncSample;
    return flags;
  }

  void SetFlagsOnSamples(const std::string& sample_info,
                         TrackFragmentRun* trun) {
    // US - SampleDependsOnUnknown & IsSyncSample
    // UN - SampleDependsOnUnknown & IsNonSyncSample
    // OS - SampleDependsOnOthers & IsSyncSample
    // ON - SampleDependsOnOthers & IsNonSyncSample
    // NS - SampleDependsOnNoOthers & IsSyncSample
    // NN - SampleDependsOnNoOthers & IsNonSyncSample
    std::vector<std::string> flags_data = base::SplitString(
        sample_info, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    if (flags_data.size() == 1u) {
      // Simulates the first_sample_flags_present set scenario,
      // where only one sample_flag value is set and the default
      // flags are used for everything else.
      ASSERT_GE(trun->sample_count, flags_data.size());
    } else {
      ASSERT_EQ(trun->sample_count, flags_data.size());
    }

    trun->sample_flags.resize(flags_data.size());
    for (size_t i = 0; i < flags_data.size(); i++)
      trun->sample_flags[i] = ToSampleFlags(flags_data[i]);
  }

  std::string KeyframeAndRAPInfo(TrackRunIterator* iter) {
    CHECK(iter->IsRunValid());
    std::stringstream ss;
    ss << iter->track_id();

    while (iter->IsSampleValid()) {
      ss << " " << (iter->is_keyframe() ? "K" : "P");
      iter->AdvanceSample();
    }

    return ss.str();
  }

  MovieFragment CreateFragment() {
    MovieFragment moof;
    moof.tracks.resize(2);
    moof.tracks[0].decode_time.decode_time = 0;
    moof.tracks[0].header.track_id = 1;
    moof.tracks[0].header.has_default_sample_flags = true;
    moof.tracks[0].header.default_sample_flags = ToSampleFlags("US");
    moof.tracks[0].header.default_sample_duration = 1024;
    moof.tracks[0].header.default_sample_size = 4;
    moof.tracks[0].runs.resize(2);
    moof.tracks[0].runs[0].sample_count = 10;
    moof.tracks[0].runs[0].data_offset = 100;
    SetAscending(&moof.tracks[0].runs[0].sample_sizes);

    moof.tracks[0].runs[1].sample_count = 10;
    moof.tracks[0].runs[1].data_offset = 10000;

    moof.tracks[1].header.track_id = 2;
    moof.tracks[1].header.has_default_sample_flags = false;
    moof.tracks[1].decode_time.decode_time = 10;
    moof.tracks[1].runs.resize(1);
    moof.tracks[1].runs[0].sample_count = 10;
    moof.tracks[1].runs[0].data_offset = 200;
    SetAscending(&moof.tracks[1].runs[0].sample_sizes);
    SetAscending(&moof.tracks[1].runs[0].sample_durations);
    SetFlagsOnSamples("US UN UN UN UN UN UN UN UN UN", &moof.tracks[1].runs[0]);

    return moof;
  }

  ProtectionSchemeInfo* GetProtectionSchemeInfoForTrack(Track* track) {
    SampleDescription* stsd =
        &track->media.information.sample_table.description;
    ProtectionSchemeInfo* sinf;
    if (!stsd->video_entries.empty()) {
       sinf = &stsd->video_entries[0].sinf;
    } else {
       sinf = &stsd->audio_entries[0].sinf;
    }
    return sinf;
  }

  // Update the first sample description of a Track to indicate CENC encryption
  void AddEncryption(Track* track) {
    ProtectionSchemeInfo* sinf = GetProtectionSchemeInfoForTrack(track);
    sinf->type.type = FOURCC_CENC;
    sinf->info.track_encryption.is_encrypted = true;
    sinf->info.track_encryption.default_iv_size = 8;
    sinf->info.track_encryption.default_kid.assign(kKeyId,
                                                   kKeyId + std::size(kKeyId));
  }

  // Add SampleGroupDescription Box to track level sample table and to
  // fragment. Populate SampleToGroup Box from input array.
  void AddCencSampleGroup(Track* track,
                          TrackFragment* frag,
                          const SampleToGroupEntry* sample_to_group_entries,
                          size_t num_entries) {
    auto& track_cenc_group =
        track->media.information.sample_table.sample_group_description;
    track_cenc_group.grouping_type = FOURCC_SEIG;
    track_cenc_group.entries.resize(1);
    track_cenc_group.entries[0].is_encrypted = true;
    track_cenc_group.entries[0].iv_size = 8;
    track_cenc_group.entries[0].key_id.assign(
        kTrackCencSampleGroupKeyId,
        kTrackCencSampleGroupKeyId + std::size(kTrackCencSampleGroupKeyId));

    frag->sample_group_description.grouping_type = FOURCC_SEIG;
    frag->sample_group_description.entries.resize(3);
    frag->sample_group_description.entries[0].is_encrypted = false;
    frag->sample_group_description.entries[0].iv_size = 0;
    frag->sample_group_description.entries[1].is_encrypted = true;
    frag->sample_group_description.entries[1].iv_size = 8;
    frag->sample_group_description.entries[1].key_id.assign(
        kFragmentCencSampleGroupKeyId,
        kFragmentCencSampleGroupKeyId +
            std::size(kFragmentCencSampleGroupKeyId));
    frag->sample_group_description.entries[2].is_encrypted = true;
    frag->sample_group_description.entries[2].iv_size = 16;
    frag->sample_group_description.entries[2].key_id.assign(
        kKeyId, kKeyId + std::size(kKeyId));

    frag->sample_to_group.grouping_type = FOURCC_SEIG;
    frag->sample_to_group.entries.assign(sample_to_group_entries,
                                         sample_to_group_entries + num_entries);
  }

  // Add aux info covering the first track run to a TrackFragment, and update
  // the run to ensure it matches length and subsample information.
  void AddAuxInfoHeaders(int offset, TrackFragment* frag) {
    frag->auxiliary_offset.offsets.push_back(offset);
    frag->auxiliary_size.sample_count = 2;
    frag->auxiliary_size.sample_info_sizes.push_back(8);
    frag->auxiliary_size.sample_info_sizes.push_back(22);
    frag->runs[0].sample_count = 2;
    frag->runs[0].sample_sizes[1] = 10;
  }

  void AddSampleEncryption(uint8_t use_subsample_flag, TrackFragment* frag) {
    frag->sample_encryption.use_subsample_encryption = use_subsample_flag;
    if (use_subsample_flag) {
      frag->sample_encryption.sample_encryption_data.assign(
          kSampleEncryptionDataWithSubsamples,
          kSampleEncryptionDataWithSubsamples +
              std::size(kSampleEncryptionDataWithSubsamples));
    } else {
      frag->sample_encryption.sample_encryption_data.assign(
          kSampleEncryptionDataWithoutSubsamples,
          kSampleEncryptionDataWithoutSubsamples +
              std::size(kSampleEncryptionDataWithoutSubsamples));
    }

    // Update sample sizes and aux info header.
    frag->runs.resize(1);
    frag->runs[0].sample_count = 2;
    frag->auxiliary_offset.offsets.push_back(0);
    frag->auxiliary_size.sample_count = 2;
    if (use_subsample_flag) {
      // Update sample sizes to match with subsample entries above.
      frag->runs[0].sample_sizes[0] = 3;
      frag->runs[0].sample_sizes[1] = 10;
      // Set aux info header.
      frag->auxiliary_size.sample_info_sizes.push_back(16);
      frag->auxiliary_size.sample_info_sizes.push_back(30);
    } else {
      frag->auxiliary_size.default_sample_info_size = 8;
    }
  }

  // Update the first sample description of a Track to indicate CBCS encryption
  // with a constant IV and pattern.
  void AddEncryptionCbcs(Track* track) {
    ProtectionSchemeInfo* sinf = GetProtectionSchemeInfoForTrack(track);
    sinf->type.type = FOURCC_CBCS;
    sinf->info.track_encryption.is_encrypted = true;
    sinf->info.track_encryption.default_iv_size = 0;
    sinf->info.track_encryption.default_crypt_byte_block = 1;
    sinf->info.track_encryption.default_skip_byte_block = 9;
    sinf->info.track_encryption.default_constant_iv_size = 16;
    memcpy(sinf->info.track_encryption.default_constant_iv, kIv3, 16);
    sinf->info.track_encryption.default_kid.assign(kKeyId,
                                                   kKeyId + std::size(kKeyId));
  }

  void AddConstantIvsToCencSampleGroup(Track* track, TrackFragment* frag) {
    auto& track_cenc_group =
        track->media.information.sample_table.sample_group_description;
    track_cenc_group.entries[0].iv_size = 0;
    track_cenc_group.entries[0].crypt_byte_block = 1;
    track_cenc_group.entries[0].skip_byte_block = 9;
    track_cenc_group.entries[0].constant_iv_size = 16;
    memcpy(track_cenc_group.entries[0].constant_iv, kIv4, 16);

    frag->sample_group_description.entries[1].iv_size = 0;
    frag->sample_group_description.entries[1].crypt_byte_block = 1;
    frag->sample_group_description.entries[1].skip_byte_block = 9;
    frag->sample_group_description.entries[1].constant_iv_size = 16;
    memcpy(frag->sample_group_description.entries[1].constant_iv, kIv5, 16);
    frag->sample_group_description.entries[2].iv_size = 0;
    frag->sample_group_description.entries[2].crypt_byte_block = 1;
    frag->sample_group_description.entries[2].skip_byte_block = 9;
    frag->sample_group_description.entries[2].constant_iv_size = 16;
    memcpy(frag->sample_group_description.entries[2].constant_iv, kIv5, 16);
  }

  void AddSampleEncryptionCbcs(TrackFragment* frag) {
    frag->sample_encryption.use_subsample_encryption = true;
    frag->sample_encryption.sample_encryption_data.assign(
        kSampleEncryptionDataWithSubsamplesAndConstantIv,
        kSampleEncryptionDataWithSubsamplesAndConstantIv +
            std::size(kSampleEncryptionDataWithSubsamplesAndConstantIv));

    // Update sample sizes and aux info header.
    frag->runs.resize(1);
    frag->runs[0].sample_count = 5;
    frag->auxiliary_offset.offsets.push_back(0);
    frag->auxiliary_size.sample_count = 5;
    // Update sample sizes to match with subsample entries above.
    frag->runs[0].sample_sizes[0] = 3;
    frag->runs[0].sample_sizes[1] = 10;
    frag->runs[0].sample_sizes[2] = 3;
    frag->runs[0].sample_sizes[3] = 3;
    frag->runs[0].sample_sizes[4] = 3;
    // Set aux info header.
    frag->auxiliary_size.sample_info_sizes.push_back(16);
    frag->auxiliary_size.sample_info_sizes.push_back(30);
    frag->auxiliary_size.sample_info_sizes.push_back(16);
    frag->auxiliary_size.sample_info_sizes.push_back(16);
    frag->auxiliary_size.sample_info_sizes.push_back(16);
  }

  bool InitMoofWithArbitraryAuxInfo(MovieFragment* moof) {
    // Add aux info header (equal sized aux info for every sample).
    for (uint32_t i = 0; i < moof->tracks.size(); ++i) {
      moof->tracks[i].auxiliary_offset.offsets.push_back(50);
      moof->tracks[i].auxiliary_size.sample_count = 10;
      moof->tracks[i].auxiliary_size.default_sample_info_size = 8;
    }

    // We don't care about the actual data in aux.
    std::vector<uint8_t> aux_info(1000);
    return iter_->Init(*moof) &&
           iter_->CacheAuxInfo(&aux_info[0], aux_info.size());
  }

  void SetAscending(std::vector<uint32_t>* vec) {
    vec->resize(10);
    for (size_t i = 0; i < vec->size(); i++)
      (*vec)[i] = i+1;
  }
};

TEST_F(TrackRunIteratorTest, NoRunsTest) {
  iter_.reset(new TrackRunIterator(&moov_, &media_log_));
  ASSERT_TRUE(iter_->Init(MovieFragment()));
  EXPECT_FALSE(iter_->IsRunValid());
  EXPECT_FALSE(iter_->IsSampleValid());
}

TEST_F(TrackRunIteratorTest, BasicOperationTest) {
  iter_.reset(new TrackRunIterator(&moov_, &media_log_));
  MovieFragment moof = CreateFragment();

  // Test that runs are sorted correctly, and that properties of the initial
  // sample of the first run are correct
  ASSERT_TRUE(iter_->Init(moof));
  EXPECT_TRUE(iter_->IsRunValid());
  EXPECT_FALSE(iter_->is_encrypted());
  EXPECT_EQ(iter_->track_id(), 1u);
  EXPECT_EQ(iter_->sample_offset(), 100);
  EXPECT_EQ(iter_->sample_size(), 1u);
  EXPECT_EQ(iter_->dts(), DecodeTimestampFromRational(0, kAudioScale));
  EXPECT_EQ(iter_->cts(), TimeDeltaFromRational(0, kAudioScale));
  EXPECT_EQ(iter_->duration(), TimeDeltaFromRational(1024, kAudioScale));
  EXPECT_TRUE(iter_->is_keyframe());

  // Advance to the last sample in the current run, and test its properties
  for (int i = 0; i < 9; i++) iter_->AdvanceSample();
  EXPECT_EQ(iter_->track_id(), 1u);
  EXPECT_EQ(iter_->sample_offset(), 100 + kSumAscending1);
  EXPECT_EQ(iter_->sample_size(), 10u);
  EXPECT_EQ(iter_->dts(), DecodeTimestampFromRational(1024 * 9, kAudioScale));
  EXPECT_EQ(iter_->duration(), TimeDeltaFromRational(1024, kAudioScale));
  EXPECT_TRUE(iter_->is_keyframe());

  // Test end-of-run
  iter_->AdvanceSample();
  EXPECT_FALSE(iter_->IsSampleValid());

  // Test last sample of next run
  iter_->AdvanceRun();
  EXPECT_TRUE(iter_->is_keyframe());
  for (int i = 0; i < 9; i++) iter_->AdvanceSample();
  EXPECT_EQ(iter_->track_id(), 2u);
  EXPECT_EQ(iter_->sample_offset(), 200 + kSumAscending1);
  EXPECT_EQ(iter_->sample_size(), 10u);
  int64_t base_dts = kSumAscending1 + moof.tracks[1].decode_time.decode_time;
  EXPECT_EQ(iter_->dts(), DecodeTimestampFromRational(base_dts, kVideoScale));
  EXPECT_EQ(iter_->duration(), TimeDeltaFromRational(10, kVideoScale));
  EXPECT_FALSE(iter_->is_keyframe());

  // Test final run
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->track_id(), 1u);
  EXPECT_EQ(iter_->dts(), DecodeTimestampFromRational(1024 * 10, kAudioScale));
  iter_->AdvanceSample();
  EXPECT_EQ(moof.tracks[0].runs[1].data_offset +
            moof.tracks[0].header.default_sample_size,
            iter_->sample_offset());
  iter_->AdvanceRun();
  EXPECT_FALSE(iter_->IsRunValid());
}

TEST_F(TrackRunIteratorTest, TrackExtendsDefaultsTest) {
  moov_.extends.tracks[0].default_sample_duration = 50;
  moov_.extends.tracks[0].default_sample_size = 3;
  moov_.extends.tracks[0].default_sample_flags = ToSampleFlags("UN");
  iter_.reset(new TrackRunIterator(&moov_, &media_log_));
  MovieFragment moof = CreateFragment();
  moof.tracks[0].header.has_default_sample_flags = false;
  moof.tracks[0].header.default_sample_size = 0;
  moof.tracks[0].header.default_sample_duration = 0;
  moof.tracks[0].runs[0].sample_sizes.clear();
  ASSERT_TRUE(iter_->Init(moof));
  iter_->AdvanceSample();
  EXPECT_FALSE(iter_->is_keyframe());
  EXPECT_EQ(iter_->sample_size(), 3u);
  EXPECT_EQ(iter_->sample_offset(), moof.tracks[0].runs[0].data_offset + 3);
  EXPECT_EQ(iter_->duration(), TimeDeltaFromRational(50, kAudioScale));
  EXPECT_EQ(iter_->dts(), DecodeTimestampFromRational(50, kAudioScale));
}

TEST_F(TrackRunIteratorTest, FirstSampleFlagTest) {
  // Ensure that keyframes are flagged correctly in the face of BMFF boxes which
  // explicitly specify the flags for the first sample in a run and rely on
  // defaults for all subsequent samples
  iter_.reset(new TrackRunIterator(&moov_, &media_log_));
  MovieFragment moof = CreateFragment();
  moof.tracks[1].header.has_default_sample_flags = true;
  moof.tracks[1].header.default_sample_flags = ToSampleFlags("UN");
  SetFlagsOnSamples("US", &moof.tracks[1].runs[0]);

  ASSERT_TRUE(iter_->Init(moof));
  EXPECT_EQ("1 K K K K K K K K K K", KeyframeAndRAPInfo(iter_.get()));

  iter_->AdvanceRun();
  EXPECT_EQ("2 K P P P P P P P P P", KeyframeAndRAPInfo(iter_.get()));
}

// Verify that parsing fails if a reserved value is in the sample flags.
TEST_F(TrackRunIteratorTest, SampleInfoTest_ReservedInSampleFlags) {
  EXPECT_MEDIA_LOG(ReservedValueInSampleDependencyInfo());
  iter_.reset(new TrackRunIterator(&moov_, &media_log_));
  MovieFragment moof = CreateFragment();
  // Change the "depends on" field on one of the samples to a
  // reserved value.
  moof.tracks[1].runs[0].sample_flags[0] = ToSampleFlags("RS");
  ASSERT_FALSE(iter_->Init(moof));
}

// Verify that parsing fails if a reserved value is in the default sample flags.
TEST_F(TrackRunIteratorTest, SampleInfoTest_ReservedInDefaultSampleFlags) {
  EXPECT_MEDIA_LOG(ReservedValueInSampleDependencyInfo());
  iter_.reset(new TrackRunIterator(&moov_, &media_log_));
  MovieFragment moof = CreateFragment();
  // Set the default flag to contain a reserved "depends on" value.
  moof.tracks[0].header.default_sample_flags = ToSampleFlags("RN");
  ASSERT_FALSE(iter_->Init(moof));
}

TEST_F(TrackRunIteratorTest, ReorderingTest) {
  // Test frame reordering and edit list support. The frames have the following
  // decode timestamps:
  //
  //   0ms 40ms   120ms     240ms
  //   | 0 | 1  - | 2  -  - |
  //
  // ...and these composition timestamps, after edit list adjustment:
  //
  //   0ms 40ms       160ms  240ms
  //   | 0 | 2  -  -  | 1 - |

  // Create an edit list with one entry, with an initial start time of 80ms
  // (that is, 2 / kVideoTimescale) and a duration of zero (which is treated as
  // infinite according to 14496-12:2012). This will cause the first 80ms of the
  // media timeline - which will be empty, due to CTS biasing - to be discarded.
  iter_.reset(new TrackRunIterator(&moov_, &media_log_));
  EditListEntry entry;
  entry.segment_duration = 0;
  entry.media_time = 2;
  entry.media_rate_integer = 1;
  entry.media_rate_fraction = 0;
  moov_.tracks[1].edit.list.edits.push_back(entry);

  // Add CTS offsets. Without bias, the CTS offsets for the first three frames
  // would simply be [0, 3, -2]. Since CTS offsets should be non-negative for
  // maximum compatibility, these values are biased up to [2, 5, 0], and the
  // extra 80ms is removed via the edit list.
  MovieFragment moof = CreateFragment();
  std::vector<int32_t>& cts_offsets =
      moof.tracks[1].runs[0].sample_composition_time_offsets;
  cts_offsets.resize(10);
  cts_offsets[0] = 2;
  cts_offsets[1] = 5;
  cts_offsets[2] = 0;
  moof.tracks[1].decode_time.decode_time = 0;

  ASSERT_TRUE(iter_->Init(moof));
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->dts(), DecodeTimestampFromRational(0, kVideoScale));
  EXPECT_EQ(iter_->cts(), TimeDeltaFromRational(0, kVideoScale));
  EXPECT_EQ(iter_->duration(), TimeDeltaFromRational(1, kVideoScale));
  iter_->AdvanceSample();
  EXPECT_EQ(iter_->dts(), DecodeTimestampFromRational(1, kVideoScale));
  EXPECT_EQ(iter_->cts(), TimeDeltaFromRational(4, kVideoScale));
  EXPECT_EQ(iter_->duration(), TimeDeltaFromRational(2, kVideoScale));
  iter_->AdvanceSample();
  EXPECT_EQ(iter_->dts(), DecodeTimestampFromRational(3, kVideoScale));
  EXPECT_EQ(iter_->cts(), TimeDeltaFromRational(1, kVideoScale));
  EXPECT_EQ(iter_->duration(), TimeDeltaFromRational(3, kVideoScale));
}

TEST_F(TrackRunIteratorTest, IgnoreUnknownAuxInfoTest) {
  iter_.reset(new TrackRunIterator(&moov_, &media_log_));
  MovieFragment moof = CreateFragment();
  moof.tracks[1].auxiliary_offset.offsets.push_back(50);
  moof.tracks[1].auxiliary_size.default_sample_info_size = 2;
  moof.tracks[1].auxiliary_size.sample_count = 2;
  moof.tracks[1].runs[0].sample_count = 2;
  ASSERT_TRUE(iter_->Init(moof));
  iter_->AdvanceRun();
  EXPECT_FALSE(iter_->AuxInfoNeedsToBeCached());
}

TEST_F(TrackRunIteratorTest,
       DecryptConfigTestWithSampleEncryptionAndNoSubsample) {
  AddEncryption(&moov_.tracks[1]);
  iter_.reset(new TrackRunIterator(&moov_, &media_log_));

  MovieFragment moof = CreateFragment();
  AddSampleEncryption(!SampleEncryption::kUseSubsampleEncryption,
                      &moof.tracks[1]);

  ASSERT_TRUE(iter_->Init(moof));
  // The run for track 2 will be the second, which is parsed according to
  // data_offset.
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->track_id(), 2u);

  EXPECT_TRUE(iter_->is_encrypted());
  // No need to cache aux info as it is already available in SampleEncryption.
  EXPECT_FALSE(iter_->AuxInfoNeedsToBeCached());
  EXPECT_EQ(iter_->aux_info_size(), 0);
  EXPECT_EQ(iter_->sample_offset(), 200);
  EXPECT_EQ(iter_->GetMaxClearOffset(), moof.tracks[1].runs[0].data_offset);
  std::unique_ptr<DecryptConfig> config = iter_->GetDecryptConfig();
  EXPECT_EQ(
      std::string(reinterpret_cast<const char*>(kKeyId), std::size(kKeyId)),
      config->key_id());
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(kIv1), std::size(kIv1)),
            config->iv());
  EXPECT_EQ(config->subsamples().size(), 0u);
  iter_->AdvanceSample();
  config = iter_->GetDecryptConfig();
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(kIv2), std::size(kIv2)),
            config->iv());
  EXPECT_EQ(config->subsamples().size(), 0u);
}

TEST_F(TrackRunIteratorTest,
       DecryptConfigTestWithSampleEncryptionAndSubsample) {
  AddEncryption(&moov_.tracks[1]);
  iter_.reset(new TrackRunIterator(&moov_, &media_log_));

  MovieFragment moof = CreateFragment();
  AddSampleEncryption(SampleEncryption::kUseSubsampleEncryption,
                      &moof.tracks[1]);
  const SampleToGroupEntry kSampleToGroupTable[] = {
      // Associated with the second entry in SampleGroupDescription Box.
      // With Iv size 8 bytes.
      {1, SampleToGroupEntry::kFragmentGroupDescriptionIndexBase + 2},
      // Associated with the third entry in SampleGroupDescription Box.
      // With Iv size 16 bytes.
      {1, SampleToGroupEntry::kFragmentGroupDescriptionIndexBase + 3}};
  AddCencSampleGroup(&moov_.tracks[1], &moof.tracks[1], kSampleToGroupTable,
                     std::size(kSampleToGroupTable));

  ASSERT_TRUE(iter_->Init(moof));
  // The run for track 2 will be the second, which is parsed according to
  // data_offset.
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->track_id(), 2u);

  EXPECT_TRUE(iter_->is_encrypted());
  // No need to cache aux info as it is already available in SampleEncryption.
  EXPECT_FALSE(iter_->AuxInfoNeedsToBeCached());
  EXPECT_EQ(iter_->aux_info_size(), 0);
  EXPECT_EQ(iter_->sample_offset(), 200);
  EXPECT_EQ(iter_->GetMaxClearOffset(), moof.tracks[1].runs[0].data_offset);
  std::unique_ptr<DecryptConfig> config = iter_->GetDecryptConfig();
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(kIv1), std::size(kIv1)),
            config->iv());
  EXPECT_EQ(config->subsamples().size(), 1u);
  EXPECT_EQ(config->subsamples()[0].clear_bytes, 1u);
  EXPECT_EQ(config->subsamples()[0].cypher_bytes, 2u);
  iter_->AdvanceSample();
  config = iter_->GetDecryptConfig();
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(kIv3), std::size(kIv3)),
            config->iv());
  EXPECT_EQ(config->subsamples().size(), 2u);
  EXPECT_EQ(config->subsamples()[0].clear_bytes, 1u);
  EXPECT_EQ(config->subsamples()[0].cypher_bytes, 2u);
  EXPECT_EQ(config->subsamples()[1].clear_bytes, 3u);
  EXPECT_EQ(config->subsamples()[1].cypher_bytes, 4u);
}

TEST_F(TrackRunIteratorTest, DecryptConfigTestWithAuxInfo) {
  AddEncryption(&moov_.tracks[1]);
  iter_.reset(new TrackRunIterator(&moov_, &media_log_));

  MovieFragment moof = CreateFragment();
  AddAuxInfoHeaders(50, &moof.tracks[1]);

  ASSERT_TRUE(iter_->Init(moof));

  // The run for track 2 will be first, since its aux info offset is the first
  // element in the file.
  EXPECT_EQ(iter_->track_id(), 2u);
  EXPECT_TRUE(iter_->is_encrypted());
  ASSERT_TRUE(iter_->AuxInfoNeedsToBeCached());
  EXPECT_EQ(static_cast<uint32_t>(iter_->aux_info_size()), std::size(kAuxInfo));
  EXPECT_EQ(iter_->aux_info_offset(), 50);
  EXPECT_EQ(iter_->GetMaxClearOffset(), 50);
  EXPECT_FALSE(iter_->CacheAuxInfo(NULL, 0));
  EXPECT_FALSE(iter_->CacheAuxInfo(kAuxInfo, 3));
  EXPECT_TRUE(iter_->AuxInfoNeedsToBeCached());
  EXPECT_TRUE(iter_->CacheAuxInfo(kAuxInfo, std::size(kAuxInfo)));
  EXPECT_FALSE(iter_->AuxInfoNeedsToBeCached());
  EXPECT_EQ(iter_->sample_offset(), 200);
  EXPECT_EQ(iter_->GetMaxClearOffset(), moof.tracks[0].runs[0].data_offset);
  std::unique_ptr<DecryptConfig> config = iter_->GetDecryptConfig();
  EXPECT_EQ(
      std::string(reinterpret_cast<const char*>(kKeyId), std::size(kKeyId)),
      config->key_id());
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(kIv1), std::size(kIv1)),
            config->iv());
  EXPECT_TRUE(config->subsamples().empty());
  iter_->AdvanceSample();
  config = iter_->GetDecryptConfig();
  EXPECT_EQ(config->subsamples().size(), 2u);
  EXPECT_EQ(config->subsamples()[0].clear_bytes, 1u);
  EXPECT_EQ(config->subsamples()[1].cypher_bytes, 4u);
}

TEST_F(TrackRunIteratorTest, CencSampleGroupTest) {
  MovieFragment moof = CreateFragment();

  const SampleToGroupEntry kSampleToGroupTable[] = {
      // Associated with the second entry in SampleGroupDescription Box.
      {1, SampleToGroupEntry::kFragmentGroupDescriptionIndexBase + 2},
      // Associated with the first entry in SampleGroupDescription Box.
      {1, SampleToGroupEntry::kFragmentGroupDescriptionIndexBase + 1}};
  AddCencSampleGroup(&moov_.tracks[0], &moof.tracks[0], kSampleToGroupTable,
                     std::size(kSampleToGroupTable));

  iter_.reset(new TrackRunIterator(&moov_, &media_log_));
  ASSERT_TRUE(InitMoofWithArbitraryAuxInfo(&moof));

  std::string cenc_sample_group_key_id(
      kFragmentCencSampleGroupKeyId,
      kFragmentCencSampleGroupKeyId + std::size(kFragmentCencSampleGroupKeyId));
  // The first sample is encrypted and the second sample is unencrypted.
  EXPECT_TRUE(iter_->is_encrypted());
  EXPECT_EQ(cenc_sample_group_key_id, iter_->GetDecryptConfig()->key_id());
  iter_->AdvanceSample();
  EXPECT_FALSE(iter_->is_encrypted());
}

TEST_F(TrackRunIteratorTest, CencSampleGroupWithTrackEncryptionBoxTest) {
  // Add TrackEncryption Box.
  AddEncryption(&moov_.tracks[0]);

  MovieFragment moof = CreateFragment();

  const SampleToGroupEntry kSampleToGroupTable[] = {
      // Associated with the 2nd entry in fragment SampleGroupDescription Box.
      {2, SampleToGroupEntry::kFragmentGroupDescriptionIndexBase + 2},
      // Associated with the default values specified in TrackEncryption Box.
      {1, 0},
      // Associated with the 1st entry in fragment SampleGroupDescription Box.
      {3, SampleToGroupEntry::kFragmentGroupDescriptionIndexBase + 1},
      // Associated with the 1st entry in track SampleGroupDescription Box.
      {2, 1}};
  AddCencSampleGroup(&moov_.tracks[0], &moof.tracks[0], kSampleToGroupTable,
                     std::size(kSampleToGroupTable));

  iter_.reset(new TrackRunIterator(&moov_, &media_log_));
  ASSERT_TRUE(InitMoofWithArbitraryAuxInfo(&moof));

  std::string track_encryption_key_id(kKeyId, kKeyId + std::size(kKeyId));
  std::string track_cenc_sample_group_key_id(
      kTrackCencSampleGroupKeyId,
      kTrackCencSampleGroupKeyId + std::size(kTrackCencSampleGroupKeyId));
  std::string fragment_cenc_sample_group_key_id(
      kFragmentCencSampleGroupKeyId,
      kFragmentCencSampleGroupKeyId + std::size(kFragmentCencSampleGroupKeyId));

  for (size_t i = 0; i < kSampleToGroupTable[0].sample_count; ++i) {
    EXPECT_TRUE(iter_->is_encrypted());
    EXPECT_EQ(fragment_cenc_sample_group_key_id,
              iter_->GetDecryptConfig()->key_id());
    iter_->AdvanceSample();
  }

  for (size_t i = 0; i < kSampleToGroupTable[1].sample_count; ++i) {
    EXPECT_TRUE(iter_->is_encrypted());
    EXPECT_EQ(track_encryption_key_id, iter_->GetDecryptConfig()->key_id());
    iter_->AdvanceSample();
  }

  for (size_t i = 0; i < kSampleToGroupTable[2].sample_count; ++i) {
    EXPECT_FALSE(iter_->is_encrypted());
    iter_->AdvanceSample();
  }

  for (size_t i = 0; i < kSampleToGroupTable[3].sample_count; ++i) {
    EXPECT_TRUE(iter_->is_encrypted());
    EXPECT_EQ(track_cenc_sample_group_key_id,
              iter_->GetDecryptConfig()->key_id());
    iter_->AdvanceSample();
  }

  // The remaining samples should be associated with the default values
  // specified in TrackEncryption Box.
  EXPECT_TRUE(iter_->is_encrypted());
  EXPECT_EQ(track_encryption_key_id, iter_->GetDecryptConfig()->key_id());
}

// It is legal for aux info blocks to be shared among multiple formats.
TEST_F(TrackRunIteratorTest, SharedAuxInfoTest) {
  AddEncryption(&moov_.tracks[0]);
  AddEncryption(&moov_.tracks[1]);
  iter_.reset(new TrackRunIterator(&moov_, &media_log_));

  MovieFragment moof = CreateFragment();
  moof.tracks[0].runs.resize(1);
  AddAuxInfoHeaders(50, &moof.tracks[0]);
  AddAuxInfoHeaders(50, &moof.tracks[1]);
  moof.tracks[0].auxiliary_size.default_sample_info_size = 8;

  ASSERT_TRUE(iter_->Init(moof));
  EXPECT_EQ(iter_->track_id(), 1u);
  EXPECT_EQ(iter_->aux_info_offset(), 50);
  EXPECT_TRUE(iter_->CacheAuxInfo(kAuxInfo, std::size(kAuxInfo)));
  std::unique_ptr<DecryptConfig> config = iter_->GetDecryptConfig();
  ASSERT_EQ(std::size(kIv1), config->iv().size());
  EXPECT_TRUE(!memcmp(kIv1, config->iv().data(), config->iv().size()));
  iter_->AdvanceSample();
  EXPECT_EQ(iter_->GetMaxClearOffset(), 50);
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->GetMaxClearOffset(), 50);
  EXPECT_EQ(iter_->aux_info_offset(), 50);
  EXPECT_TRUE(iter_->CacheAuxInfo(kAuxInfo, std::size(kAuxInfo)));
  EXPECT_EQ(iter_->GetMaxClearOffset(), 200);
  ASSERT_EQ(std::size(kIv1), config->iv().size());
  EXPECT_TRUE(!memcmp(kIv1, config->iv().data(), config->iv().size()));
  iter_->AdvanceSample();
  EXPECT_EQ(iter_->GetMaxClearOffset(), 201);
}

// Sensible files are expected to place auxiliary information for a run
// immediately before the main data for that run. Alternative schemes are
// possible, however, including the somewhat reasonable behavior of placing all
// aux info at the head of the 'mdat' box together, and the completely
// unreasonable behavior demonstrated here:
//  byte 50: track 2, run 1 aux info
//  byte 100: track 1, run 1 data
//  byte 200: track 2, run 1 data
//  byte 201: track 1, run 2 aux info (*inside* track 2, run 1 data)
//  byte 10000: track 1, run 2 data
//  byte 20000: track 1, run 1 aux info
TEST_F(TrackRunIteratorTest, UnexpectedOrderingTest) {
  AddEncryption(&moov_.tracks[0]);
  AddEncryption(&moov_.tracks[1]);
  iter_.reset(new TrackRunIterator(&moov_, &media_log_));

  MovieFragment moof = CreateFragment();
  AddAuxInfoHeaders(20000, &moof.tracks[0]);
  moof.tracks[0].auxiliary_offset.offsets.push_back(201);
  moof.tracks[0].auxiliary_size.sample_count += 2;
  moof.tracks[0].auxiliary_size.default_sample_info_size = 8;
  moof.tracks[0].runs[1].sample_count = 2;
  AddAuxInfoHeaders(50, &moof.tracks[1]);
  moof.tracks[1].runs[0].sample_sizes[0] = 5;

  ASSERT_TRUE(iter_->Init(moof));
  EXPECT_EQ(iter_->track_id(), 2u);
  EXPECT_EQ(iter_->aux_info_offset(), 50);
  EXPECT_EQ(iter_->sample_offset(), 200);
  EXPECT_TRUE(iter_->CacheAuxInfo(kAuxInfo, std::size(kAuxInfo)));
  EXPECT_EQ(iter_->GetMaxClearOffset(), 100);
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->track_id(), 1u);
  EXPECT_EQ(iter_->aux_info_offset(), 20000);
  EXPECT_EQ(iter_->sample_offset(), 100);
  EXPECT_TRUE(iter_->CacheAuxInfo(kAuxInfo, std::size(kAuxInfo)));
  EXPECT_EQ(iter_->GetMaxClearOffset(), 100);
  iter_->AdvanceSample();
  EXPECT_EQ(iter_->GetMaxClearOffset(), 101);
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->track_id(), 1u);
  EXPECT_EQ(iter_->aux_info_offset(), 201);
  EXPECT_EQ(iter_->sample_offset(), 10000);
  EXPECT_EQ(iter_->GetMaxClearOffset(), 201);
  EXPECT_TRUE(iter_->CacheAuxInfo(kAuxInfo, std::size(kAuxInfo)));
  EXPECT_EQ(iter_->GetMaxClearOffset(), 10000);
}

TEST_F(TrackRunIteratorTest, KeyFrameFlagCombinations) {
  // Setup both audio and video tracks to each have 6 samples covering all the
  // combinations of mp4 "sync sample" and "depends on" relationships.
  MovieFragment moof = CreateFragment();
  moof.tracks[0].runs.resize(1);
  moof.tracks[1].runs.resize(1);
  moof.tracks[0].runs[0].sample_count = 6;
  moof.tracks[1].runs[0].sample_count = 6;
  SetFlagsOnSamples("US UN OS ON NS NN", &moof.tracks[0].runs[0]);
  SetFlagsOnSamples("US UN OS ON NS NN", &moof.tracks[1].runs[0]);
  iter_.reset(new TrackRunIterator(&moov_, &media_log_));

  ASSERT_TRUE(iter_->Init(moof));
  EXPECT_TRUE(iter_->IsRunValid());

  // Keyframes should be marked according to downstream's expectations that
  // keyframes serve as points of random access for seeking.

  // For audio, any sync sample should be marked as a key frame. Whether a
  // sample "depends on" other samples is not considered. Unlike video samples,
  // audio samples are often marked as depending on other samples but are still
  // workable for random access. While we allow for parsing of audio samples
  // that are non-sync samples, we generally expect all audio samples to be sync
  // samples and downstream will log and discard any non-sync audio samples.
  EXPECT_EQ("1 K P K P K P", KeyframeAndRAPInfo(iter_.get()));

  iter_->AdvanceRun();

  // For video, any key frame should be both a sync sample and have no known
  // dependents. Ideally, a video sync sample should always be marked as having
  // no dependents, but we occasionally encounter media where all samples are
  // marked "sync" and we must rely on combining the two flags to pick out the
  // true key frames. See http://crbug.com/310712 and http://crbug.com/507916.
  // Realiably knowing the keyframes for video is also critical to SPS PPS
  // insertion.
  EXPECT_EQ("2 K P P P K P", KeyframeAndRAPInfo(iter_.get()));
}

TEST_F(TrackRunIteratorTest, DecryptConfigTestWithConstantIvNoAuxInfo) {
  AddEncryptionCbcs(&moov_.tracks[1]);
  iter_.reset(new TrackRunIterator(&moov_, &media_log_));

  MovieFragment moof = CreateFragment();

  ASSERT_TRUE(iter_->Init(moof));

  // The run for track 2 will be the second.
  iter_->AdvanceRun();
  EXPECT_EQ(iter_->track_id(), 2u);
  EXPECT_TRUE(iter_->is_encrypted());
  ASSERT_FALSE(iter_->AuxInfoNeedsToBeCached());
  EXPECT_EQ(iter_->sample_offset(), 200);
  std::unique_ptr<DecryptConfig> config = iter_->GetDecryptConfig();
  EXPECT_EQ(
      std::string(reinterpret_cast<const char*>(kKeyId), std::size(kKeyId)),
      config->key_id());
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(kIv3), std::size(kIv3)),
            config->iv());
  EXPECT_TRUE(config->subsamples().empty());
  iter_->AdvanceSample();
  config = iter_->GetDecryptConfig();
  EXPECT_EQ(
      std::string(reinterpret_cast<const char*>(kKeyId), std::size(kKeyId)),
      config->key_id());
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(kIv3), std::size(kIv3)),
            config->iv());
  EXPECT_TRUE(config->subsamples().empty());
}

TEST_F(TrackRunIteratorTest, DecryptConfigTestWithSampleGroupsAndConstantIv) {
  // Add TrackEncryption Box.
  AddEncryptionCbcs(&moov_.tracks[1]);

  MovieFragment moof = CreateFragment();
  AddSampleEncryptionCbcs(&moof.tracks[1]);

  const SampleToGroupEntry kSampleToGroupTable[] = {
      // Associated with the 2nd entry in fragment SampleGroupDescription Box.
      {1, SampleToGroupEntry::kFragmentGroupDescriptionIndexBase + 2},
      // Associated with the default values specified in TrackEncryption Box.
      {1, 0},
      // Associated with the 1st entry in fragment SampleGroupDescription Box.
      {1, SampleToGroupEntry::kFragmentGroupDescriptionIndexBase + 1},
      // Associated with the 1st entry in track SampleGroupDescription Box.
      {1, 1}};
  AddCencSampleGroup(&moov_.tracks[1], &moof.tracks[1], kSampleToGroupTable,
                     std::size(kSampleToGroupTable));
  AddConstantIvsToCencSampleGroup(&moov_.tracks[1], &moof.tracks[1]);
  iter_.reset(new TrackRunIterator(&moov_, &media_log_));
  ASSERT_TRUE(iter_->Init(moof));

  // The run for track 2 will be the second.
  iter_->AdvanceRun();

  std::string track_encryption_iv(kIv3, kIv3 + std::size(kIv3));
  std::string track_cenc_sample_group_iv(kIv4, kIv4 + std::size(kIv4));
  std::string fragment_cenc_sample_group_iv(kIv5, kIv5 + std::size(kIv5));

  for (size_t i = 0; i < kSampleToGroupTable[0].sample_count; ++i) {
    EXPECT_TRUE(iter_->is_encrypted());
    EXPECT_EQ(fragment_cenc_sample_group_iv, iter_->GetDecryptConfig()->iv());
    iter_->AdvanceSample();
  }

  for (size_t i = 0; i < kSampleToGroupTable[1].sample_count; ++i) {
    EXPECT_TRUE(iter_->is_encrypted());
    EXPECT_EQ(track_encryption_iv, iter_->GetDecryptConfig()->iv());
    iter_->AdvanceSample();
  }

  for (size_t i = 0; i < kSampleToGroupTable[2].sample_count; ++i) {
    EXPECT_FALSE(iter_->is_encrypted());
    iter_->AdvanceSample();
  }

  for (size_t i = 0; i < kSampleToGroupTable[3].sample_count; ++i) {
    EXPECT_TRUE(iter_->is_encrypted());
    EXPECT_EQ(track_cenc_sample_group_iv, iter_->GetDecryptConfig()->iv());
    iter_->AdvanceSample();
  }

  // The remaining samples should be associated with the default values
  // specified in TrackEncryption Box.
  EXPECT_TRUE(iter_->is_encrypted());
  EXPECT_EQ(track_encryption_iv, iter_->GetDecryptConfig()->iv());
}

}  // namespace mp4
}  // namespace media
