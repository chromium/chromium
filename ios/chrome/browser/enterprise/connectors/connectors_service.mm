// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/connectors_service.h"

namespace enterprise_connectors {

ConnectorsService::ConnectorsService(PrefService* pref_service)
    : prefs_(pref_service) {
  DCHECK(prefs_);
}

bool ConnectorsService::IsConnectorEnabled(AnalysisConnector connector) const {
  // None of the analysis connector policies are supported on iOS.
  return false;
}

std::optional<ConnectorsServiceBase::DmToken> ConnectorsService::GetDmToken(
    const char* scope_pref) const {
  // TODO(crbug.com/370466578): Implement this method.
  return std::nullopt;
}

bool ConnectorsService::ConnectorsEnabled() const {
  // TODO(crbug.com/370466578): Implement this method.
  return false;
}

PrefService* ConnectorsService::GetPrefs() {
  return prefs_;
}

const PrefService* ConnectorsService::GetPrefs() const {
  return prefs_;
}

ConnectorsManagerBase* ConnectorsService::GetConnectorsManagerBase() {
  // TODO(crbug.com/370466578): Implement this method.
  return nullptr;
}

const ConnectorsManagerBase* ConnectorsService::GetConnectorsManagerBase()
    const {
  // TODO(crbug.com/370466578): Implement this method.
  return nullptr;
}

}  // namespace enterprise_connectors
