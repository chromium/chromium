// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/cdm_info.h"

#include "base/logging.h"

namespace content {

CdmCapability::CdmCapability() = default;

CdmCapability::CdmCapability(
    std::vector<media::VideoCodec> video_codecs,
    base::flat_set<media::EncryptionScheme> encryption_schemes,
    base::flat_set<media::CdmSessionType> session_types,
    base::flat_set<media::CdmProxy::Protocol> cdm_proxy_protocols)
    : video_codecs(std::move(video_codecs)),
      encryption_schemes(std::move(encryption_schemes)),
      session_types(std::move(session_types)),
      cdm_proxy_protocols(std::move(cdm_proxy_protocols)) {}

CdmCapability::CdmCapability(const CdmCapability& other) = default;

CdmCapability::~CdmCapability() = default;

CdmInfo::CdmInfo(const std::string& name,
                 const base::Token& guid,
                 const base::Version& version,
                 const base::FilePath& path,
                 const std::string& file_system_id,
                 CdmCapability capability,
                 const std::string& supported_key_system,
                 bool supports_sub_key_systems)
    : name(name),
      guid(guid),
      version(version),
      path(path),
      file_system_id(file_system_id),
      capability(std::move(capability)),
      supported_key_system(supported_key_system),
      supports_sub_key_systems(supports_sub_key_systems) {
  DCHECK(!capability.encryption_schemes.empty());
}

CdmInfo::CdmInfo(const CdmInfo& other) = default;

CdmInfo::~CdmInfo() = default;

}  // namespace content
