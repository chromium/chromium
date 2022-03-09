// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_SOURCE_TYPE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_SOURCE_TYPE_H_

#include "content/common/content_export.h"

namespace content {

// Denotes the type of source for this impression. This allows different types
// of impressions to be processed differently by storage and attribution
// logic.
enum class AttributionSourceType {
  // An impression which was associated with a top-level navigation.
  kNavigation = 0,
  // An impression which was not associated with a navigation.
  kEvent = 1,
  kMaxValue = kEvent,
};

// Returns "navigation" or "event".
CONTENT_EXPORT const char* AttributionSourceTypeToString(AttributionSourceType);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_SOURCE_TYPE_H_
