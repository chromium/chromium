// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CDM_INFO_H_
#define CONTENT_PUBLIC_COMMON_CDM_INFO_H_

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/version.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "media/base/cdm_capability.h"
#include "media/base/content_decryption_module.h"
#include "media/base/encryption_scheme.h"
#include "media/base/video_codecs.h"
#include "media/cdm/cdm_type.h"

namespace content {

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
// CdmType for Chrome OS.
const CONTENT_EXPORT media::CdmType kChromeOsCdmType{0xa6ecd3fc63b3ded2ull,
                                                     0x9306d3270227ce5full};
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)

// Represents a Content Decryption Module implementation and its capabilities.
struct CONTENT_EXPORT CdmInfo {
  enum class Robustness {
    kHardwareSecure,
    kSoftwareSecure,
  };

  // Status of the `capability`. These values are persisted to logs. Entries
  // should not be renumbered and numeric values should never be reused.
  enum class Status {
    kUninitialized,  // Uninitialized; `capability` must be nullopt.
    kEnabled,  // Initialized and enabled; if `capability` is nullopt, then no
               // capability is supported.
    kCommandLineOverridden,  // Overridden from command line and enabled
    kHardwareSecureDecryptionDisabled,  // kHardwareSecureDecryption disabled
    kAcceleratedVideoDecodeDisabled,    // kDisableAcceleratedVideoDecode
    kGpuFeatureDisabled,      // gpu::DISABLE_MEDIA_FOUNDATION_HARDWARE_SECURITY
    kGpuCompositionDisabled,  // GPU (direct) composition disabled
    kDisabledByPref,  // Disabled due to previous errors (stored in Local State)
    kDisabledOnError,                // Disabled after errors or crashes
    kDisabledBySoftwareEmulatedGpu,  // Disabled by software emulated GPU
    kMaxValue = kDisabledBySoftwareEmulatedGpu,
  };

  // If `capability` is nullopt, the `capability` will be lazy initialized.
  CdmInfo(const std::string& key_system,
          Robustness robustness,
          std::optional<media::CdmCapability> capability,
          bool supports_sub_key_systems,
          const std::string& name,
          const media::CdmType& type,
          const base::Version& version,
          const base::FilePath& path);
  CdmInfo(const std::string& key_system,
          Robustness robustness,
          std::optional<media::CdmCapability> capability,
          const media::CdmType& type);
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
  std::optional<media::CdmCapability> capability;

  // Whether the CdmInfo is enabled etc. This only affects capability query.
  Status status = Status::kEnabled;

  // Whether we also support sub key systems of the `key_system`.
  // A sub key system to a key system is like a sub domain to a domain.
  // For example, com.example.somekeysystem.a and com.example.somekeysystem.b
  // are both sub key systems of com.example.somekeysystem.
  bool supports_sub_key_systems = false;

  // Display name of the CDM (e.g. Widevine Content Decryption Module).
  std::string name;

  // An object to uniquely identify the type of the CDM. Used for per-CDM-type
  // isolation, e.g. for running different CDMs in different child processes,
  // and per-CDM-type storage.
  media::CdmType type;

  // Version of the CDM. May be empty if the version is not known.
  base::Version version;

  // Path to the library implementing the CDM. May be empty if the
  // CDM is not a separate library (e.g. Widevine on Android).
  base::FilePath path;
};

CONTENT_EXPORT std::string GetCdmInfoRobustnessName(
    CdmInfo::Robustness robustness);

inline std::ostream& operator<<(std::ostream& os,
                                CdmInfo::Robustness robustness) {
  return os << GetCdmInfoRobustnessName(robustness);
}

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CDM_INFO_H_
