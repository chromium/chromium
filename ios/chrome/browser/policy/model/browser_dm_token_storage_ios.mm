// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/browser_dm_token_storage_ios.h"

#import <Foundation/Foundation.h>

#import "base/apple/backup_util.h"
#import "base/apple/foundation_util.h"
#import "base/base64url.h"
#import "base/files/file_util.h"
#import "base/files/important_file_writer.h"
#import "base/hash/sha1.h"
#import "base/ios/device_util.h"
#import "base/path_service.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/thread_pool.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/core/common/policy_logger.h"
#import "components/policy/policy_constants.h"

namespace policy {

namespace {

const char kDmTokenBaseDir[] =
    FILE_PATH_LITERAL("Google/Chrome Cloud Enrollment/");

bool GetDmTokenFilePath(base::FilePath* token_file_path,
                        const std::string& client_id,
                        bool create_dir) {
  if (!base::PathService::Get(base::DIR_APP_DATA, token_file_path))
    return false;

  *token_file_path = token_file_path->Append(kDmTokenBaseDir);

  if (create_dir && !base::CreateDirectory(*token_file_path))
    return false;

  std::string filename;
  base::Base64UrlEncode(base::SHA1HashString(client_id),
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &filename);
  *token_file_path = token_file_path->Append(filename.c_str());

  return true;
}

bool StoreDMTokenInDirAppDataDir(const std::string& token,
                                 const std::string& client_id) {
  base::FilePath token_file_path;
  if (!GetDmTokenFilePath(&token_file_path, client_id, /*create_dir=*/true)) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  if (!base::ImportantFileWriter::WriteFileAtomically(token_file_path, token)) {
    LOG_POLICY(ERROR, CBCM_ENROLLMENT) << "Failed to save DMToken to file";
    return false;
  }

  base::apple::SetBackupExclusion(token_file_path);
  return true;
}

bool DeleteDMTokenFromAppDataDir(const std::string& client_id) {
  base::FilePath token_file_path;
  if (!GetDmTokenFilePath(&token_file_path, client_id, /*create_dir=*/false)) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  return base::DeleteFile(token_file_path);
}

}  // namespace

BrowserDMTokenStorageIOS::BrowserDMTokenStorageIOS()
    : task_runner_(base::ThreadPool::CreateTaskRunner({base::MayBlock()})) {}

BrowserDMTokenStorageIOS::~BrowserDMTokenStorageIOS() {}

std::string BrowserDMTokenStorageIOS::InitClientId() {
  return ios::device_util::GetVendorId();
}

std::string BrowserDMTokenStorageIOS::InitEnrollmentToken() {
  NSDictionary* raw_policies = [[NSUserDefaults standardUserDefaults]
      dictionaryForKey:kPolicyLoaderIOSConfigurationKey];
  NSString* token =
      base::apple::ObjCCast<NSString>(raw_policies[base::SysUTF8ToNSString(
          key::kCloudManagementEnrollmentToken)]);

  if (token) {
    return std::string(base::TrimWhitespaceASCII(base::SysNSStringToUTF8(token),
                                                 base::TRIM_ALL));
  }

  return std::string();
}

std::string BrowserDMTokenStorageIOS::InitDMToken() {
  base::FilePath token_file_path;
  if (!GetDmTokenFilePath(&token_file_path, InitClientId(),
                          /*create_dir=*/false)) {
    LOG_POLICY(WARNING, CBCM_ENROLLMENT) << "Failed to get DMToken file path";
    return std::string();
  }

  std::string token;
  if (!base::ReadFileToString(token_file_path, &token)) {
    LOG_POLICY(WARNING, CBCM_ENROLLMENT) << "Failed to read DMToken from file";
    return std::string();
  }

  return std::string(base::TrimWhitespaceASCII(token, base::TRIM_ALL));
}

bool BrowserDMTokenStorageIOS::InitEnrollmentErrorOption() {
  // No error should be shown if enrollment fails on iOS.
  LOG_POLICY(ERROR, CBCM_ENROLLMENT) << "Error initializing enrollment token";
  return false;
}

bool BrowserDMTokenStorageIOS::CanInitEnrollmentToken() const {
  return true;
}

BrowserDMTokenStorage::StoreTask BrowserDMTokenStorageIOS::SaveDMTokenTask(
    const std::string& token,
    const std::string& client_id) {
  return base::BindOnce(&StoreDMTokenInDirAppDataDir, token, client_id);
}

BrowserDMTokenStorage::StoreTask BrowserDMTokenStorageIOS::DeleteDMTokenTask(
    const std::string& client_id) {
  return base::BindOnce(&DeleteDMTokenFromAppDataDir, client_id);
}

scoped_refptr<base::TaskRunner>
BrowserDMTokenStorageIOS::SaveDMTokenTaskRunner() {
  return task_runner_;
}

}  // namespace policy
