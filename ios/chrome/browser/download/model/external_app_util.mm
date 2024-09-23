// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/external_app_util.h"

#import <UIKit/UIKit.h>

#import "base/files/file_path.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"

NSString* const kGoogleDriveITunesItemIdentifier = @"507874739";
NSString* const kGoogleDriveAppURLScheme = @"googledrive";
NSString* const kGoogleDriveAppBundleID = @"com.google.Drive";
NSString* const kGoogleMapsAppUrlScheme = @"comgooglemaps://";

NSURL* GetGoogleDriveAppUrl() {
  NSURLComponents* google_drive_url = [[NSURLComponents alloc] init];
  google_drive_url.scheme = kGoogleDriveAppURLScheme;
  return google_drive_url.URL;
}

NSURL* GetGoogleMapsAppUrl() {
  return [NSURL URLWithString:kGoogleMapsAppUrlScheme];
}

bool IsGoogleDriveAppInstalled() {
  return [[UIApplication sharedApplication] canOpenURL:GetGoogleDriveAppUrl()];
}

bool IsGoogleMapsAppInstalled() {
  return [[UIApplication sharedApplication] canOpenURL:GetGoogleMapsAppUrl()];
}

NSURL* GetFilesAppUrl() {
  base::FilePath download_dir;
  GetDownloadsDirectory(&download_dir);

  return [NSURL
      URLWithString:[NSString stringWithFormat:@"shareddocuments://%s",
                                               download_dir.value().c_str()]];
}
