// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/memory_file_stream_writer.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/file_system/file_stream_test_utils.h"
#include "storage/browser/file_system/file_stream_writer_test.h"
#include "storage/browser/file_system/obfuscated_file_util_memory_delegate.h"

namespace storage {

class MemoryFileStreamWriterTest : public FileStreamWriterTest {
 public:
  MemoryFileStreamWriterTest() = default;

  void SetUp() override {
    ASSERT_TRUE(file_system_directory_.CreateUniqueTempDir());
    file_util_ = std::make_unique<ObfuscatedFileUtilMemoryDelegate>(
        file_system_directory_.GetPath());
  }

  void TearDown() override {
    // In memory operations should not have any residue in file system
    // directory.
    EXPECT_TRUE(base::IsDirectoryEmpty(file_system_directory_.GetPath()));
  }

  ObfuscatedFileUtilMemoryDelegate* file_util() { return file_util_.get(); }

  base::FilePath Path(const std::string& name) {
    return file_system_directory_.GetPath().AppendASCII(name);
  }

 protected:
  bool CreateFileWithContent(const std::string& name,
                             const std::string& data) override {
    return file_util()->CreateFileForTesting(Path(name), data) ==
           base::File::FILE_OK;
  }

  std::unique_ptr<FileStreamWriter> CreateWriter(const std::string& name,
                                                 int64_t offset) override {
    return std::make_unique<MemoryFileStreamWriter>(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        file_util_->GetWeakPtr(), Path(name), offset);
  }

  bool FilePathExists(const std::string& name) override {
    return file_util()->PathExists(Path(name));
  }

  std::string GetFileContent(const std::string& name) override {
    base::FilePath path = Path(name);
    base::File::Info info;
    EXPECT_EQ(base::File::FILE_OK, file_util()->GetFileInfo(path, &info));

    auto content = base::MakeRefCounted<net::IOBufferWithSize>(
        static_cast<size_t>(info.size));
    EXPECT_EQ(info.size,
              file_util_->ReadFile(path, 0, content.get(), info.size));

    return std::string(content->data(), info.size);
  }

 private:
  base::ScopedTempDir file_system_directory_;
  std::unique_ptr<ObfuscatedFileUtilMemoryDelegate> file_util_;
};

INSTANTIATE_TYPED_TEST_SUITE_P(Memory,
                               FileStreamWriterTypedTest,
                               MemoryFileStreamWriterTest);

}  // namespace storage
