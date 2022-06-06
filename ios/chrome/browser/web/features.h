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

// Reneables text zoom on iPad.
extern const base::Feature kWebPageTextZoomIPad;

// Feature flag for to use native session restoration.
extern const base::Feature kRestoreSessionFromCache;

}  // namespace web

#endif  // IOS_CHROME_BROWSER_WEB_FEATURES_H_
