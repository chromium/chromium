// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/sandbox_dump.h"

#import <Foundation/Foundation.h>

#import "base/apple/bundle_locations.h"
#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/startup/ios_enable_sandbox_dump_buildflags.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "third_party/zlib/google/zip.h"

#if !BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)
#error "This file should only be compiled with IOS_ENABLE_SANDBOX_DUMP flag."
#endif  // BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)

void DumpSandboxIfRequested() {
  if (![[NSUserDefaults standardUserDefaults]
          boolForKey:@"EnableDumpSandboxes"]) {
    return;
  }
  [[NSUserDefaults standardUserDefaults] setBool:NO
                                          forKey:@"EnableDumpSandboxes"];
  NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                       NSUserDomainMask, YES);

  NSString* document_directory = [paths objectAtIndex:0];
  NSString* appdata_directory =
      [document_directory stringByDeletingLastPathComponent];
  NSString* dump_directory =
      [document_directory stringByAppendingPathComponent:@"sandboxdump"];
  NSError* error;
  [[NSFileManager defaultManager] removeItemAtPath:dump_directory error:&error];
  [[NSFileManager defaultManager] createDirectoryAtPath:dump_directory
                            withIntermediateDirectories:NO
                                             attributes:nil
                                                  error:&error];

  NSString* application_zip = [dump_directory
      stringByAppendingPathComponent:
          [NSString stringWithFormat:@"%@.zip", [base::apple::FrameworkBundle()
                                                    bundleIdentifier]]];

  zip::FilterCallback callback =
      base::BindRepeating(^(const base::FilePath& path) {
        NSString* nspath = base::SysUTF8ToNSString(path.value());
        if ([nspath hasPrefix:document_directory]) {
          return false;
        }
        if (![[NSFileManager defaultManager] isReadableFileAtPath:nspath]) {
          return false;
        }
        return true;
      });

  base::FilePath source_dir(base::SysNSStringToUTF8(appdata_directory));
  base::FilePath zip_path(base::SysNSStringToUTF8(application_zip));
  zip::ZipWithFilterCallback(source_dir, zip_path, callback);

  NSString* common_group = app_group::CommonApplicationGroup();
  if ([common_group length]) {
    NSURL* common_group_url = [[NSFileManager defaultManager]
        containerURLForSecurityApplicationGroupIdentifier:common_group];
    NSString* common_group_zip = [dump_directory
        stringByAppendingPathComponent:[NSString
                                           stringWithFormat:@"%@.zip",
                                                            common_group]];
    base::FilePath common_group_path(
        base::SysNSStringToUTF8([common_group_url path]));
    base::FilePath common_group_zip_path(
        base::SysNSStringToUTF8(common_group_zip));
    zip::ZipWithFilterCallback(common_group_path, common_group_zip_path,
                               callback);
  }

  NSString* app_group = app_group::ApplicationGroup();
  if ([app_group length]) {
    NSURL* app_group_url = [[NSFileManager defaultManager]
        containerURLForSecurityApplicationGroupIdentifier:app_group];
    NSString* app_group_zip = [dump_directory
        stringByAppendingPathComponent:[NSString stringWithFormat:@"%@.zip",
                                                                  app_group]];
    base::FilePath app_group_path(
        base::SysNSStringToUTF8([app_group_url path]));
    base::FilePath app_group_zip_path(base::SysNSStringToUTF8(app_group_zip));
    zip::ZipWithFilterCallback(app_group_path, app_group_zip_path, callback);
  }
}
