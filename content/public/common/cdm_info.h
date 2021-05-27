// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CDM_INFO_H_
#define CONTENT_PUBLIC_COMMON_CDM_INFO_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/token.h"
#include "base/version.h"
#include "content/common/content_export.h"
#include "media/base/content_decryption_module.h"
#include "media/base/encryption_scheme.h"
#include "media/base/video_codecs.h"
#include "media/cdm/cdm_capability.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// Represents a Content Decryption Module implementation and its capabilities.
struct CONTENT_EXPORT CdmInfo {
  enum class Robustness {
    kHardwareSecure,
    kSoftwareSecure,
  };

  CdmInfo(const std::string& key_system,
          Robustness robustness,
          absl::optional<media::CdmCapability> capability,
          bool supports_sub_key_systems,
          const std::string& name,
          const base::Token& guid,
          const base::Version& version,
          const base::FilePath& path,
          const std::string& file_system_id);
  CdmInfo(const std::string& key_system,
          Robustness robustness,
          absl::optional<media::CdmCapability> capability);
  CdmInfo(const CdmInfo& other);
  ~CdmInfo();

  // The key system supported by this CDM.
  std::string key_system;

  // Whether this CdmInfo is for the hardware secure pipeline. Even for the
  // same `key_system`, the software and hardware secure pipeline (specified as
  // `robustness` in EME) could be supported by different CDMs, or having
  // different CDM capabilities. Therefore, we use this flag to differentiate
  // between the software and hardware secure pipelines.
  Robustness robustness;

  // CDM capability, e.g. video codecs, encryption schemes and session types.
  // Optional to allow lazy initialization, i.e. to populate the capability
  // after registration.
  absl::optional<media::CdmCapability> capability;

  // Whether we also support sub key systems of the `key_system`.
  // A sub key system to a key system is like a sub domain to a domain.
  // For example, com.example.somekeysystem.a and com.example.somekeysystem.b
  // are both sub key systems of com.example.somekeysystem.
  bool supports_sub_key_systems = false;

  // Display name of the CDM (e.g. Widevine Content Decryption Module).
  std::string name;

  // A token to uniquely identify this type of CDM.
  base::Token guid;

  // Version of the CDM. May be empty if the version is not known.
  base::Version version;

  // Path to the library implementing the CDM. May be empty if the
  // CDM is not a separate library (e.g. Widevine on Android).
  base::FilePath path;

  // Identifier used by the PluginPrivateFileSystem to identify the files
  // stored by this CDM. Valid identifiers only contain letters (A-Za-z),
  // digits(0-9), or "._-".
  std::string file_system_id;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CDM_INFO_H_
