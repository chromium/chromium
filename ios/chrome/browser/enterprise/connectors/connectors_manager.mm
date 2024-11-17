// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/connectors_manager.h"

namespace enterprise_connectors {

ConnectorsManager::ConnectorsManager(PrefService* pref_service,
                                     const ServiceProviderConfig* config)
    : ConnectorsManagerBase(pref_service, config, /*observe_prefs=*/true) {}

ConnectorsManager::~ConnectorsManager() = default;

}  // namespace enterprise_connectors
