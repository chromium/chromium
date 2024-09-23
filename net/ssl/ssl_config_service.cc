// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_config_service.h"

#include <tuple>

#include "base/feature_list.h"
#include "base/observer_list.h"
#include "net/base/features.h"
#include "net/ssl/ssl_config_service_defaults.h"

namespace net {

SSLContextConfig::SSLContextConfig() = default;
SSLContextConfig::SSLContextConfig(const SSLContextConfig&) = default;
SSLContextConfig::SSLContextConfig(SSLContextConfig&&) = default;
SSLContextConfig::~SSLContextConfig() = default;
SSLContextConfig& SSLContextConfig::operator=(const SSLContextConfig&) =
    default;
SSLContextConfig& SSLContextConfig::operator=(SSLContextConfig&&) = default;
bool SSLContextConfig::operator==(const SSLContextConfig&) const = default;

bool SSLContextConfig::PostQuantumKeyAgreementEnabled() const {
  return post_quantum_override.value_or(
      base::FeatureList::IsEnabled(features::kPostQuantumKyber));
}

SSLConfigService::SSLConfigService()
    : observer_list_(base::ObserverListPolicy::EXISTING_ONLY) {}

SSLConfigService::~SSLConfigService() = default;

void SSLConfigService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SSLConfigService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void SSLConfigService::NotifySSLContextConfigChange() {
  for (auto& observer : observer_list_)
    observer.OnSSLContextConfigChanged();
}

void SSLConfigService::ProcessConfigUpdate(const SSLContextConfig& old_config,
                                           const SSLContextConfig& new_config,
                                           bool force_notification) {
  // Do nothing if the configuration hasn't changed.
  if (old_config != new_config || force_notification) {
    NotifySSLContextConfigChange();
  }
}

}  // namespace net
