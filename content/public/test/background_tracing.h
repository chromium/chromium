// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BACKGROUND_TRACING_H_
#define CONTENT_PUBLIC_TEST_BACKGROUND_TRACING_H_

#include <memory>

namespace tracing {
class BackgroundTracingManager;
}

namespace content {

std::unique_ptr<tracing::BackgroundTracingManager>
CreateBackgroundTracingManagerForTesting();

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BACKGROUND_TRACING_H_
