// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/sessions_storage_util.h"

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/path_service.h"
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

void MarkSessionsForRemoval(NSArray<NSString*>* session_ids) {
  NSString* file_path = GetDiscardedSessionsFilePath();
  NSMutableArray* sessions = [NSMutableArray arrayWithContentsOfFile:file_path];
  if (!sessions) {
    sessions = [[NSMutableArray alloc] init];
  }
  [sessions addObjectsFromArray:session_ids];
  [sessions writeToFile:file_path atomically:YES];
}

NSArray<NSString*>* GetDiscardedSessions() {
  return [NSArray arrayWithContentsOfFile:GetDiscardedSessionsFilePath()];
}

void ResetDiscardedSessions() {
  NSFileManager* file_manager = [[NSFileManager alloc] init];
  [file_manager removeItemAtPath:GetDiscardedSessionsFilePath() error:nil];
}

}  // namespace sessions_storage_util
