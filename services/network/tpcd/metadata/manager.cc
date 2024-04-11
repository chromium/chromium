// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/tpcd/metadata/manager.h"

#include <utility>

#include "base/check.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "components/tpcd/metadata/common/manager_base.h"
#include "net/base/features.h"

namespace network::tpcd::metadata {

Manager::Manager() {
  if (base::FeatureList::IsEnabled(
          content_settings::features::kHostIndexedMetadataGrants)) {
    grants_ = content_settings::HostIndexedContentSettings();
  } else {
    grants_ = ContentSettingsForOneType();
  }
}

Manager::~Manager() = default;

void Manager::SetGrants(const ContentSettingsForOneType& grants) {
  if (absl::holds_alternative<content_settings::HostIndexedContentSettings>(
          grants_)) {
    auto indices = content_settings::HostIndexedContentSettings::Create(grants);
    if (indices.empty()) {
      grants_ = content_settings::HostIndexedContentSettings();
    } else {
      CHECK_EQ(indices.size(), 1u);
      grants_ = std::move(indices.front());
    }
  } else {
    grants_ = grants;
  }
}

ContentSetting Manager::GetContentSetting(
    const GURL& third_party_url,
    const GURL& first_party_url,
    content_settings::SettingInfo* out_info) const {
  return ManagerBase::GetContentSetting(grants_, third_party_url,
                                        first_party_url, out_info);
}

ContentSettingsForOneType Manager::GetGrants() const {
  if (!base::FeatureList::IsEnabled(net::features::kTpcdMetadataGrants)) {
    return ContentSettingsForOneType();
  }

  if (absl::holds_alternative<content_settings::HostIndexedContentSettings>(
          grants_)) {
    return ManagerBase::GetContentSettingForOneType(
        absl::get<content_settings::HostIndexedContentSettings>(grants_));
  }

  return absl::get<ContentSettingsForOneType>(grants_);
}

}  // namespace network::tpcd::metadata
