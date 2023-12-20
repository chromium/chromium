// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/memory_file_stream_reader.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/file_system/file_stream_reader_test.h"
#include "storage/browser/file_system/file_stream_test_utils.h"
#include "storage/browser/file_system/obfuscated_file_util_memory_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

class MemoryFileStreamReaderTest : public FileStreamReaderTest {
 public:
  MemoryFileStreamReaderTest() = default;

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    file_util_ = std::make_unique<ObfuscatedFileUtilMemoryDelegate>(test_dir());
  }

  void TearDown() override {
    // In memory operations should not have any residue in file system
    // directory.
    EXPECT_TRUE(base::IsDirectoryEmpty(test_dir()));
  }

  std::unique_ptr<FileStreamReader> CreateFileReader(
      const std::string& file_name,
      int64_t initial_offset,
      const base::Time& expected_modification_time) override {
    return std::make_unique<MemoryFileStreamReader>(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        file_util_->GetWeakPtr(), test_dir().AppendASCII(file_name),
        initial_offset, expected_modification_time);
  }

  void WriteFile(const std::string& file_name,
                 std::string_view data,
                 base::Time* modification_time) override {
    base::FilePath path = test_dir().AppendASCII(file_name);
    file_util_->CreateFileForTesting(path, data);
    base::File::Info file_info;
    ASSERT_EQ(base::File::FILE_OK, file_util_->GetFileInfo(path, &file_info));
    if (modification_time)
      *modification_time = file_info.last_modified;
  }

  void TouchFile(const std::string& file_name, base::TimeDelta delta) override {
    base::FilePath path = test_dir().AppendASCII(file_name);
    base::File::Info file_info;
    ASSERT_EQ(base::File::FILE_OK, file_util_->GetFileInfo(path, &file_info));
    ASSERT_EQ(base::File::FILE_OK,
              file_util_->Touch(path, file_info.last_accessed,
                                file_info.last_modified + delta));
  }

  base::FilePath test_dir() const { return dir_.GetPath(); }

 private:
  base::ScopedTempDir dir_;
  std::unique_ptr<ObfuscatedFileUtilMemoryDelegate> file_util_;
};

INSTANTIATE_TYPED_TEST_SUITE_P(Memory,
                               FileStreamReaderTypedTest,
                               MemoryFileStreamReaderTest);

}  // namespace storage
