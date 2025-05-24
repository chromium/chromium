// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GOOGLE_ONE_SHARED_GOOGLE_ONE_ENTRY_POINT_H_
#define IOS_CHROME_BROWSER_GOOGLE_ONE_SHARED_GOOGLE_ONE_ENTRY_POINT_H_

// The entry point from which Google one management was triggered.
enum class GoogleOneEntryPoint {
  // Triggered from account screen in settings.
  kSettings,
  // Triggered from no storage alert in Save to Drive.
  kSaveToDriveAlert,
  // Triggered from no storage alert in Save to Photos.
  kSaveToPhotosAlert,
};

#endif  // IOS_CHROME_BROWSER_GOOGLE_ONE_SHARED_GOOGLE_ONE_ENTRY_POINT_H_
