// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/mapped_file.h"

#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

class MappedFileWriterTest : public testing::Test {
 protected:
  MappedFileWriterTest() = default;
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_path_ = temp_dir_.GetPath().AppendASCII("test-file");
  }

  base::FilePath file_path_;

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(MappedFileWriterTest, Keep) {
  EXPECT_FALSE(base::PathExists(file_path_));
  {
    using base::File;
    File file(file_path_, File::FLAG_CREATE_ALWAYS | File::FLAG_READ |
                              File::FLAG_WRITE | File::FLAG_WIN_SHARE_DELETE |
                              File::FLAG_CAN_DELETE_ON_CLOSE);
    MappedFileWriter file_writer(file_path_, std::move(file), 10);
    EXPECT_FALSE(file_writer.HasError());
    EXPECT_TRUE(file_writer.Keep());
    EXPECT_FALSE(file_writer.HasError());
    EXPECT_TRUE(file_writer.error().empty());
  }
  EXPECT_TRUE(base::PathExists(file_path_));
}

TEST_F(MappedFileWriterTest, DeleteOnClose) {
  EXPECT_FALSE(base::PathExists(file_path_));
  {
    using base::File;
    File file(file_path_, File::FLAG_CREATE_ALWAYS | File::FLAG_READ |
                              File::FLAG_WRITE | File::FLAG_WIN_SHARE_DELETE |
                              File::FLAG_CAN_DELETE_ON_CLOSE);
    MappedFileWriter file_writer(file_path_, std::move(file), 10);
    EXPECT_FALSE(file_writer.HasError());
    EXPECT_TRUE(file_writer.error().empty());
  }
  EXPECT_FALSE(base::PathExists(file_path_));
}

}  // namespace zucchini
