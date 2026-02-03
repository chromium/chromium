// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/analysis/analysis_service_settings.h"

#import "components/enterprise/connectors/core/service_provider_config.h"

namespace enterprise_connectors {

AnalysisServiceSettings::AnalysisServiceSettings(
    const base::Value& settings_value,
    const ServiceProviderConfig& service_provider_config)
    : AnalysisServiceSettingsBase(settings_value, service_provider_config) {}

AnalysisServiceSettings::AnalysisServiceSettings(AnalysisServiceSettings&&) =
    default;
AnalysisServiceSettings& AnalysisServiceSettings::operator=(
    AnalysisServiceSettings&&) = default;
AnalysisServiceSettings::~AnalysisServiceSettings() = default;

}  // namespace enterprise_connectors
