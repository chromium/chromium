// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CLEAR_KEY_CDM_COMMON_H_
#define MEDIA_CDM_CLEAR_KEY_CDM_COMMON_H_

#include "build/build_config.h"
#include "media/cdm/cdm_type.h"
#include "media/media_buildflags.h"

namespace media {

// Clear Key key system defined in the EME spec.
inline constexpr char kClearKeyKeySystem[] = "org.w3.clearkey";

// UUID from http://dashif.org/identifiers/content_protection/. UUIDs are used
// in Android for creating MediaDRM objects that support the DRM scheme required
// by content.
#if BUILDFLAG(IS_ANDROID)
inline const uint8_t kClearKeyUuid[16] = {
    0xE2, 0x71, 0x9D, 0x58, 0xA9, 0x85, 0xB3, 0xC9,  //
    0x78, 0x1A, 0xB0, 0X30, 0xAF, 0x78, 0xD3, 0x0E   //
};
#endif

// External Clear Key key system ("org.chromium.externalclearkey" and variants)
// only for testing.
inline constexpr char kExternalClearKeyKeySystem[] =
    "org.chromium.externalclearkey";

// Variants of External Clear Key key system to test different scenarios.
// To add a new variant, make sure you also update:
// - media/test/data/eme_player_js/globals.js
// - media/test/data/eme_player_js/player_utils.js
// - CreateCdmInstance() in clear_key_cdm.cc

#if BUILDFLAG(IS_WIN)
// MediaFoundation Clear Key key system only for testing.
inline constexpr char kMediaFoundationClearKeyKeySystem[] =
    "org.chromium.externalclearkey.mediafoundation";

inline constexpr wchar_t kMediaFoundationClearKeyKeySystemWideString[] =
    L"org.chromium.externalclearkey.mediafoundation";
#endif  // BUILDFLAG(IS_WIN)

// A sub key system that is invalid for testing purpose.
inline constexpr char kExternalClearKeyInvalidKeySystem[] =
    "org.chromium.externalclearkey.invalid";

// A sub key system that supports decrypt-only mode.
inline constexpr char kExternalClearKeyDecryptOnlyKeySystem[] =
    "org.chromium.externalclearkey.decryptonly";

// A sub key system that triggers various types of messages.
inline constexpr char kExternalClearKeyMessageTypeTestKeySystem[] =
    "org.chromium.externalclearkey.messagetypetest";

// A sub key system that triggers the FileIO test.
inline constexpr char kExternalClearKeyFileIOTestKeySystem[] =
    "org.chromium.externalclearkey.fileiotest";

// A sub key system that triggers the output protection test.
inline constexpr char kExternalClearKeyOutputProtectionTestKeySystem[] =
    "org.chromium.externalclearkey.outputprotectiontest";

// A sub key system that triggers the platform verification test.
inline constexpr char kExternalClearKeyPlatformVerificationTestKeySystem[] =
    "org.chromium.externalclearkey.platformverificationtest";

// A sub key system that triggers a crash.
inline constexpr char kExternalClearKeyCrashKeySystem[] =
    "org.chromium.externalclearkey.crash";

// A sub key system that triggers the verify host files test.
inline constexpr char kExternalClearKeyVerifyCdmHostTestKeySystem[] =
    "org.chromium.externalclearkey.verifycdmhosttest";

// A sub key system that fetches the Storage ID.
inline constexpr char kExternalClearKeyStorageIdTestKeySystem[] =
    "org.chromium.externalclearkey.storageidtest";

// A sub key system that is registered with a different CDM type.
inline constexpr char kExternalClearKeyDifferentCdmTypeTestKeySystem[] =
    "org.chromium.externalclearkey.differentcdmtype";

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
// Name of the ClearKey CDM library.
inline constexpr char kClearKeyCdmLibraryName[] = "clearkeycdm";

inline constexpr char kClearKeyCdmBaseDirectory[] =
#if BUILDFLAG(IS_FUCHSIA)
    "lib/"
#endif
    "ClearKeyCdm";

// Display name for Clear Key CDM.
inline constexpr char kClearKeyCdmDisplayName[] = "Clear Key CDM";
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(IS_WIN)
// Name of the MediaFoundation ClearKey CDM library.
inline constexpr char kMediaFoundationClearKeyCdmLibraryName[] =
    "MediaFoundation.ClearKey.CDM";

// Display name for MediaFoundation Clear Key CDM.
inline constexpr char kMediaFoundationClearKeyCdmDisplayName[] =
    "Media Foundation Clear Key CDM";
#endif  // BUILDFLAG(IS_WIN)

// The default GUID for Clear Key Cdm.
const CdmType kClearKeyCdmType{0x3a2e0fadde4bd1b7ull, 0xcb90df3e240d1694ull};

// A different GUID for Clear Key Cdm for testing running different types of
// CDMs in the system.
const CdmType kClearKeyCdmDifferentCdmType{0xc3914773474bdb02ull,
                                           0x8e8de4d84d3ca030ull};

#if BUILDFLAG(IS_WIN)
// The default GUID for MediaFoundation Clear Key Cdm.
const CdmType kMediaFoundationClearKeyCdmType{0xbec8776b734d80faull,
                                              0xdff8375bb3cb3df8ull};
#endif  // BUILDFLAG(IS_WIN)

}  // namespace media

#endif  // MEDIA_CDM_CLEAR_KEY_CDM_COMMON_H_
