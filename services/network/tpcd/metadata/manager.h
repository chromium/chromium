// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TPCD_METADATA_MANAGER_H_
#define SERVICES_NETWORK_TPCD_METADATA_MANAGER_H_

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "components/tpcd/metadata/common/manager_base.h"

namespace network::tpcd::metadata {

// This Manager class will be responsible to host and manager inquiries to the
// TPCD Metadata content setting. It is expected to be primarily queried by the
// CookieSetting object to affect cookie access decisions.
//
// The content settings held by the object from this class will be prepared by
// the manager object on the browser layer and copied/synced into this object.
class Manager : public ::tpcd::metadata::common::ManagerBase {
 public:
  Manager();
  ~Manager();

  Manager(const Manager&) = delete;
  Manager& operator=(const Manager&) = delete;

  void SetGrants(const ContentSettingsForOneType& grants);
  [[nodiscard]] ContentSettingsForOneType GetGrants() const;
  [[nodiscard]] ContentSetting GetContentSetting(
      const GURL& third_party_url,
      const GURL& first_party_url,
      content_settings::SettingInfo* out_info) const;

 private:
  content_settings::HostIndexedContentSettings grants_;
};

}  // namespace network::tpcd::metadata

#endif  // SERVICES_NETWORK_TPCD_METADATA_MANAGER_H_
