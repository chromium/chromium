// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/connectors_manager.h"

#import "components/enterprise/connectors/core/common.h"

namespace enterprise_connectors {

ConnectorsManager::ConnectorsManager(PrefService* pref_service,
                                     const ServiceProviderConfig* config)
    : ConnectorsManagerBase(pref_service, config, /*observe_prefs=*/true) {}

ConnectorsManager::~ConnectorsManager() = default;

void ConnectorsManager::CacheAnalysisConnectorPolicy(
    AnalysisConnector connector) const {
  // do nothing
}

DataRegion ConnectorsManager::GetDataRegion(AnalysisConnector connector) const {
  return DataRegion::NO_PREFERENCE;
}

}  // namespace enterprise_connectors
