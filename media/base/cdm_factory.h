// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CDM_FACTORY_H_
#define MEDIA_BASE_CDM_FACTORY_H_

#include "media/base/content_decryption_module.h"
#include "media/base/media_export.h"
#include "media/base/status.h"

namespace url {
class Origin;
}

namespace media {

// CDM creation status.
// These are reported to UMA server. Do not renumber or reuse values.
enum class CreateCdmStatus : StatusCodeType {
  // Succeeded.
  kSuccess = 0,
  // Unknown error.
  kUnknownError = 1,
  // CDM creation aborted.
  kCdmCreationAborted = 2,
  // 3 was kLoadCdmFailed; no longer used.
  // CreateCdmFunc not available.
  kCreateCdmFuncNotAvailable = 4,
  // CDM helper creation failed.
  kCdmHelperCreationFailed = 5,
  // Failed to get the CDM preference data.
  kGetCdmPrefDataFailed = 6,
  // Failed to get the CDM origin ID.
  kGetCdmOriginIdFailed = 7,
  // Failed to initialize CDM.
  kInitCdmFailed = 8,
  // CDM Factory creation failed.
  kCdmFactoryCreationFailed = 9,
  // CDM not supported.
  kCdmNotSupported = 10,
  // Invalid CdmConfig. e.g. MediaFoundationService requires both distinctive
  // identifier and persistent state.
  kInvalidCdmConfig = 11,
  // Unsupported key system.
  kUnsupportedKeySystem = 12,
  // Disconnection error. The remote process dropped the callback. e.g. in case
  // of crash.
  kDisconnectionError = 13,
  // EME use is not allowed on unique origins.
  kNotAllowedOnUniqueOrigin = 14,
  // 15 was kMediaDrmBridgeCreationFailed; no longer used as we now use more
  // detailed statuses.
  // Android: MediaCrypto not available.
  kMediaCryptoNotAvailable = 16,
  // CrOs: Only one instance allowed.
  kNoMoreInstances = 17,
  // CrOs: Insufficient GPU memory available.
  kInsufficientGpuResources = 18,
  // CrOs: Verified Access is disabled.
  kCrOsVerifiedAccessDisabled = 19,
  // CrOs: Remote factory creation failed.
  kCrOsRemoteFactoryCreationFailed = 20,
  // Android: Illegal argument passed to MediaDrm.
  kAndroidMediaDrmIllegalArgument = 21,
  // Android: MediaDrm not initialized properly.
  kAndroidMediaDrmIllegalState = 22,
  // Android: Unable to set L1 security level.
  kAndroidFailedL1SecurityLevel = 23,
  // Android: Unable to set L3 security level.
  kAndroidFailedL3SecurityLevel = 24,
  // Android: Unable to set origin.
  kAndroidFailedSecurityOrigin = 25,
  // Android: Unable to create MediaCrypto session.
  kAndroidFailedMediaCryptoSession = 26,
  // Android: Unable to start provisioning.
  kAndroidFailedToStartProvisioning = 27,
  // Android: Unable to create MediaCrypto object.
  kAndroidFailedMediaCryptoCreate = 28,
  // Android: Crypto scheme not supported.
  kAndroidUnsupportedMediaCryptoScheme = 29,

  kMaxValue = kAndroidUnsupportedMediaCryptoScheme,
};

struct CreateCdmStatusTraits {
  using Codes = CreateCdmStatus;
  static constexpr StatusGroupType Group() { return "CreateCdmStatus"; }
  static constexpr Codes OkEnumValue() { return Codes::kSuccess; }
};

using CreateCdmTypedStatus = TypedStatus<CreateCdmStatusTraits>;

// Callback used when CDM is created. |status| tells the detailed reason why CDM
// can't be created if ContentDecryptionModule is null.
using CdmCreatedCB =
    base::OnceCallback<void(const scoped_refptr<ContentDecryptionModule>&,
                            CreateCdmStatus status)>;

struct CdmConfig;

class MEDIA_EXPORT CdmFactory {
 public:
  CdmFactory();

  CdmFactory(const CdmFactory&) = delete;
  CdmFactory& operator=(const CdmFactory&) = delete;

  virtual ~CdmFactory();

  // Creates a CDM for |cdm_config| and returns it through |cdm_created_cb|
  // asynchronously.
  virtual void Create(
      const CdmConfig& cdm_config,
      const SessionMessageCB& session_message_cb,
      const SessionClosedCB& session_closed_cb,
      const SessionKeysChangeCB& session_keys_change_cb,
      const SessionExpirationUpdateCB& session_expiration_update_cb,
      CdmCreatedCB cdm_created_cb) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_CDM_FACTORY_H_
