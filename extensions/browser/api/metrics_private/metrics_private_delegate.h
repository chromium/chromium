// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_METRICS_PRIVATE_METRICS_PRIVATE_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_METRICS_PRIVATE_METRICS_PRIVATE_DELEGATE_H_

namespace extensions {

// Delegate class for embedder-specific functionality of chrome.metricsPrivate.
class MetricsPrivateDelegate {
 public:
  virtual ~MetricsPrivateDelegate() {}

  // Returns whether crash reporting is enabled.
  virtual bool IsCrashReportingEnabled() = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_METRICS_PRIVATE_METRICS_PRIVATE_DELEGATE_H_
