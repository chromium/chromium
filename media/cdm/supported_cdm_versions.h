// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_SUPPORTED_CDM_VERSIONS_H_
#define MEDIA_CDM_SUPPORTED_CDM_VERSIONS_H_

#include <stddef.h>

#include <array>

#include "media/base/media_export.h"
#include "media/cdm/api/content_decryption_module.h"

// A library CDM interface is "supported" if it's implemented by CdmAdapter and
// CdmWrapper. Typically multiple CDM interfaces are supported:
// - The latest stable CDM interface.
// - Previous stable CDM interface(s), for supporting older CDMs.
// - Experimental CDM interface(s), for development.
//
// A library CDM interface is "enabled" if it's enabled at runtime, e.g. being
// able to be registered and creating CDM instances. Experimental CDM interfaces
// must not be enabled by default.
//
// Whether a CDM interface is enabled can also be overridden by using command
// line switch switches::kOverrideEnabledCdmInterfaceVersion for finer control
// in a test environment or for local debugging, including enabling experimental
// CDM interfaces.

namespace media {

struct SupportedVersion {
  int version;
  bool enabled;
};

constexpr std::array<SupportedVersion, 2> kSupportedCdmInterfaceVersions = {{
    {10, true},
    {11, false},
}};

// In most cases CdmInterface::kVersion == CdmInterface::Host::kVersion. However
// this is not guaranteed. For example, a newer CDM interface may use an
// existing CDM host. So we keep CDM host support separate from CDM interface
// support. In CdmInterfaceTraits we also static assert that for supported CDM
// interface, CdmInterface::Host::kVersion must also be supported.
constexpr int kMinSupportedCdmHostVersion = 10;
constexpr int kMaxSupportedCdmHostVersion = 11;

constexpr bool IsSupportedCdmModuleVersion(int version) {
  return version == CDM_MODULE_VERSION;
}

// Returns whether the CDM interface of |version| is supported in the
// implementation.
constexpr bool IsSupportedCdmInterfaceVersion(int version) {
  for (size_t i = 0; i < kSupportedCdmInterfaceVersions.size(); ++i) {
    if (kSupportedCdmInterfaceVersions[i].version == version)
      return true;
  }

  return false;
}

// Returns whether the CDM host interface of |version| is supported in the
// implementation. Currently there's no way to disable a supported CDM host
// interface at run time.
constexpr bool IsSupportedCdmHostVersion(int version) {
  return kMinSupportedCdmHostVersion <= version &&
         version <= kMaxSupportedCdmHostVersion;
}

// Returns whether the CDM interface of |version| is enabled by default.
constexpr bool IsCdmInterfaceVersionEnabledByDefault(int version) {
  for (size_t i = 0; i < kSupportedCdmInterfaceVersions.size(); ++i) {
    if (kSupportedCdmInterfaceVersions[i].version == version)
      return kSupportedCdmInterfaceVersions[i].enabled;
  }

  return false;
}

// Returns whether the CDM interface of |version| is supported in the
// implementation and enabled at runtime.
MEDIA_EXPORT bool IsSupportedAndEnabledCdmInterfaceVersion(int version);

typedef bool (*VersionCheckFunc)(int version);

// Returns true if all versions in the range [min_version, max_version] and no
// versions outside the range are supported, and false otherwise.
constexpr bool CheckVersions(VersionCheckFunc check_func,
                             int min_version,
                             int max_version) {
  // For simplicity, only check one version out of the range boundary.
  if (check_func(min_version - 1) || check_func(max_version + 1))
    return false;

  for (int version = min_version; version <= max_version; ++version) {
    if (!check_func(version))
      return false;
  }

  return true;
}

// Ensures CDM interface versions in and only in the range [min_version,
// max_version] are supported in the implementation.
constexpr bool CheckSupportedCdmInterfaceVersions(int min_version,
                                                  int max_version) {
  return CheckVersions(IsSupportedCdmInterfaceVersion, min_version,
                       max_version);
}

// Ensures CDM host interface versions in and only in the range [min_version,
// max_version] are supported in the implementation.
constexpr bool CheckSupportedCdmHostVersions(int min_version, int max_version) {
  return CheckVersions(IsSupportedCdmHostVersion, min_version, max_version);
}

// Traits for CDM Interfaces
template <int CdmInterfaceVersion>
struct CdmInterfaceTraits {};

// TODO(xhwang): CDM_9 support has been removed; consider to use a macro to
// help define CdmInterfaceTraits specializations.
template <>
struct CdmInterfaceTraits<10> {
  using CdmInterface = cdm::ContentDecryptionModule_10;
  static_assert(CdmInterface::kVersion == 10, "CDM interface version mismatch");
  static_assert(IsSupportedCdmHostVersion(CdmInterface::Host::kVersion),
                "Host not supported");
  static_assert(
      CdmInterface::kIsStable ||
          !IsCdmInterfaceVersionEnabledByDefault(CdmInterface::kVersion),
      "Experimental CDM interface should not be enabled by default");
};

template <>
struct CdmInterfaceTraits<11> {
  using CdmInterface = cdm::ContentDecryptionModule_11;
  static_assert(CdmInterface::kVersion == 11, "CDM interface version mismatch");
  static_assert(IsSupportedCdmHostVersion(CdmInterface::Host::kVersion),
                "Host not supported");
  static_assert(
      CdmInterface::kIsStable ||
          !IsCdmInterfaceVersionEnabledByDefault(CdmInterface::kVersion),
      "Experimental CDM interface should not be enabled by default");
};

}  // namespace media

#endif  // MEDIA_CDM_SUPPORTED_CDM_VERSIONS_H_
