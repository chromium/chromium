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
  kSuccess,                    // Succeeded
  kUnknownError,               // Unknown error.
  kCdmCreationAborted,         // CDM creation aborted.
  kLoadCdmFailed,              // Failed to load the CDM.
  kCreateCdmFuncNotAvailable,  // CreateCdmFunc not available.
  kCdmHelperCreationFailed,    // CDM helper creation failed.
  kGetCdmPrefDataFailed,       // Failed to get the CDM preference data.
  kGetCdmOriginIdFailed,       // Failed to get the CDM origin ID.
  kInitCdmFailed,              // Failed to initialize CDM.
  kCdmFactoryCreationFailed,   // CDM Factory creation failed.
  kCdmNotSupported,            // CDM not supported.
  kInvalidCdmConfig,  // Invalid CdmConfig. e.g. MediaFoundationService requires
                      // both distinctive identifier and persistent state.
  kUnsupportedKeySystem,  // Unsupported key system.
  kDisconnectionError,    // Disconnection error. The remote process dropped the
                          // callback. e.g. in case of crash.
  kNotAllowedOnUniqueOrigin,         // EME use is not allowed on unique
                                     // origins.
  kMediaDrmBridgeCreationFailed,     // Android: MediaDrmBridge creation failed.
  kMediaCryptoNotAvailable,          // Android: MediaCrypto not available.
  kNoMoreInstances,                  // CrOs: Only one instance allowed.
  kInsufficientGpuResources,         // CrOs: Insufficient GPU memory
                                     // available.
  kCrOsVerifiedAccessDisabled,       // CrOs: Verified Access is disabled.
  kCrOsRemoteFactoryCreationFailed,  // CrOs: Remote factory creation failed.
  kAndroidMediaDrmIllegalArgument,   // Android: Illegal argument passed to
                                     // MediaDrm.
  kAndroidMediaDrmIllegalState,   // Android: MediaDrm not initialized properly.
  kAndroidFailedL1SecurityLevel,  // Android: Unable to set L1 security level.
  kAndroidFailedL3SecurityLevel,  // Android: Unable to set L3 security level.
  kAndroidFailedSecurityOrigin,   // Android: Unable to set origin.
  kAndroidFailedMediaCryptoSession,   // Android: Unable to create MediaCrypto
                                      // session.
  kAndroidFailedToStartProvisioning,  // Android: Unable to start provisioning.
  kAndroidFailedMediaCryptoCreate,    // Android: Unable to create MediaCrypto
                                      // object.
  kAndroidUnsupportedMediaCryptoScheme,  // Android: Crypto scheme not
                                         // supported.
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
