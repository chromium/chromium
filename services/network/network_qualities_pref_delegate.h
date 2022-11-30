// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NETWORK_QUALITIES_PREF_DELEGATE_H_
#define SERVICES_NETWORK_NETWORK_QUALITIES_PREF_DELEGATE_H_

#include <map>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "net/nqe/cached_network_quality.h"
#include "net/nqe/network_id.h"
#include "net/nqe/network_qualities_prefs_manager.h"

namespace net {
class NetworkQualityEstimator;
}

class PrefRegistrySimple;
class PrefService;

namespace network {

// UI service to manage storage of network quality prefs.
class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkQualitiesPrefDelegate {
 public:
  NetworkQualitiesPrefDelegate(
      PrefService* pref_service,
      net::NetworkQualityEstimator* network_quality_estimator);

  NetworkQualitiesPrefDelegate(const NetworkQualitiesPrefDelegate&) = delete;
  NetworkQualitiesPrefDelegate& operator=(const NetworkQualitiesPrefDelegate&) =
      delete;

  ~NetworkQualitiesPrefDelegate();

  // Registers the profile-specific network quality estimator prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Clear the network quality estimator prefs.
  void ClearPrefs();

  // Reads the prefs from the disk, parses them into a map of NetworkIDs and
  // CachedNetworkQualities, and returns the map.
  std::map<net::nqe::internal::NetworkID,
           net::nqe::internal::CachedNetworkQuality>
  ForceReadPrefsForTesting() const;

 private:
  // Called when pref service is initialized.
  void OnPrefServiceInitialized(bool success);

  // Prefs manager that is owned by this service. Created on the UI thread, but
  // used and deleted on the IO thread.
  net::NetworkQualitiesPrefsManager prefs_manager_;

  // Guaranteed to be non-null during the lifetime of |this|.
  raw_ptr<net::NetworkQualityEstimator> network_quality_estimator_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<NetworkQualitiesPrefDelegate> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_NETWORK_QUALITIES_PREF_DELEGATE_H_
