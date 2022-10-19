// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OPEN_IN_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_OPEN_IN_FEATURES_H_

#import "base/feature_list.h"

// Feature flag that enables Open In download.
BASE_DECLARE_FEATURE(kEnableOpenInDownload);

// Feature parameters for Open In download. If no parameter is set, the  default
// download and toolbar will be used.
extern const char kOpenInDownloadParameterName[];

// Indicates which Open In download variant to use.
extern const char kOpenInDownloadInShareButtonParam[];
extern const char kOpenInDownloadWithWKDownloadParam[];
extern const char kOpenInDownloadWithV2Param[];

// Convenience method for determining when Open In with legacy download in share
// button is enabled.
bool IsOpenInDownloadInShareButton();

// Convenience method for determining when Open In download with WKDownload in
// share button is enabled.
bool IsOpenInDownloadWithWKDownload();

// Convenience method for determining when Open In download with V2 in share
// button is enabled.
bool IsOpenInDownloadWithV2();

// Convenience method for determining when new download experiment is
// enabled.
bool IsOpenInNewDownloadEnabled();

// Convenience method for determining if Open In activities are moved in the
// share button.
bool IsOpenInActivitiesInShareButtonEnabled();

#endif  // IOS_CHROME_BROWSER_UI_OPEN_IN_FEATURES_H_
