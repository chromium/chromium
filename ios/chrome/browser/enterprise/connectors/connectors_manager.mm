// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/connectors_manager.h"

#import "components/enterprise/connectors/core/common.h"
#import "ios/chrome/browser/enterprise/connectors/analysis/analysis_service_settings.h"

namespace enterprise_connectors {

ConnectorsManager::ConnectorsManager(PrefService* pref_service,
                                     const ServiceProviderConfig* config)
    : ConnectorsManagerBase(pref_service, config, /*observe_prefs=*/true) {}

ConnectorsManager::~ConnectorsManager() = default;

void ConnectorsManager::CacheAnalysisConnectorPolicy(
    AnalysisConnector connector) const {
  analysis_connector_settings_.erase(connector);

  // Connectors with non-existing policies should not reach this code.
  const char* pref = AnalysisConnectorPref(connector);
  DCHECK(pref);

  const base::ListValue& policy_value = prefs()->GetList(pref);
  for (const base::Value& service_settings : policy_value) {
    analysis_connector_settings_[connector].push_back(
        std::make_unique<AnalysisServiceSettings>(service_settings,
                                                  *service_provider_config_));
  }
}

DataRegion ConnectorsManager::GetDataRegion(AnalysisConnector connector) const {
  return DataRegion::NO_PREFERENCE;
}

}  // namespace enterprise_connectors
