// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CDM_CAPABILITY_H_
#define MEDIA_BASE_CDM_CAPABILITY_H_

#include <map>
#include <optional>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "media/base/audio_codecs.h"
#include "media/base/content_decryption_module.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"

namespace media {

static constexpr char kMediaFoundationGetCdmFactoryHresultUmaPostfix[] =
    "MediaFoundationGetCdmFactoryHresult";
static constexpr char kCreateDummyMediaFoundationCdmHresultUmaPostfix[] =
    "CreateDummyMediaFoundationCdmHresult";

struct MEDIA_EXPORT VideoCodecInfo {
  VideoCodecInfo();
  VideoCodecInfo(base::flat_set<VideoCodecProfile> supported_profiles,
                 bool supports_clear_lead);
  explicit VideoCodecInfo(base::flat_set<VideoCodecProfile> supported_profiles);
  VideoCodecInfo(const VideoCodecInfo& other);
  ~VideoCodecInfo();

  // Set of VideoCodecProfiles supported. If no profiles for a
  // particular codec are specified, then it is assumed that all
  // profiles are supported by the CDM.
  base::flat_set<VideoCodecProfile> supported_profiles;

  // We default to supports_clear_lead = true because in majority of cases,
  // the CDM does support clear lead. In a few cases, (b/231241602), we
  // need to adjust this boolean to handle cases where clear lead results
  // in issues for the user.
  bool supports_clear_lead = true;
};

bool MEDIA_EXPORT operator==(const VideoCodecInfo& lhs,
                             const VideoCodecInfo& rhs);

// Capabilities supported by a Content Decryption Module.
struct MEDIA_EXPORT CdmCapability {
  using VideoCodecMap = std::map<VideoCodec, VideoCodecInfo>;
  CdmCapability();
  CdmCapability(base::flat_set<AudioCodec> audio_codecs,
                VideoCodecMap video_codecs,
                base::flat_set<EncryptionScheme> encryption_schemes,
                base::flat_set<CdmSessionType> session_types,
                base::Version version);
  CdmCapability(const CdmCapability& other);
  ~CdmCapability();

  // List of audio codecs supported by the CDM (e.g. opus). This is the set of
  // codecs supported by the media pipeline using the CDM. This does not include
  // codec profiles, as in general Chromium doesn't handle audio codec profiles
  // separately.
  base::flat_set<AudioCodec> audio_codecs;

  // Map of video codecs and a struct containing the associated profiles
  // supported by the CDM (e.g. vp8) and whether clear lead is supported.
  VideoCodecMap video_codecs;

  // List of encryption schemes supported by the CDM (e.g. cenc).
  base::flat_set<EncryptionScheme> encryption_schemes;

  // List of session types supported by the CDM.
  base::flat_set<CdmSessionType> session_types;

  // Version of the CDM. May be empty if the version is not known.
  base::Version version;
};

bool MEDIA_EXPORT operator==(const CdmCapability& lhs,
                             const CdmCapability& rhs);

// Status of the CDM capability query. This can be used to inspect the reason
// when no capability reported. These values are persisted to logs. Entries
// should not be renumbered and numeric values should never be reused. Please
// keep the consistency with CdmCapabilityQueryStatus in
// tools/metrics/histograms/metadata/media/enums.xml.
enum class CdmCapabilityQueryStatus {
  // Capability query succeeded. Used only for reporting the result UMA when one
  // or more capabilities reported.
  kSuccess = 0,
  // Unknown. e.g. CDM is disabled for some reasons.
  kUnknown = 1,
  // Hardware secure codec not supported for the key system.
  kHardwareSecureCodecNotSupported = 2,
  // No supported video codec.
  kNoSupportedVideoCodec = 3,
  // No supported encryption scheme.
  kNoSupportedEncryptionScheme = 4,
  // Unsupported key system.
  kUnsupportedKeySystem = 5,
  // MediaFoundation CDM is not supported. e.g. Unsupported OS version. Note
  // that this is only applicable on Windows.
  kMediaFoundationCdmNotSupported = 6,
  // Disconnection error. e.g. crash while checking capability.
  kDisconnectionError = 7,
  // MediaFoundationCdmModule::GetCdmFactory failed with error code (HRESULT).
  kMediaFoundationGetCdmFactoryFailed = 8,
  // CreateDummyMediaFoundationCdm failed with error code (HRESULT).
  kCreateDummyMediaFoundationCdmFailed = 9,
  // Unexpected empty video codec, encryption scheme or session type.
  kUnexpectedEmptyCapability = 10,
  // MediaDrm not available for the key system and robustness specified.
  kNoMediaDrmSupport = 11,
  // Creation of IMFExtendedDRMTypeSupport failed.
  kMediaFoundationGetExtendedDRMTypeSupportFailed = 12,
  kMaxValue = kMediaFoundationGetExtendedDRMTypeSupportFailed,
};

// Returns a string version of the status.
MEDIA_EXPORT std::string CdmCapabilityQueryStatusToString(
    const std::optional<CdmCapabilityQueryStatus>& status);

// CdmCapability or status if no capability.
using CdmCapabilityOrStatus =
    base::expected<CdmCapability, CdmCapabilityQueryStatus>;

// Callback for when a capability is initialized if lazy initialization
// required. CdmCapabilityQueryStatus can be used to inspect the reason behind
// no capability.
using CdmCapabilityCB = base::OnceCallback<void(CdmCapabilityOrStatus)>;
}  // namespace media

#endif  // MEDIA_BASE_CDM_CAPABILITY_H_
