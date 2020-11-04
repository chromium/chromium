// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CDM_INFO_H_
#define CONTENT_PUBLIC_COMMON_CDM_INFO_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/token.h"
#include "base/version.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "media/base/content_decryption_module.h"
#include "media/base/encryption_scheme.h"
#include "media/base/video_codecs.h"

namespace content {

// Capabilites supported by a Content Decryption Module.
struct CONTENT_EXPORT CdmCapability {
  CdmCapability();
  CdmCapability(std::vector<media::VideoCodec> video_codecs,
                base::flat_set<media::EncryptionScheme> encryption_schemes,
                base::flat_set<media::CdmSessionType> session_types);
  CdmCapability(const CdmCapability& other);
  ~CdmCapability();

  // List of video codecs supported by the CDM (e.g. vp8). This is the set of
  // codecs that can be decrypted and decoded by the CDM. As this is generic,
  // not all profiles or levels of the specified codecs may actually be
  // supported.
  // TODO(crbug.com/796725) Find a way to include profiles and levels.
  std::vector<media::VideoCodec> video_codecs;

  // When VP9 is supported in |video_codecs|, whether profile 2 is supported.
  // This is needed because there are older CDMs that only supports profile 0.
  // TODO(xhwang): Remove this after older CDMs that only supports VP9 profile 0
  // are obsolete.
  bool supports_vp9_profile2 = false;

  // List of encryption schemes supported by the CDM (e.g. cenc).
  base::flat_set<media::EncryptionScheme> encryption_schemes;

  // List of session types supported by the CDM.
  base::flat_set<media::CdmSessionType> session_types;
};

// Represents a Content Decryption Module implementation and its capabilities.
struct CONTENT_EXPORT CdmInfo {
  CdmInfo(const std::string& name,
          const base::Token& guid,
          const base::Version& version,
          const base::FilePath& path,
          const std::string& file_system_id,
          CdmCapability capability,
          const std::string& supported_key_system,
          bool supports_sub_key_systems);
  CdmInfo(const CdmInfo& other);
  ~CdmInfo();

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

  // CDM capability, e.g. video codecs, encryption schemes and session types.
  CdmCapability capability;

  // The key system supported by this CDM.
  std::string supported_key_system;

  // Whether we also support sub key systems of the |supported_key_system|.
  // A sub key system to a key system is like a sub domain to a domain.
  // For example, com.example.somekeysystem.a and com.example.somekeysystem.b
  // are both sub key systems of com.example.somekeysystem.
  bool supports_sub_key_systems;

#if defined(OS_MAC) && defined(ARCH_CPU_ARM64)
  // If true, the child process will be launched as x86_64 code under Rosetta
  // translation.
  bool launch_x86_64 = false;
#endif  // OS_MAC && ARCH_CPU_ARM64
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CDM_INFO_H_
