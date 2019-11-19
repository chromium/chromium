// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/json_file_sanitizer.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class JsonFileSanitizerTest : public testing::Test {
 public:
  JsonFileSanitizerTest() {}

 protected:
  base::FilePath CreateFilePath(const base::FilePath::StringType& file_name) {
    return temp_dir_.GetPath().Append(file_name);
  }

  void CreateValidJsonFile(const base::FilePath& path) {
    std::string kJson = "{\"hello\":\"bonjour\"}";
    ASSERT_EQ(static_cast<int>(kJson.size()),
              base::WriteFile(path, kJson.data(), kJson.size()));
  }

  void CreateInvalidJsonFile(const base::FilePath& path) {
    std::string kJson = "sjkdsk;'<?js";
    ASSERT_EQ(static_cast<int>(kJson.size()),
              base::WriteFile(path, kJson.data(), kJson.size()));
  }

  const base::FilePath& GetJsonFilePath() const { return temp_dir_.GetPath(); }

  void WaitForSanitizationDone() {
    ASSERT_FALSE(done_callback_);
    base::RunLoop run_loop;
    done_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void CreateAndStartSanitizer(const std::set<base::FilePath>& file_paths) {
    sanitizer_ = JsonFileSanitizer::CreateAndStart(
        &data_decoder_, file_paths,
        base::BindOnce(&JsonFileSanitizerTest::SanitizationDone,
                       base::Unretained(this)));
  }

  JsonFileSanitizer::Status last_reported_status() const {
    return last_status_;
  }

  const std::string& last_reported_error() const { return last_error_; }

 private:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void SanitizationDone(JsonFileSanitizer::Status status,
                        const std::string& error_msg) {
    last_status_ = status;
    last_error_ = error_msg;
    if (done_callback_)
      std::move(done_callback_).Run();
  }

  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  data_decoder::DataDecoder data_decoder_;
  JsonFileSanitizer::Status last_status_;
  std::string last_error_;
  base::OnceClosure done_callback_;
  std::unique_ptr<JsonFileSanitizer> sanitizer_;
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(JsonFileSanitizerTest);
};

}  // namespace

TEST_F(JsonFileSanitizerTest, NoFilesProvided) {
  CreateAndStartSanitizer(std::set<base::FilePath>());
  WaitForSanitizationDone();
  EXPECT_EQ(last_reported_status(), JsonFileSanitizer::Status::kSuccess);
  EXPECT_TRUE(last_reported_error().empty());
}

TEST_F(JsonFileSanitizerTest, ValidCase) {
  constexpr std::array<const base::FilePath::CharType* const, 10> kFileNames{
      {FILE_PATH_LITERAL("test0"), FILE_PATH_LITERAL("test1"),
       FILE_PATH_LITERAL("test2"), FILE_PATH_LITERAL("test3"),
       FILE_PATH_LITERAL("test4"), FILE_PATH_LITERAL("test5"),
       FILE_PATH_LITERAL("test6"), FILE_PATH_LITERAL("test7"),
       FILE_PATH_LITERAL("test8"), FILE_PATH_LITERAL("test9")}};
  std::set<base::FilePath> paths;
  for (const base::FilePath::CharType* file_name : kFileNames) {
    base::FilePath path = CreateFilePath(file_name);
    CreateValidJsonFile(path);
    paths.insert(path);
  }
  CreateAndStartSanitizer(paths);
  WaitForSanitizationDone();
  EXPECT_EQ(last_reported_status(), JsonFileSanitizer::Status::kSuccess);
  EXPECT_TRUE(last_reported_error().empty());
  // Make sure the JSON files are there and non empty.
  for (const auto& path : paths) {
    int64_t file_size = 0;
    EXPECT_TRUE(base::GetFileSize(path, &file_size));
    EXPECT_GT(file_size, 0);
  }
}

TEST_F(JsonFileSanitizerTest, MissingJsonFile) {
  constexpr base::FilePath::CharType kGoodName[] =
      FILE_PATH_LITERAL("i_exists");
  constexpr base::FilePath::CharType kNonExistingName[] =
      FILE_PATH_LITERAL("i_don_t_exist");
  base::FilePath good_path = CreateFilePath(kGoodName);
  CreateValidJsonFile(good_path);
  base::FilePath invalid_path = CreateFilePath(kNonExistingName);
  CreateAndStartSanitizer({good_path, invalid_path});
  WaitForSanitizationDone();
  EXPECT_EQ(last_reported_status(), JsonFileSanitizer::Status::kFileReadError);
}

TEST_F(JsonFileSanitizerTest, InvalidJson) {
  constexpr base::FilePath::CharType kGoodJsonFileName[] =
      FILE_PATH_LITERAL("good.json");
  constexpr base::FilePath::CharType kBadJsonFileName[] =
      FILE_PATH_LITERAL("bad.json");
  base::FilePath good_path = CreateFilePath(kGoodJsonFileName);
  CreateValidJsonFile(good_path);
  base::FilePath badd_path = CreateFilePath(kBadJsonFileName);
  CreateInvalidJsonFile(badd_path);
  CreateAndStartSanitizer({good_path, badd_path});
  WaitForSanitizationDone();
  EXPECT_EQ(last_reported_status(), JsonFileSanitizer::Status::kDecodingError);
  EXPECT_FALSE(last_reported_error().empty());
}

}  // namespace extensions
