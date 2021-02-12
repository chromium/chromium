// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_LEGACY_TLS_CONFIG_DISTRIBUTOR_H_
#define SERVICES_NETWORK_LEGACY_TLS_CONFIG_DISTRIBUTOR_H_

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "services/network/public/proto/tls_deprecation_config.pb.h"

namespace network {

// A LegacyTLSExperimentConfig is a wrapper for a
// chrome_browser_ssl::LegacyTLSExperimentConfig proto, which allows lookups of
// whether legacy TLS warnings should be suppressed for a URL.
class COMPONENT_EXPORT(NETWORK_SERVICE) LegacyTLSExperimentConfig
    : public base::RefCountedThreadSafe<LegacyTLSExperimentConfig> {
 public:
  LegacyTLSExperimentConfig();
  LegacyTLSExperimentConfig(const LegacyTLSExperimentConfig&) = delete;
  LegacyTLSExperimentConfig& operator=(const LegacyTLSExperimentConfig&) =
      delete;

  // Parses a binary proto in |data| into a LegacyTLSExperiment config. Returns
  // nullptr if parsing fails.
  static scoped_refptr<LegacyTLSExperimentConfig> Parse(
      const std::string& data);

  // Looks up whether |hostname| is in the experiment config.
  bool ShouldSuppressLegacyTLSWarning(const std::string& hostname) const;

 private:
  ~LegacyTLSExperimentConfig();

  friend class base::RefCountedThreadSafe<LegacyTLSExperimentConfig>;

  chrome_browser_ssl::LegacyTLSExperimentConfig proto_;
};

// LegacyTLSConfigDistributor is a helper class to handle fan-out distribution
// of new legacy TLS configs. As new encoded configs are received (via
// OnNewLegacyTLSConfig), they will be parsed and, if successful, dispatched to
// LegacyTLSConfigDistributor::Observers' OnNewLegacyTLSConfig().
class COMPONENT_EXPORT(NETWORK_SERVICE) LegacyTLSConfigDistributor {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    // Called whenever a new Legacy TLS config has been received.
    virtual void OnNewLegacyTLSConfig(
        scoped_refptr<LegacyTLSExperimentConfig> config) = 0;

   protected:
    Observer() = default;
    ~Observer() override = default;
  };

  LegacyTLSConfigDistributor();
  ~LegacyTLSConfigDistributor();
  LegacyTLSConfigDistributor(const LegacyTLSConfigDistributor&) = delete;
  LegacyTLSConfigDistributor& operator=(const LegacyTLSConfigDistributor&) =
      delete;

  // Adds an observer to be notified when new LegacyTLSConfigs are available.
  // Note: Newly-added observers are not notified on the current |config()|,
  // only newly configured LegacyTLSConfigs after the AddObserver call.
  void AddObserver(Observer* observer);
  // Removes a previously registered observer.
  void RemoveObserver(Observer* observer);

  // Returns the currently configured LegacyTLSConfig, or nullptr if one has not
  // yet been configured.
  scoped_refptr<LegacyTLSExperimentConfig> config() const { return config_; }

  // Notifies the distributor that a new encoded LegacyTLSConfig, |config|, has
  // been received. If the LegacyTLSConfig successfully decodes and is newer
  // than the current LegacyTLSConfig, all observers will be notified.
  // |callback| will be notified once all observers have been notified.
  // |callback| is guaranteed to run (e.g., even if this object is deleted prior
  // to it being run).
  void OnNewLegacyTLSConfig(base::span<const uint8_t> config,
                            base::OnceClosure callback);

 private:
  void OnLegacyTLSConfigParsed(scoped_refptr<LegacyTLSExperimentConfig> config);

  base::ObserverList<Observer,
                     /*check_empty=*/true,
                     /*allow_reentrancy=*/false>
      observers_;
  scoped_refptr<LegacyTLSExperimentConfig> config_ = nullptr;

  base::WeakPtrFactory<LegacyTLSConfigDistributor> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_LEGACY_TLS_CONFIG_DISTRIBUTOR_H_