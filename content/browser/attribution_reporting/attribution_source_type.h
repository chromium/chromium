// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_SOURCE_TYPE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_SOURCE_TYPE_H_

#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/common/content_export.h"

namespace content {

using AttributionSourceType = ::attribution_reporting::mojom::SourceType;

// Returns "navigation" or "event".
CONTENT_EXPORT
const char* AttributionSourceTypeToString(AttributionSourceType);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_SOURCE_TYPE_H_
