// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_ANALYSIS_SERVICE_SETTINGS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_ANALYSIS_SERVICE_SETTINGS_H_

#import "components/enterprise/connectors/core/analysis_service_settings_base.h"
#import "components/enterprise/connectors/core/analysis_settings.h"

namespace enterprise_connectors {

// The settings for an analysis service obtained from a connector policy.
// Currently just a wrapper class for AnalysisServiceSettingsBase because it is
// not supposed to be used directly. Will implement this later to specify iOS's
// version of the settings.
//
// TODO(crbug.com/479863110): implement AnalysisServiceSettings.
class AnalysisServiceSettings : public AnalysisServiceSettingsBase {
 public:
  explicit AnalysisServiceSettings(
      const base::Value& settings_value,
      const ServiceProviderConfig& service_provider_config);
  AnalysisServiceSettings(const AnalysisServiceSettings&) = delete;
  AnalysisServiceSettings(AnalysisServiceSettings&&);
  AnalysisServiceSettings& operator=(const AnalysisServiceSettings&) = delete;
  AnalysisServiceSettings& operator=(AnalysisServiceSettings&&);
  ~AnalysisServiceSettings() override;
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_ANALYSIS_SERVICE_SETTINGS_H_
