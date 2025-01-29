// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_URL_UTILS_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_URL_UTILS_H_

#import "url/gurl.h"

namespace lens {

/// Whether the `url` is a lens overlay SRP.
bool IsLensOverlaySRP(GURL url);

/// Returns the search tearm of the lens overlay SRP.
std::string ExtractQueryFromLensOverlaySRP(GURL url);

}  // namespace lens

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_URL_UTILS_H_
