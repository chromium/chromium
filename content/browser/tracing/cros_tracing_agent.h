// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_CROS_TRACING_AGENT_H_
#define CONTENT_BROWSER_TRACING_CROS_TRACING_AGENT_H_

#include "base/feature_list.h"
#include "content/common/content_export.h"

namespace content {

void RegisterCrOSTracingDataSource();

CONTENT_EXPORT BASE_DECLARE_FEATURE(kCrOSTracingDataSource);

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_CROS_TRACING_AGENT_H_
