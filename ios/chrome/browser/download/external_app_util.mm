// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/external_app_util.h"

#import <UIKit/UIKit.h>

#include "base/files/file_path.h"
#include "ios/chrome/browser/download/download_directory_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kGoogleDriveITunesItemIdentifier = @"507874739";
NSString* const kGoogleDriveAppURLScheme = @"googledrive";
NSString* const kGoogleDriveAppBundleID = @"com.google.Drive";

NSURL* GetGoogleDriveAppUrl() {
  NSURLComponents* google_drive_url = [[NSURLComponents alloc] init];
  google_drive_url.scheme = kGoogleDriveAppURLScheme;
  return google_drive_url.URL;
}

bool IsGoogleDriveAppInstalled() {
  return [[UIApplication sharedApplication] canOpenURL:GetGoogleDriveAppUrl()];
}

NSURL* GetFilesAppUrl() {
  base::FilePath download_dir;
  GetDownloadsDirectory(&download_dir);

  return [NSURL
      URLWithString:[NSString stringWithFormat:@"shareddocuments://%s",
                                               download_dir.value().c_str()]];
}
