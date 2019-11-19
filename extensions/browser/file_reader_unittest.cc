// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/file_reader.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/extension_resource.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class FileReaderTest : public testing::Test {
 public:
  FileReaderTest() {}

 private:
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(FileReaderTest);
};

class Receiver {
 public:
  Receiver(const ExtensionResource& resource)
      : succeeded_(false),
        file_reader_(new FileReader(
            resource,
            FileReader::OptionalFileSequenceTask(),
            base::Bind(&Receiver::DidReadFile, base::Unretained(this)))) {}

  void Run() {
    file_reader_->Start();
    run_loop_.Run();
  }

  bool succeeded() const { return succeeded_; }
  const std::string& data() const { return *data_; }

 private:
  void DidReadFile(bool success, std::unique_ptr<std::string> data) {
    succeeded_ = success;
    data_ = std::move(data);
    run_loop_.QuitWhenIdle();
  }

  bool succeeded_;
  std::unique_ptr<std::string> data_;
  scoped_refptr<FileReader> file_reader_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(Receiver);
};

void RunBasicTest(const char* filename) {
  base::FilePath path;
  base::PathService::Get(DIR_TEST_DATA, &path);
  std::string extension_id = crx_file::id_util::GenerateId("test");
  ExtensionResource resource(
      extension_id, path, base::FilePath().AppendASCII(filename));
  path = path.AppendASCII(filename);

  std::string file_contents;
  ASSERT_TRUE(base::ReadFileToString(path, &file_contents));

  Receiver receiver(resource);
  receiver.Run();

  EXPECT_TRUE(receiver.succeeded());
  EXPECT_EQ(file_contents, receiver.data());
}

TEST_F(FileReaderTest, SmallFile) {
  RunBasicTest("smallfile");
}

TEST_F(FileReaderTest, BiggerFile) {
  RunBasicTest("bigfile");
}

TEST_F(FileReaderTest, NonExistantFile) {
  base::FilePath path;
  base::PathService::Get(DIR_TEST_DATA, &path);
  std::string extension_id = crx_file::id_util::GenerateId("test");
  ExtensionResource resource(extension_id, path, base::FilePath(
      FILE_PATH_LITERAL("file_that_does_not_exist")));
  path = path.AppendASCII("file_that_does_not_exist");

  Receiver receiver(resource);
  receiver.Run();

  EXPECT_FALSE(receiver.succeeded());
}

}  // namespace extensions
