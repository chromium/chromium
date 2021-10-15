// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_FEATURES_H_
#define IOS_CHROME_BROWSER_WEB_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace web {

// Feature flag to tie the default zoom level for webpages to the current
// dynamic type setting.
extern const base::Feature kWebPageDefaultZoomFromDynamicType;

// Used to enable a different method of zooming web pages.
extern const base::Feature kWebPageAlternativeTextZoom;

// Feature flag for to use native session restoration.
extern const base::Feature kRestoreSessionFromCache;

// When enabled, the major version number returned by Chrome will be forced to
// 100.  This feature is only applicable for M96-M99 and will be removed after
// M99.  The purpose of this feature is to allow users to test and proactively
// fix any issues as Chrome approaches a 3-digit major version number.
extern const base::Feature kForceMajorVersion100InUserAgent;

}  // namespace web

#endif  // IOS_CHROME_BROWSER_WEB_FEATURES_H_
