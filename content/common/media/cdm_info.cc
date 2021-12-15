// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/cdm_info.h"

#include "base/check.h"
#include "media/cdm/cdm_type.h"

namespace content {

CdmInfo::CdmInfo(const std::string& key_system,
                 Robustness robustness,
                 absl::optional<media::CdmCapability> capability,
                 bool supports_sub_key_systems,
                 const std::string& name,
                 const media::CdmType& type,
                 const base::Version& version,
                 const base::FilePath& path)
    : key_system(key_system),
      robustness(robustness),
      capability(std::move(capability)),
      supports_sub_key_systems(supports_sub_key_systems),
      name(name),
      type(type),
      version(version),
      path(path) {
  DCHECK(!this->capability || !this->capability->encryption_schemes.empty());
}

CdmInfo::CdmInfo(const std::string& key_system,
                 Robustness robustness,
                 absl::optional<media::CdmCapability> capability,
                 const media::CdmType& type)
    : key_system(key_system),
      robustness(robustness),
      capability(std::move(capability)),
      type(type) {
  DCHECK(!this->capability || !this->capability->encryption_schemes.empty());
}

CdmInfo::CdmInfo(const CdmInfo& other) = default;

CdmInfo::~CdmInfo() = default;

}  // namespace content
