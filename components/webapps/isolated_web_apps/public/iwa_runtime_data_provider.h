// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_IWA_RUNTIME_DATA_PROVIDER_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_IWA_RUNTIME_DATA_PROVIDER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/observer_list_types.h"
#include "base/one_shot_event.h"
#include "base/values.h"

namespace web_app {

// This class is an abstract interface for providers of IWA runtime data.
// The `components/` layer uses this interface to get data without needing
// to know where that data comes from (e.g., Component Updater). The concrete
// implementation is provided by the embedder (e.g., Chrome).
class IwaRuntimeDataProvider {
 public:
  // The KeyRotationInfo struct provides information about expected public keys
  // for Isolated Web Apps, which is fundamental to IWA security.
  struct KeyRotationInfo {
    using PublicKeyData = std::vector<uint8_t>;

    explicit KeyRotationInfo(std::optional<PublicKeyData> public_key);
    ~KeyRotationInfo();
    KeyRotationInfo(const KeyRotationInfo&);

    base::Value AsDebugValue() const;

    std::optional<PublicKeyData> public_key;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called when the underlying runtime data (including key data) may have
    // changed. Consumers should re-validate any data that depends on this
    // information.
    virtual void OnRuntimeDataChanged() = 0;
  };

  virtual ~IwaRuntimeDataProvider() = default;

  virtual const KeyRotationInfo* GetKeyRotationInfo(
      const std::string& web_bundle_id) const = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Allows a consumer to wait until the provider has the most up-to-date
  // data that it can have within a reasonable time budget. The concrete
  // implementation is left to the embedder.
  virtual base::OneShotEvent& OnBestEffortRuntimeDataReady() = 0;
};

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_IWA_RUNTIME_DATA_PROVIDER_H_
