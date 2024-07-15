// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/sessions_storage_util.h"

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"

namespace {

// Returns the path of the list of sessions that are marked for removal.
NSString* GetDiscardedSessionsFilePath() {
  base::FilePath directory_path;
  base::PathService::Get(ios::DIR_USER_DATA, &directory_path);
  return base::apple::FilePathToNSString(
      directory_path.Append(FILE_PATH_LITERAL("DiscardedSessions")));
}

}  // namespace

namespace sessions_storage_util {

void MarkSessionsForRemoval(std::set<std::string> session_ids) {
  std::set<std::string> discarded_session_ids = GetDiscardedSessions();
  discarded_session_ids.merge(std::move(session_ids));
  if (discarded_session_ids.empty()) {
    ResetDiscardedSessions();
  } else {
    NSMutableArray<NSString*>* sessions = [[NSMutableArray alloc] init];
    for (const std::string& session_id : discarded_session_ids) {
      [sessions addObject:base::SysUTF8ToNSString(session_id)];
    }
    [sessions writeToFile:GetDiscardedSessionsFilePath() atomically:YES];
  }
}

std::set<std::string> GetDiscardedSessions() {
  std::set<std::string> discarded_session_ids;
  for (NSString* session_id in
       [NSArray arrayWithContentsOfFile:GetDiscardedSessionsFilePath()]) {
    discarded_session_ids.insert(base::SysNSStringToUTF8(session_id));
  }
  return discarded_session_ids;
}

void ResetDiscardedSessions() {
  NSFileManager* file_manager = [[NSFileManager alloc] init];
  [file_manager removeItemAtPath:GetDiscardedSessionsFilePath() error:nil];
}

}  // namespace sessions_storage_util
