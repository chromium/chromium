// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BACKGROUND_TRACING_TEST_SUPPORT_H_
#define CONTENT_PUBLIC_TEST_BACKGROUND_TRACING_TEST_SUPPORT_H_

#include "content/public/browser/background_tracing_manager.h"

namespace content {

// Add/remove EnabledStateObserver.
void AddBackgroundTracingEnabledStateObserverForTesting(
    BackgroundTracingManager::EnabledStateTestObserver* observer);

void RemoveBackgroundTracingEnabledStateObserverForTesting(
    BackgroundTracingManager::EnabledStateTestObserver* observer);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BACKGROUND_TRACING_TEST_SUPPORT_H_