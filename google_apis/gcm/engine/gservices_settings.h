// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_GSERVICES_SETTINGS_H_
#define GOOGLE_APIS_GCM_ENGINE_GSERVICES_SETTINGS_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "google_apis/gcm/engine/gcm_store.h"
#include "google_apis/gcm/protocol/checkin.pb.h"
#include "url/gurl.h"

namespace gcm {

// Class responsible for handling G-services settings. It takes care of
// extracting them from checkin response and storing in GCMStore.
class GCM_EXPORT GServicesSettings {
 public:
  typedef std::map<std::string, std::string> SettingsMap;

  // Minimum periodic checkin interval in seconds.
  static const base::TimeDelta MinimumCheckinInterval();

  // Calculates digest of provided settings.
  static std::string CalculateDigest(const SettingsMap& settings);

  GServicesSettings();
  ~GServicesSettings();

  // Updates the settings based on |checkin_response|.
  bool UpdateFromCheckinResponse(
      const checkin_proto::AndroidCheckinResponse& checkin_response);

  // Updates the settings based on |load_result|. Returns true if update was
  // successful, false otherwise.
  void UpdateFromLoadResult(const GCMStore::LoadResult& load_result);

  SettingsMap settings_map() const { return settings_; }

  std::string digest() const { return digest_; }

  // Gets the interval at which device should perform a checkin.
  base::TimeDelta GetCheckinInterval() const;

  // Gets the URL to use when checking in.
  GURL GetCheckinURL() const;

  // Gets address of main MCS endpoint.
  GURL GetMCSMainEndpoint() const;

  // Gets address of fallback MCS endpoint. Will be empty if there isn't one.
  GURL GetMCSFallbackEndpoint() const;

  // Gets the URL to use when registering or unregistering the apps.
  GURL GetRegistrationURL() const;

 private:
  // Digest (hash) of the settings, used to check whether settings need update.
  // It is meant to be sent with checkin request, instead of sending the whole
  // settings table.
  std::string digest_;

  // G-services settings as provided by checkin response.
  SettingsMap settings_;

  // Factory for creating references in callbacks.
  base::WeakPtrFactory<GServicesSettings> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GServicesSettings);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_GSERVICES_SETTINGS_H_
