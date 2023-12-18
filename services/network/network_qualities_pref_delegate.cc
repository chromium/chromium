// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_qualities_pref_delegate.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/http/http_server_properties.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/nqe/pref_names.h"

namespace {

// PrefDelegateImpl writes the provided dictionary value to the network quality
// estimator prefs on the disk.
class PrefDelegateImpl
    : public net::NetworkQualitiesPrefsManager::PrefDelegate {
 public:
  // |pref_service| is used to read and write prefs from/to the disk.
  explicit PrefDelegateImpl(PrefService* pref_service)
      : pref_service_(pref_service), path_(net::nqe::kNetworkQualities) {
    DCHECK(pref_service_);
  }

  PrefDelegateImpl(const PrefDelegateImpl&) = delete;
  PrefDelegateImpl& operator=(const PrefDelegateImpl&) = delete;

  ~PrefDelegateImpl() override {}

  void SetDictionaryValue(const base::Value::Dict& dict) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    pref_service_->SetDict(path_, dict.Clone());
  }

  base::Value::Dict GetDictionaryValue() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return pref_service_->GetDict(path_).Clone();
  }

 private:
  raw_ptr<PrefService> pref_service_;

  // |path_| is the location of the network quality estimator prefs.
  const std::string path_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Returns true if |pref_service| has been initialized.
bool IsPrefServiceInitialized(PrefService* pref_service) {
  return pref_service->GetInitializationStatus() !=
         PrefService::INITIALIZATION_STATUS_WAITING;
}

}  // namespace

namespace network {

NetworkQualitiesPrefDelegate::NetworkQualitiesPrefDelegate(
    PrefService* pref_service,
    net::NetworkQualityEstimator* network_quality_estimator)
    : prefs_manager_(std::make_unique<PrefDelegateImpl>(pref_service)),
      network_quality_estimator_(network_quality_estimator) {
  DCHECK(pref_service);
  DCHECK(network_quality_estimator_);

  if (IsPrefServiceInitialized(pref_service)) {
    OnPrefServiceInitialized(true);
  } else {
    // Register for a callback that will be invoked when |pref_service| is
    // initialized.
    pref_service->AddPrefInitObserver(
        base::BindOnce(&NetworkQualitiesPrefDelegate::OnPrefServiceInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

NetworkQualitiesPrefDelegate::~NetworkQualitiesPrefDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NetworkQualitiesPrefDelegate::OnPrefServiceInitialized(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  prefs_manager_.InitializeOnNetworkThread(network_quality_estimator_);
}

void NetworkQualitiesPrefDelegate::ClearPrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  prefs_manager_.ClearPrefs();
}

// static
void NetworkQualitiesPrefDelegate::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(net::nqe::kNetworkQualities);
}

std::map<net::nqe::internal::NetworkID,
         net::nqe::internal::CachedNetworkQuality>
NetworkQualitiesPrefDelegate::ForceReadPrefsForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return prefs_manager_.ForceReadPrefsForTesting();
}

}  // namespace network
