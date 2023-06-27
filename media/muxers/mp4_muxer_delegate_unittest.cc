// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/test/task_environment.h"
#include "media/muxers/mp4_muxer_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class Mp4MuxerDelegateTest : public testing::Test {
 public:
  Mp4MuxerDelegateTest() = default;

 protected:
  void LoadVideo(base::StringPiece filename) {
    base::FilePath file_path = GetTestDataFilePath(filename);
    ASSERT_TRUE(video_stream_.Initialize(file_path))
        << "Couldn't open stream file: " << file_path.MaybeAsASCII();
  }

  base::StringPiece GetStreamData() const {
    return base::StringPiece(
        reinterpret_cast<const char*>(video_stream_.data()),
        video_stream_.length());
  }

 private:
  base::FilePath GetTestDataFilePath(base::StringPiece name) {
    base::FilePath file_path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
    file_path = file_path.Append(FILE_PATH_LITERAL("media"))
                    .Append(FILE_PATH_LITERAL("test"))
                    .Append(FILE_PATH_LITERAL("data"))
                    .AppendASCII(name);
    return file_path;
  }

  base::test::TaskEnvironment task_environment;
  base::MemoryMappedFile video_stream_;
};

TEST_F(Mp4MuxerDelegateTest, AddVideoFrame) {
  // TODO: Add test.
}

}  // namespace media
