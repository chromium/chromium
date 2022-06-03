// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_EXTERNAL_APP_UTIL_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_EXTERNAL_APP_UTIL_H_

@class NSString;
@class NSURL;

// iTunes Store item identifier for Google Drive app. Can be used with
// StoreKitCoordinator.
extern NSString* const kGoogleDriveITunesItemIdentifier;

// Custom URL scheme for Google Drive app. Can be used to check if Google Drive
// app is installed.
extern NSString* const kGoogleDriveAppURLScheme;

// Bundle ID of Google Drive application.
extern NSString* const kGoogleDriveAppBundleID;

// Returns URL which can be used to check if Google Drive app is installed via
// -[UIApplication canOpenURL:] call.
NSURL* GetGoogleDriveAppUrl();

// Returns true if Google Drive app is installed.
bool IsGoogleDriveAppInstalled();

// Returns URL which can be used to open Chrome's directory in files.app.
// Returns nil if it cannot get the directory.
NSURL* GetFilesAppUrl();

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_EXTERNAL_APP_UTIL_H_
