// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/auto_deletion/auto_deletion_service.h"

#import "base/base64.h"
#import "base/files/file_path.h"
#import "base/hash/md5.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/download/model/auto_deletion/scheduled_file.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/web/public/download/download_task.h"

namespace {
// The base64 encoding option given for converting the DownloadTask's contents
// to a string in order to compute a hash.
NSDataBase64EncodingOptions kEncodingOption =
    NSDataBase64EncodingEndLineWithCarriageReturn;

// Creates an MD5Hash of the downloaded file's contents. This hash is used to
// verify that the file that is scheduled to be deleted is the same file that
// was originally scheduled for deletion.
std::string HashDownloadData(NSData* data) {
  base::MD5Digest hash;
  NSString* base_64_string =
      [data base64EncodedStringWithOptions:kEncodingOption];
  std::string utf_8_base_64_string = base::SysNSStringToUTF8(base_64_string);
  base::MD5Sum(base::as_byte_span(utf_8_base_64_string), &hash);
  return base::MD5DigestToBase16(hash);
}

}  // namespace

namespace auto_deletion {

AutoDeletionService::AutoDeletionService()
    : scheduler_(GetApplicationContext()->GetLocalState()) {}

AutoDeletionService::~AutoDeletionService() = default;

// static
void AutoDeletionService::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDownloadAutoDeletionEnabled, false);
  registry->RegisterBooleanPref(prefs::kDownloadAutoDeletionIPHShown, false);
  registry->RegisterListPref(prefs::kDownloadAutoDeletionScheduledFiles);
}

void AutoDeletionService::ScheduleFileForDeletion(web::DownloadTask* task) {
  task->GetResponseData(
      base::BindOnce(&AutoDeletionService::ScheduleFileForDeletionHelper,
                     weak_ptr_factory_.GetWeakPtr(), std::move(task)));
}

void AutoDeletionService::ScheduleFileForDeletionHelper(web::DownloadTask* task,
                                                        NSData* data) {
  ScheduledFile file(task->GetResponsePath(), HashDownloadData(data),
                     base::Time::Now());
  scheduler_.ScheduleFile(std::move(file));
}

}  // namespace auto_deletion
