// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CDM_FACTORY_H_
#define MEDIA_BASE_CDM_FACTORY_H_

#include "media/base/content_decryption_module.h"
#include "media/base/media_export.h"

namespace url {
class Origin;
}

namespace media {

// CDM creation status.
// These are reported to UMA server. Do not renumber or reuse values.
enum class CreateCdmStatus {
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
  kMediaDrmBridgeCreationFailed,     // MediaDrmBridge creation failed.
  kMediaCryptoNotAvailable,          // MediaCrypto not available.
  kNoMoreInstances,                  // CrOs: Only one instance allowed.
  kInsufficientGpuResources,         // CrOs: Insufficient GPU memory
                                     // available.
  kCrOsVerifiedAccessDisabled,       // CrOs: Verified Access is disabled.
  kCrOsRemoteFactoryCreationFailed,  // CrOs: Remote factory creation failed.
  kMaxValue = kCrOsRemoteFactoryCreationFailed,
};

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
