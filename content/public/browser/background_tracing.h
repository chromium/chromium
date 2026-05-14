// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_H_
#define CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_H_

#include <memory>

#include "content/common/content_export.h"

namespace tracing {
class BackgroundTracingManager;
}

namespace content {

class TracingDelegate;

CONTENT_EXPORT std::unique_ptr<tracing::BackgroundTracingManager>
CreateBackgroundTracingManager(TracingDelegate* delegate);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_H_
