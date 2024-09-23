// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/file_reader.h"

#include <limits>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/extension_resource.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class FileReaderTest : public testing::Test {
 public:
  FileReaderTest() {}

  FileReaderTest(const FileReaderTest&) = delete;
  FileReaderTest& operator=(const FileReaderTest&) = delete;

 private:
  base::test::TaskEnvironment task_environment_;
};

class Receiver {
 public:
  explicit Receiver(
      std::vector<ExtensionResource> resources,
      size_t max_resources_length = std::numeric_limits<size_t>::max())
      : file_reader_(base::MakeRefCounted<FileReader>(
            std::move(resources),
            max_resources_length,
            FileReader::OptionalFileSequenceTask(),
            base::BindOnce(&Receiver::DidReadFile, base::Unretained(this)))) {}

  Receiver(const Receiver&) = delete;
  Receiver& operator=(const Receiver&) = delete;

  void Run() {
    file_reader_->Start();
    run_loop_.Run();
  }

  // Removes the pointer indirection from the read data for use with
  // comparators.
  std::vector<std::string> GetStringData() const {
    std::vector<std::string> string_data;
    string_data.reserve(data_.size());
    for (const auto& entry : data_) {
      EXPECT_TRUE(entry);
      string_data.push_back(*entry);
    }
    return string_data;
  }

  const std::optional<std::string>& error() const { return error_; }
  bool succeeded() const { return !error_; }
  const std::vector<std::unique_ptr<std::string>>& data() const {
    return data_;
  }

 private:
  void DidReadFile(std::vector<std::unique_ptr<std::string>> data,
                   std::optional<std::string> error) {
    error_ = std::move(error);
    data_ = std::move(data);
    run_loop_.QuitWhenIdle();
  }

  std::optional<std::string> error_;
  std::vector<std::unique_ptr<std::string>> data_;
  scoped_refptr<FileReader> file_reader_;
  base::RunLoop run_loop_;
};

void RunBasicTest(const std::vector<std::string>& filenames) {
  base::FilePath root_path;
  base::PathService::Get(DIR_TEST_DATA, &root_path);
  ExtensionId extension_id = crx_file::id_util::GenerateId("test");

  std::vector<ExtensionResource> resources;
  resources.reserve(filenames.size());
  std::vector<std::string> expected_contents;
  expected_contents.reserve(filenames.size());
  for (const auto& filename : filenames) {
    resources.emplace_back(extension_id, root_path,
                           base::FilePath().AppendASCII(filename));

    base::FilePath path = root_path.AppendASCII(filename);
    std::string file_contents;
    ASSERT_TRUE(base::ReadFileToString(path, &file_contents));
    expected_contents.push_back(std::move(file_contents));
  }

  Receiver receiver(resources);
  receiver.Run();

  EXPECT_TRUE(receiver.succeeded()) << *receiver.error();
  EXPECT_THAT(receiver.GetStringData(),
              ::testing::ElementsAreArray(expected_contents));
}

TEST_F(FileReaderTest, SmallFile) {
  RunBasicTest({"smallfile"});
}

TEST_F(FileReaderTest, BiggerFile) {
  RunBasicTest({"bigfile"});
}

TEST_F(FileReaderTest, MultiFile) {
  RunBasicTest({"smallfile", "bigfile"});
}

TEST_F(FileReaderTest, NonExistentFile) {
  base::FilePath path;
  base::PathService::Get(DIR_TEST_DATA, &path);
  ExtensionId extension_id = crx_file::id_util::GenerateId("test");
  ExtensionResource resource(
      extension_id, path,
      base::FilePath(FILE_PATH_LITERAL("file_that_does_not_exist")));

  Receiver receiver({resource});
  receiver.Run();

  EXPECT_FALSE(receiver.succeeded());
  EXPECT_EQ("Could not load file: 'file_that_does_not_exist'.",
            *receiver.error());
}

TEST_F(FileReaderTest, AboveSizeLimitFile) {
  base::FilePath path;
  base::PathService::Get(DIR_TEST_DATA, &path);
  ExtensionId extension_id = crx_file::id_util::GenerateId("test");

  ExtensionResource resource(extension_id, path,
                             base::FilePath().AppendASCII("bigfile"));

  Receiver receiver({resource}, /*max_resources_length=*/100u);
  receiver.Run();

  EXPECT_FALSE(receiver.succeeded());
  EXPECT_EQ("Could not load file: 'bigfile'. Resource size exceeded.",
            *receiver.error());
}

}  // namespace extensions
