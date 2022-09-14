// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OPEN_IN_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_OPEN_IN_FEATURES_H_

#import "base/feature_list.h"

// Feature flag that enables Open In download.
extern const base::Feature kEnableOpenInDownload;

// Indicates which Open In download variant to use.
extern const char kOpenInDownloadWithWKDownloadParam[];
extern const char kOpenInDownloadWithV2Param[];

// Convenience method for determining when Open In download with WKDownload is
// enabled.
bool IsOpenInDownloadWithWKDownload();

// Convenience method for determining when Open In download with V2 is enabled.
bool IsOpenInDownloadWithV2();

// Convenience method for determining when Open In download experiment is
// enabled.
bool IsOpenInDownloadEnabled();

#endif  // IOS_CHROME_BROWSER_UI_OPEN_IN_FEATURES_H_
