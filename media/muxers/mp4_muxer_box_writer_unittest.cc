// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/big_endian.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/box_reader.h"
#include "media/formats/mp4/writable_box_definitions.h"
#include "media/muxers/mp4_box_writer.h"
#include "media/muxers/mp4_movie_box_writer.h"
#include "media/muxers/mp4_muxer_context.h"
#include "media/muxers/output_position_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class Mp4MuxerBoxWriterTest : public testing::Test {
 public:
  Mp4MuxerBoxWriterTest() = default;

 protected:
  void CreateContext(std::unique_ptr<OutputPositionTracker> tracker) {
    context_ = std::make_unique<Mp4MuxerContext>(std::move(tracker));
  }

  Mp4MuxerContext* context() const { return context_.get(); }

  void Reset() { context_ = nullptr; }

 private:
  mp4::writable_boxes::Movie mp4_moov_box_;
  std::unique_ptr<Mp4MuxerContext> context_;
  base::test::TaskEnvironment task_environment;
};

TEST_F(Mp4MuxerBoxWriterTest, Mp4MovieAndHeader) {
  // Tests `written_data` and `mvhd` box writer.
  base::RunLoop run_loop;

  std::vector<uint8_t> written_data;
  auto tracker = std::make_unique<OutputPositionTracker>(base::BindRepeating(
      [&](base::OnceClosure run_loop_quit, std::vector<uint8_t>* written_data,
          base::StringPiece mp4_data_string) {
        // Callback is called per box output.

        std::copy(mp4_data_string.begin(), mp4_data_string.end(),
                  std::back_inserter(*written_data));
        std::move(run_loop_quit).Run();
      },
      run_loop.QuitClosure(), &written_data));

  // Initialize.
  CreateContext(std::move(tracker));
  context()->SetVideoIndex(0);

  // Populates the boxes during Mp4Muxer::OnEncodedVideo.
  mp4::writable_boxes::Movie mp4_moov_box;
  base::Time creation_time = base::Time::FromTimeT(0x1234567);
  base::Time modification_time = base::Time::FromTimeT(0x2345678);
  {
    mp4_moov_box.fourcc = mp4::FOURCC_MOOV;
    mp4_moov_box.header.fourcc = mp4::FOURCC_MVHD;
    mp4_moov_box.header.creation_time = creation_time;
    mp4_moov_box.header.modification_time = modification_time;
    mp4_moov_box.header.timescale = 0x7530;
    mp4_moov_box.header.duration = base::Seconds(0);
    mp4_moov_box.header.next_track_id = 2;
  }

  // Flush at requested.
  Mp4MovieBoxWriter box_writer(*context(), mp4_moov_box);
  box_writer.WriteAndFlush();

  // Wait for finishing flush of all boxes.
  run_loop.Run();

  // Validation of the written boxes.

  // `written_data` test.
  std::unique_ptr<mp4::BoxReader> reader;
  mp4::ParseResult result = mp4::BoxReader::ReadTopLevelBox(
      written_data.data(), written_data.size(), nullptr, &reader);

  EXPECT_EQ(result, mp4::ParseResult::kOk);
  EXPECT_TRUE(reader);
  EXPECT_EQ(mp4::FOURCC_MOOV, reader->type());

  // `mvhd` test.
  std::vector<mp4::MovieHeader> mvhd_boxs;
  EXPECT_TRUE(reader->ReadAllChildrenAndCheckFourCC(&mvhd_boxs));
  mp4::MovieHeader mvhd_box = mvhd_boxs[0];
  EXPECT_EQ(mvhd_box.version, 1);

  base::Time time1904;
  CHECK(base::Time::FromUTCString("1904-01-01 00:00:00 UTC", &time1904));
  uint64_t iso_time_creation_time = (creation_time - time1904).InSeconds();
  uint64_t iso_time_modification_time =
      (modification_time - time1904).InSeconds();

  EXPECT_EQ(mvhd_box.creation_time, iso_time_creation_time);
  EXPECT_EQ(mvhd_box.modification_time, iso_time_modification_time);
  EXPECT_EQ(mvhd_box.timescale, 0x7530u);
  EXPECT_EQ(mvhd_box.duration, 0u);
  EXPECT_EQ(mvhd_box.next_track_id, 2u);

  // Once Flush, it needs to reset the internal objects of context and buffer.
  Reset();
}

}  // namespace media
