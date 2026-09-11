// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/background_tracing.h"

#include <memory>

#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/tracing_delegate.h"
#include "content/public/common/content_client.h"

namespace content {

std::unique_ptr<tracing::BackgroundTracingManager>
CreateBackgroundTracingManagerForTesting() {
  return std::make_unique<BackgroundTracingManagerImpl>(
      GetContentClientForTesting()->browser()->CreateTracingDelegate());
}

}  // namespace content
