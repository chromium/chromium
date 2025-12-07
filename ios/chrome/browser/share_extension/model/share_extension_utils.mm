// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_extension/model/share_extension_utils.h"

#import "base/apple/foundation_util.h"
#import "base/threading/scoped_blocking_call.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/share_extension/model/parsed_share_extension_entry.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

bool CreateShareExtensionFilesDirectory(NSURL* folder_url) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  NSFileManager* manager = [NSFileManager defaultManager];
  NSError* error = nil;
  if ([manager fileExistsAtPath:[folder_url path]]) {
    return true;
  }

  bool shareExtensionFilesDirectoryCreated =
      [manager createDirectoryAtPath:[folder_url path]
          withIntermediateDirectories:NO
                           attributes:nil
                                error:&error];
  if (error) {
    return false;
  }

  return shareExtensionFilesDirectoryCreated;
}

NSArray<NSURL*>* EnumerateFilesInDirectory(NSURL* directory_url) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  NSFileManager* manager = [NSFileManager defaultManager];
  NSError* error = nil;
  NSArray<NSURL*>* files =
      [manager contentsOfDirectoryAtURL:directory_url
             includingPropertiesForKeys:nil
                                options:NSDirectoryEnumerationSkipsHiddenFiles
                                  error:&error];
  if (error) {
    return nil;
  }

  return files;
}

ParsedShareExtensionEntry* PerformBlockingFileReadAndParse(NSURL* file_url) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  ParsedShareExtensionEntry* result = [[ParsedShareExtensionEntry alloc] init];

  if (![[NSFileManager defaultManager] fileExistsAtPath:[file_url path]]) {
    return result;
  }

  NSError* fileReadError = nil;
  NSData* fileData =
      [[NSFileManager defaultManager] contentsAtPath:[file_url path]];

  if (fileReadError || !fileData) {
    return result;
  }

  NSError* unarchiverError = nil;
  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:fileData
                                                  error:&unarchiverError];
  if (!unarchiver || unarchiverError) {
    return result;
  }

  unarchiver.requiresSecureCoding = NO;
  id entryID = [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
  NSDictionary* entry = base::apple::ObjCCast<NSDictionary>(entryID);
  if (!entry) {
    return result;
  }

  NSNumber* cancelled = base::apple::ObjCCast<NSNumber>(
      [entry objectForKey:app_group::kShareItemCancel]);
  if (cancelled && [cancelled boolValue]) {
    result.cancelled = true;
    return result;
  }

  NSURL* entryURL = [entry objectForKey:app_group::kShareItemURL];
  if (entryURL) {
    result.url = entryURL;
  }

  NSString* entryTitle = [entry objectForKey:app_group::kShareItemTitle];
  if (entryTitle) {
    result.title = entryTitle;
  }

  NSDate* entryDate = base::apple::ObjCCast<NSDate>(
      [entry objectForKey:app_group::kShareItemDate]);
  if (entryDate) {
    result.date = entryDate;
  }

  NSNumber* entryType = base::apple::ObjCCast<NSNumber>(
      [entry objectForKey:app_group::kShareItemType]);
  if (entryType) {
    result.type = (app_group::ShareExtensionItemType)[entryType integerValue];
  }

  NSString* entrySource = base::apple::ObjCCast<NSString>(
      [entry objectForKey:app_group::kShareItemSource]);
  if (entrySource) {
    result.source = entrySource;
  }

  NSString* gaiaID = base::apple::ObjCCast<NSString>(
      [entry objectForKey:app_group::kShareItemGaiaID]);

  if (gaiaID) {
    result.gaiaID = GaiaId(gaiaID);
  }

  return result;
}
