// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_RANKER_METRICS_PROVIDER_H_
#define IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_RANKER_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

namespace translate {

// Provides metrics related to the translate ranker.
class TranslateRankerMetricsProvider : public metrics::MetricsProvider {
 public:
  TranslateRankerMetricsProvider() : logging_enabled_(false) {}

  TranslateRankerMetricsProvider(const TranslateRankerMetricsProvider&) =
      delete;
  TranslateRankerMetricsProvider& operator=(
      const TranslateRankerMetricsProvider&) = delete;

  ~TranslateRankerMetricsProvider() override {}

  // From metrics::MetricsProvider...
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;

 private:
  // Updates the logging state of all ranker instances.
  void UpdateLoggingState();

  // The current state of logging.
  bool logging_enabled_;
};

}  // namespace translate

#endif  // IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_RANKER_METRICS_PROVIDER_H_
