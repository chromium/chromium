// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/test_token_storage.h"

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"

namespace {
const base::FilePath::CharType kTokenFileName[] =
    FILE_PATH_LITERAL("tokens.json");
const base::FilePath::CharType kRemotingFolder[] =
    FILE_PATH_LITERAL("remoting");
const base::FilePath::CharType kTokenStoreFolder[] =
    FILE_PATH_LITERAL("token_store");
constexpr char kUnspecifiedUsername[] = "unspecified";
constexpr char kRefreshTokenKey[] = "refresh_token";
constexpr char kUserEmailKey[] = "user_email";
constexpr char kAccessTokenKey[] = "access_token";
constexpr char kDeviceIdKey[] = "device_id";
}  // namespace

namespace remoting {
namespace test {

// Provides functionality to write a refresh token to a local folder on disk and
// read it back during subsequent tool runs.
class TestTokenStorageOnDisk : public TestTokenStorage {
 public:
  TestTokenStorageOnDisk(const std::string& user_name,
                         const base::FilePath& tokens_file_path);
  ~TestTokenStorageOnDisk() override;

  // TestTokenStorage interface.
  std::string FetchRefreshToken() override;
  bool StoreRefreshToken(const std::string& refresh_token) override;
  std::string FetchUserEmail() override;
  bool StoreUserEmail(const std::string& user_email) override;
  std::string FetchAccessToken() override;
  bool StoreAccessToken(const std::string& access_token) override;
  std::string FetchDeviceId() override;
  bool StoreDeviceId(const std::string& device_id) override;

 private:
  std::string FetchTokenFromKey(const std::string& key);
  bool StoreTokenForKey(const std::string& key, const std::string& value);

  // Returns the path for the file used to read from or store a token for the
  // user.
  base::FilePath GetPathForTokens();

  // Used to access the user specific token file.
  std::string user_name_;

  // Path used to retrieve the tokens file.
  base::FilePath file_path_;

  DISALLOW_COPY_AND_ASSIGN(TestTokenStorageOnDisk);
};

TestTokenStorageOnDisk::TestTokenStorageOnDisk(const std::string& user_name,
                                               const base::FilePath& file_path)
    : user_name_(user_name), file_path_(base::MakeAbsoluteFilePath(file_path)) {
  if (user_name_.empty()) {
    user_name_ = kUnspecifiedUsername;
  }
  VLOG(0) << "User name: " << user_name_;
  VLOG(0) << "Storage file path: " << GetPathForTokens();
}

TestTokenStorageOnDisk::~TestTokenStorageOnDisk() = default;

std::string TestTokenStorageOnDisk::FetchRefreshToken() {
  return FetchTokenFromKey(kRefreshTokenKey);
}

bool TestTokenStorageOnDisk::StoreRefreshToken(
    const std::string& refresh_token) {
  return StoreTokenForKey(kRefreshTokenKey, refresh_token);
}

std::string TestTokenStorageOnDisk::FetchUserEmail() {
  return FetchTokenFromKey(kUserEmailKey);
}

bool TestTokenStorageOnDisk::StoreUserEmail(const std::string& user_email) {
  return StoreTokenForKey(kUserEmailKey, user_email);
}

std::string TestTokenStorageOnDisk::FetchAccessToken() {
  return FetchTokenFromKey(kAccessTokenKey);
}

bool TestTokenStorageOnDisk::StoreAccessToken(const std::string& access_token) {
  return StoreTokenForKey(kAccessTokenKey, access_token);
}

std::string TestTokenStorageOnDisk::FetchDeviceId() {
  return FetchTokenFromKey(kDeviceIdKey);
}

bool TestTokenStorageOnDisk::StoreDeviceId(const std::string& device_id) {
  return StoreTokenForKey(kDeviceIdKey, device_id);
}

std::string TestTokenStorageOnDisk::FetchTokenFromKey(const std::string& key) {
  base::FilePath file_path(GetPathForTokens());
  DCHECK(!file_path.empty());
  VLOG(1) << "Reading string from: " << file_path.value();

  std::string file_contents;
  if (!base::ReadFileToString(file_path, &file_contents)) {
    VLOG(1) << "Couldn't read file: " << file_path.value();
    return std::string();
  }

  base::Optional<base::Value> token_data(base::JSONReader::Read(file_contents));
  base::DictionaryValue* tokens = nullptr;
  if (!token_data.has_value() || !token_data->GetAsDictionary(&tokens)) {
    LOG(ERROR) << "File contents were not valid JSON, "
               << "could not retrieve token.";
    return std::string();
  }

  base::Value* token = tokens->FindPath({user_name_, key});
  if (!token) {
    VLOG(1) << "Could not find token for: " << key;
    return std::string();
  }

  return token->GetString();
}

bool TestTokenStorageOnDisk::StoreTokenForKey(const std::string& key,
                                              const std::string& value) {
  DCHECK(!value.empty());

  base::FilePath file_path(GetPathForTokens());
  DCHECK(!file_path.empty());
  VLOG(2) << "Storing token to: " << file_path.value();

  base::FilePath file_dir(file_path.DirName());
  if (!base::DirectoryExists(file_dir) && !base::CreateDirectory(file_dir)) {
    LOG(ERROR) << "Failed to create directory, token not stored.";
    return false;
  }

  std::string file_contents("{}");
  if (base::PathExists(file_path)) {
    if (!base::ReadFileToString(file_path, &file_contents)) {
      LOG(ERROR) << "Invalid token file: " << file_path.value();
      return false;
    }
  }

  base::Optional<base::Value> token_data(base::JSONReader::Read(file_contents));
  base::DictionaryValue* tokens = nullptr;
  if (!token_data.has_value() || !token_data->GetAsDictionary(&tokens)) {
    LOG(ERROR) << "Invalid token file format, could not store token.";
    return false;
  }

  std::string json_string;
  tokens->SetPath({user_name_, key}, base::Value(value));
  if (!base::JSONWriter::Write(*token_data, &json_string)) {
    LOG(ERROR) << "Couldn't convert JSON data to string";
    return false;
  }

  if (!base::ImportantFileWriter::WriteFileAtomically(file_path, json_string)) {
    LOG(ERROR) << "Failed to save token to the file on disk.";
    return false;
  }

  return true;
}

base::FilePath TestTokenStorageOnDisk::GetPathForTokens() {
  base::FilePath file_path(file_path_);

  // If we weren't given a specific file path, then use the default path.
  if (file_path_.empty()) {
    if (!GetTempDir(&file_path)) {
      LOG(WARNING) << "Failed to retrieve temporary directory path.";
      return base::FilePath();
    }

    file_path = file_path.Append(kRemotingFolder);
    file_path = file_path.Append(kTokenStoreFolder);
  }

  // If no file has been specified, then we will use a default file name.
  if (file_path.Extension().empty()) {
    file_path = file_path.Append(kTokenFileName);
  }

  return file_path;
}

std::unique_ptr<TestTokenStorage> TestTokenStorage::OnDisk(
    const std::string& user_name,
    const base::FilePath& refresh_token_file_path) {
  return std::make_unique<TestTokenStorageOnDisk>(user_name,
                                                  refresh_token_file_path);
}

}  // namespace test
}  // namespace remoting
