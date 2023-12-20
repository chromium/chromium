// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/local_file_stream_reader.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "storage/browser/file_system/file_stream_reader_test.h"
#include "storage/browser/file_system/file_stream_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include "base/files/scoped_file.h"
#endif

namespace storage {

namespace {

using ::testing::_;

file_access::ScopedFileAccess CreateScopedFileAccess(bool allowed) {
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return file_access::ScopedFileAccess(allowed, base::ScopedFD());
#else
  return file_access::ScopedFileAccess(allowed);
#endif
}

}  // namespace

class LocalFileStreamReaderTest : public FileStreamReaderTest {
 public:
  LocalFileStreamReaderTest() : file_thread_("TestFileThread") {}

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    ASSERT_TRUE(file_thread_.Start());
  }

  void TearDown() override {
    // Give another chance for deleted streams to perform Close.
    base::RunLoop().RunUntilIdle();
    file_thread_.Stop();
    base::RunLoop().RunUntilIdle();
  }
  std::unique_ptr<FileStreamReader> CreateFileReader(
      const std::string& file_name,
      int64_t initial_offset,
      const base::Time& expected_modification_time) override {
    return CreateFileReader(file_name, initial_offset,
                            expected_modification_time, base::NullCallback());
  }

  std::unique_ptr<FileStreamReader> CreateFileReader(
      const std::string& file_name,
      int64_t initial_offset,
      const base::Time& expected_modification_time,
      file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
          file_access) {
    return FileStreamReader::CreateForLocalFile(
        file_task_runner(), test_dir().AppendASCII(file_name), initial_offset,
        expected_modification_time, std::move(file_access));
  }

  void WriteFile(const std::string& file_name,
                 std::string_view data,
                 base::Time* modification_time) override {
    base::FilePath path = test_dir().AppendASCII(file_name);
    base::WriteFile(path, data);

    base::File::Info file_info;
    ASSERT_TRUE(base::GetFileInfo(path, &file_info));
    if (modification_time)
      *modification_time = file_info.last_modified;
  }

  void TouchFile(const std::string& file_name, base::TimeDelta delta) override {
    base::FilePath path = test_dir().AppendASCII(file_name);
    base::File::Info file_info;
    ASSERT_TRUE(base::GetFileInfo(path, &file_info));
    ASSERT_TRUE(base::TouchFile(path, file_info.last_accessed,
                                file_info.last_modified + delta));
  }

  void EnsureFileTaskFinished() override {
    base::RunLoop run_loop;
    file_task_runner()->PostTaskAndReply(FROM_HERE, base::DoNothing(),

                                         run_loop.QuitClosure());
    run_loop.Run();
  }

  base::FilePath test_dir() const { return dir_.GetPath(); }

  base::SingleThreadTaskRunner* file_task_runner() const {
    return file_thread_.task_runner().get();
  }

 private:
  base::ScopedTempDir dir_;
  base::Thread file_thread_;
};

INSTANTIATE_TYPED_TEST_SUITE_P(Local,
                               FileStreamReaderTypedTest,
                               LocalFileStreamReaderTest);

// TODO(b/265908846): Replace direct call to
// file_access::ScopedFileAccessDelegate with getting access through a callback.
TEST_F(LocalFileStreamReaderTest, ReadAllowedByDataLeakPrevention) {
  this->WriteTestFile();
  std::unique_ptr<FileStreamReader> reader(
      this->CreateFileReader(std::string(this->kTestFileName), 0,
                             this->test_file_modification_time()));

  base::MockRepeatingCallback<void(
      const std::vector<base::FilePath>&,
      base::OnceCallback<void(file_access::ScopedFileAccess)>)>
      callback;
  file_access::ScopedFileAccessDelegate::
      ScopedRequestFilesAccessCallbackForTesting file_access_callback(
          callback.Get());
  EXPECT_CALL(
      callback,
      Run(testing::ElementsAre(test_dir().AppendASCII(kTestFileName)), _))
      .WillOnce(base::test::RunOnceCallback<1>(CreateScopedFileAccess(true)));

  auto data_or_error =
      ReadFromReader(*reader, /*bytes_to_read=*/this->kTestData.size());
  ASSERT_TRUE(data_or_error.has_value());
  EXPECT_EQ(data_or_error.value(), this->kTestData);
}

// TODO(b/265908846): Replace direct call to
// file_access::ScopedFileAccessDelegate with getting access through a callback.
TEST_F(LocalFileStreamReaderTest, ReadBlockedByDataLeakPrevention) {
  this->WriteTestFile();
  std::unique_ptr<FileStreamReader> reader(
      this->CreateFileReader(std::string(this->kTestFileName), 0,
                             this->test_file_modification_time()));

  base::MockRepeatingCallback<void(
      const std::vector<base::FilePath>&,
      base::OnceCallback<void(file_access::ScopedFileAccess)>)>
      callback;
  file_access::ScopedFileAccessDelegate::
      ScopedRequestFilesAccessCallbackForTesting file_access_callback(
          callback.Get());
  EXPECT_CALL(
      callback,
      Run(testing::ElementsAre(test_dir().AppendASCII(kTestFileName)), _))
      .WillOnce(base::test::RunOnceCallback<1>(CreateScopedFileAccess(false)));

  auto data_or_error =
      ReadFromReader(*reader, /*bytes_to_read=*/this->kTestData.size());
  ASSERT_FALSE(data_or_error.has_value());
  EXPECT_EQ(data_or_error.error(), net::ERR_ACCESS_DENIED);
}

TEST_F(LocalFileStreamReaderTest, ReadAllowedByDataLeakPreventionCallback) {
  this->WriteTestFile();
  base::MockRepeatingCallback<void(
      const std::vector<base::FilePath>&,
      base::OnceCallback<void(file_access::ScopedFileAccess)>)>
      callback;
  EXPECT_CALL(
      callback,
      Run(testing::ElementsAre(test_dir().AppendASCII(kTestFileName)), _))
      .WillOnce(base::test::RunOnceCallback<1>(CreateScopedFileAccess(true)));

  std::unique_ptr<FileStreamReader> reader(this->CreateFileReader(
      std::string(this->kTestFileName), 0, this->test_file_modification_time(),
      callback.Get()));

  auto data_or_error =
      ReadFromReader(*reader, /*bytes_to_read=*/this->kTestData.size());
  ASSERT_TRUE(data_or_error.has_value());
  EXPECT_EQ(data_or_error.value(), this->kTestData);
}

TEST_F(LocalFileStreamReaderTest, ReadBlockedByDataLeakPreventionCallback) {
  this->WriteTestFile();
  base::MockRepeatingCallback<void(
      const std::vector<base::FilePath>&,
      base::OnceCallback<void(file_access::ScopedFileAccess)>)>
      callback;
  file_access::ScopedFileAccessDelegate::
      ScopedRequestFilesAccessCallbackForTesting file_access_callback(
          callback.Get());
  EXPECT_CALL(
      callback,
      Run(testing::ElementsAre(test_dir().AppendASCII(kTestFileName)), _))
      .WillOnce(base::test::RunOnceCallback<1>(CreateScopedFileAccess(false)));
  std::unique_ptr<FileStreamReader> reader(this->CreateFileReader(
      std::string(this->kTestFileName), 0, this->test_file_modification_time(),
      callback.Get()));

  auto data_or_error =
      ReadFromReader(*reader, /*bytes_to_read=*/this->kTestData.size());
  ASSERT_FALSE(data_or_error.has_value());
  EXPECT_EQ(data_or_error.error(), net::ERR_ACCESS_DENIED);
}

}  // namespace storage
