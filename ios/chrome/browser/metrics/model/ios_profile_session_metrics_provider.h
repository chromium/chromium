// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_SESSION_METRICS_PROVIDER_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_SESSION_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

#include <memory>

std::unique_ptr<metrics::MetricsProvider>
CreateIOSProfileSessionMetricsProvider();

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_SESSION_METRICS_PROVIDER_H_
