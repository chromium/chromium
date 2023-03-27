// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/background_tracing_test_support.h"

#include "content/browser/tracing/background_tracing_manager_impl.h"

namespace content {

void AddBackgroundTracingEnabledStateObserverForTesting(
    BackgroundTracingManager::EnabledStateTestObserver* observer) {
  BackgroundTracingManagerImpl::GetInstance().AddEnabledStateObserverForTesting(
      observer);
}

void RemoveBackgroundTracingEnabledStateObserverForTesting(
    BackgroundTracingManager::EnabledStateTestObserver* observer) {
  BackgroundTracingManagerImpl::GetInstance()
      .RemoveEnabledStateObserverForTesting(observer);
}

}  // namespace content