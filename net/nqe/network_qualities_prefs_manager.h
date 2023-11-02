// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_NETWORK_QUALITIES_PREFS_MANAGER_H_
#define NET_NQE_NETWORK_QUALITIES_PREFS_MANAGER_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "net/base/net_export.h"
#include "net/nqe/cached_network_quality.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_id.h"
#include "net/nqe/network_quality_store.h"

namespace net {
class NetworkQualityEstimator;

typedef std::map<nqe::internal::NetworkID, nqe::internal::CachedNetworkQuality>
    ParsedPrefs;

// Using the provided PrefDelegate, NetworkQualitiesPrefsManager creates and
// updates network quality information that is stored in prefs.
class NET_EXPORT NetworkQualitiesPrefsManager
    : public nqe::internal::NetworkQualityStore::NetworkQualitiesCacheObserver {
 public:
  // Provides an interface that must be implemented by the embedder.
  class NET_EXPORT PrefDelegate {
   public:
    virtual ~PrefDelegate() = default;

    // Sets the persistent pref to the given value.
    virtual void SetDictionaryValue(const base::Value::Dict& dict) = 0;

    // Returns a copy of the persistent prefs.
    virtual base::Value::Dict GetDictionaryValue() = 0;
  };

  // Creates an instance of the NetworkQualitiesPrefsManager. Ownership of
  // |pref_delegate| is taken by this class.
  explicit NetworkQualitiesPrefsManager(
      std::unique_ptr<PrefDelegate> pref_delegate);

  NetworkQualitiesPrefsManager(const NetworkQualitiesPrefsManager&) = delete;
  NetworkQualitiesPrefsManager& operator=(const NetworkQualitiesPrefsManager&) =
      delete;

  ~NetworkQualitiesPrefsManager() override;

  // Initialize on the Network thread. Must be called after pref service has
  // been initialized, and prefs are ready for reading.
  void InitializeOnNetworkThread(
      NetworkQualityEstimator* network_quality_estimator);

  // Prepare for shutdown.
  void ShutdownOnPrefSequence();

  // Clear the network quality estimator prefs.
  void ClearPrefs();

  // Reads the prefs again, parses them into a map of NetworkIDs and
  // CachedNetworkQualities, and returns the map.
  ParsedPrefs ForceReadPrefsForTesting() const;

 private:
  // Responsible for writing the persistent prefs to the disk.
  std::unique_ptr<PrefDelegate> pref_delegate_;

  // Current prefs on the disk.
  base::Value::Dict prefs_;

  // nqe::internal::NetworkQualityStore::NetworkQualitiesCacheObserver
  // implementation:
  void OnChangeInCachedNetworkQuality(
      const nqe::internal::NetworkID& network_id,
      const nqe::internal::CachedNetworkQuality& cached_network_quality)
      override;

  raw_ptr<NetworkQualityEstimator> network_quality_estimator_ = nullptr;

  // Network quality prefs read from the disk at the time of startup.
  ParsedPrefs read_prefs_startup_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace net

#endif  // NET_NQE_NETWORK_QUALITIES_PREFS_MANAGER_H_
