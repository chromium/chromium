// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_MEDIA_TYPES_ENUM_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_MEDIA_TYPES_ENUM_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "build/build_config.h"
#include "media/base/cdm_factory.h"
#include "media/base/renderer_factory_selector.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/video_transformation.h"
#include "media/cdm/cdm_document_service.h"
#include "media/mojo/mojom/media_types.mojom-shared.h"

// Most enums have automatically generated traits, in media_types.mojom.h, due
// to their [native] attribute. This file defines traits for enums that are used
// in files that cannot directly include media_types.mojom.h.

namespace mojo {

#if BUILDFLAG(IS_WIN)
template <>
struct EnumTraits<media::mojom::CdmEvent, ::media::CdmEvent> {
  static media::mojom::CdmEvent ToMojom(::media::CdmEvent input) {
    switch (input) {
      case ::media::CdmEvent::kSignificantPlayback:
        return media::mojom::CdmEvent::kSignificantPlayback;
      case ::media::CdmEvent::kPlaybackError:
        return media::mojom::CdmEvent::kPlaybackError;
      case ::media::CdmEvent::kCdmError:
        return media::mojom::CdmEvent::kCdmError;
      case ::media::CdmEvent::kHardwareContextReset:
        return media::mojom::CdmEvent::kHardwareContextReset;
    }

    NOTREACHED();
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static bool FromMojom(media::mojom::CdmEvent input,
                        ::media::CdmEvent* output) {
    switch (input) {
      case media::mojom::CdmEvent::kSignificantPlayback:
        *output = ::media::CdmEvent::kSignificantPlayback;
        return true;
      case media::mojom::CdmEvent::kPlaybackError:
        *output = ::media::CdmEvent::kPlaybackError;
        return true;
      case media::mojom::CdmEvent::kCdmError:
        *output = ::media::CdmEvent::kCdmError;
        return true;
      case media::mojom::CdmEvent::kHardwareContextReset:
        *output = ::media::CdmEvent::kHardwareContextReset;
        return true;
    }

    NOTREACHED();
  }
};
#endif  // BUILDFLAG(IS_WIN)

template <>
struct EnumTraits<media::mojom::CdmSessionClosedReason,
                  ::media::CdmSessionClosedReason> {
  static media::mojom::CdmSessionClosedReason ToMojom(
      ::media::CdmSessionClosedReason input) {
    switch (input) {
      case ::media::CdmSessionClosedReason::kInternalError:
        return media::mojom::CdmSessionClosedReason::kInternalError;
      case ::media::CdmSessionClosedReason::kClose:
        return media::mojom::CdmSessionClosedReason::kClose;
      case ::media::CdmSessionClosedReason::kReleaseAcknowledged:
        return media::mojom::CdmSessionClosedReason::kReleaseAcknowledged;
      case ::media::CdmSessionClosedReason::kHardwareContextReset:
        return media::mojom::CdmSessionClosedReason::kHardwareContextReset;
      case ::media::CdmSessionClosedReason::kResourceEvicted:
        return media::mojom::CdmSessionClosedReason::kResourceEvicted;
    }

    NOTREACHED();
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static bool FromMojom(media::mojom::CdmSessionClosedReason input,
                        ::media::CdmSessionClosedReason* output) {
    switch (input) {
      case media::mojom::CdmSessionClosedReason::kInternalError:
        *output = ::media::CdmSessionClosedReason::kInternalError;
        return true;
      case media::mojom::CdmSessionClosedReason::kClose:
        *output = ::media::CdmSessionClosedReason::kClose;
        return true;
      case media::mojom::CdmSessionClosedReason::kReleaseAcknowledged:
        *output = ::media::CdmSessionClosedReason::kReleaseAcknowledged;
        return true;
      case media::mojom::CdmSessionClosedReason::kHardwareContextReset:
        *output = ::media::CdmSessionClosedReason::kHardwareContextReset;
        return true;
      case media::mojom::CdmSessionClosedReason::kResourceEvicted:
        *output = ::media::CdmSessionClosedReason::kResourceEvicted;
        return true;
    }

    NOTREACHED();
  }
};

template <>
struct EnumTraits<media::mojom::EncryptionType, ::media::EncryptionType> {
  static media::mojom::EncryptionType ToMojom(::media::EncryptionType input) {
    switch (input) {
      case ::media::EncryptionType::kNone:
        return media::mojom::EncryptionType::kNone;
      case ::media::EncryptionType::kClear:
        return media::mojom::EncryptionType::kClear;
      case ::media::EncryptionType::kEncrypted:
        return media::mojom::EncryptionType::kEncrypted;
      case ::media::EncryptionType::kEncryptedWithClearLead:
        return media::mojom::EncryptionType::kEncryptedWithClearLead;
    }

    NOTREACHED();
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static bool FromMojom(media::mojom::EncryptionType input,
                        ::media::EncryptionType* output) {
    switch (input) {
      case media::mojom::EncryptionType::kNone:
        *output = ::media::EncryptionType::kNone;
        return true;
      case media::mojom::EncryptionType::kClear:
        *output = ::media::EncryptionType::kClear;
        return true;
      case media::mojom::EncryptionType::kEncrypted:
        *output = ::media::EncryptionType::kEncrypted;
        return true;
      case media::mojom::EncryptionType::kEncryptedWithClearLead:
        *output = ::media::EncryptionType::kEncryptedWithClearLead;
        return true;
    }

    NOTREACHED();
  }
};

template <>
struct EnumTraits<media::mojom::SVCScalabilityMode, media::SVCScalabilityMode> {
  static media::mojom::SVCScalabilityMode ToMojom(
      media::SVCScalabilityMode input) {
    switch (input) {
      case media::SVCScalabilityMode::kL1T1:
        return media::mojom::SVCScalabilityMode::kL1T1;
      case media::SVCScalabilityMode::kL1T2:
        return media::mojom::SVCScalabilityMode::kL1T2;
      case media::SVCScalabilityMode::kL1T3:
        return media::mojom::SVCScalabilityMode::kL1T3;
      case media::SVCScalabilityMode::kL2T1:
        return media::mojom::SVCScalabilityMode::kL2T1;
      case media::SVCScalabilityMode::kL2T2:
        return media::mojom::SVCScalabilityMode::kL2T2;
      case media::SVCScalabilityMode::kL2T3:
        return media::mojom::SVCScalabilityMode::kL2T3;
      case media::SVCScalabilityMode::kL3T1:
        return media::mojom::SVCScalabilityMode::kL3T1;
      case media::SVCScalabilityMode::kL3T2:
        return media::mojom::SVCScalabilityMode::kL3T2;
      case media::SVCScalabilityMode::kL3T3:
        return media::mojom::SVCScalabilityMode::kL3T3;
      case media::SVCScalabilityMode::kL2T1Key:
        return media::mojom::SVCScalabilityMode::kL2T1Key;
      case media::SVCScalabilityMode::kL2T2Key:
        return media::mojom::SVCScalabilityMode::kL2T2Key;
      case media::SVCScalabilityMode::kL2T3Key:
        return media::mojom::SVCScalabilityMode::kL2T3Key;
      case media::SVCScalabilityMode::kL3T1Key:
        return media::mojom::SVCScalabilityMode::kL3T1Key;
      case media::SVCScalabilityMode::kL3T2Key:
        return media::mojom::SVCScalabilityMode::kL3T2Key;
      case media::SVCScalabilityMode::kL3T3Key:
        return media::mojom::SVCScalabilityMode::kL3T3Key;
      case media::SVCScalabilityMode::kS2T1:
        return media::mojom::SVCScalabilityMode::kS2T1;
      case media::SVCScalabilityMode::kS2T2:
        return media::mojom::SVCScalabilityMode::kS2T2;
      case media::SVCScalabilityMode::kS2T3:
        return media::mojom::SVCScalabilityMode::kS2T3;
      case media::SVCScalabilityMode::kS3T1:
        return media::mojom::SVCScalabilityMode::kS3T1;
      case media::SVCScalabilityMode::kS3T2:
        return media::mojom::SVCScalabilityMode::kS3T2;
      case media::SVCScalabilityMode::kS3T3:
        return media::mojom::SVCScalabilityMode::kS3T3;
      case media::SVCScalabilityMode::kL2T1h:
      case media::SVCScalabilityMode::kL2T2h:
      case media::SVCScalabilityMode::kL2T3h:
      case media::SVCScalabilityMode::kS2T1h:
      case media::SVCScalabilityMode::kS2T2h:
      case media::SVCScalabilityMode::kS2T3h:
      case media::SVCScalabilityMode::kS3T1h:
      case media::SVCScalabilityMode::kS3T2h:
      case media::SVCScalabilityMode::kS3T3h:
      case media::SVCScalabilityMode::kL2T2KeyShift:
      case media::SVCScalabilityMode::kL2T3KeyShift:
      case media::SVCScalabilityMode::kL3T2KeyShift:
      case media::SVCScalabilityMode::kL3T3KeyShift:
      case media::SVCScalabilityMode::kL3T1h:
      case media::SVCScalabilityMode::kL3T2h:
      case media::SVCScalabilityMode::kL3T3h:
        NOTREACHED();
    }
  }

  static bool FromMojom(media::mojom::SVCScalabilityMode input,
                        media::SVCScalabilityMode* output) {
    switch (input) {
      case media::mojom::SVCScalabilityMode::kUnsupportedMode:
        NOTREACHED();
      case media::mojom::SVCScalabilityMode::kL1T1:
        *output = media::SVCScalabilityMode::kL1T1;
        return true;
      case media::mojom::SVCScalabilityMode::kL1T2:
        *output = media::SVCScalabilityMode::kL1T2;
        return true;
      case media::mojom::SVCScalabilityMode::kL1T3:
        *output = media::SVCScalabilityMode::kL1T3;
        return true;
      case media::mojom::SVCScalabilityMode::kL2T1:
        *output = media::SVCScalabilityMode::kL2T1;
        return true;
      case media::mojom::SVCScalabilityMode::kL2T2:
        *output = media::SVCScalabilityMode::kL2T2;
        return true;
      case media::mojom::SVCScalabilityMode::kL2T3:
        *output = media::SVCScalabilityMode::kL2T3;
        return true;
      case media::mojom::SVCScalabilityMode::kL3T1:
        *output = media::SVCScalabilityMode::kL3T1;
        return true;
      case media::mojom::SVCScalabilityMode::kL3T2:
        *output = media::SVCScalabilityMode::kL3T2;
        return true;
      case media::mojom::SVCScalabilityMode::kL3T3:
        *output = media::SVCScalabilityMode::kL3T3;
        return true;
      case media::mojom::SVCScalabilityMode::kL2T1Key:
        *output = media::SVCScalabilityMode::kL2T1Key;
        return true;
      case media::mojom::SVCScalabilityMode::kL2T2Key:
        *output = media::SVCScalabilityMode::kL2T2Key;
        return true;
      case media::mojom::SVCScalabilityMode::kL2T3Key:
        *output = media::SVCScalabilityMode::kL2T3Key;
        return true;
      case media::mojom::SVCScalabilityMode::kL3T1Key:
        *output = media::SVCScalabilityMode::kL3T1Key;
        return true;
      case media::mojom::SVCScalabilityMode::kL3T2Key:
        *output = media::SVCScalabilityMode::kL3T2Key;
        return true;
      case media::mojom::SVCScalabilityMode::kL3T3Key:
        *output = media::SVCScalabilityMode::kL3T3Key;
        return true;
      case media::mojom::SVCScalabilityMode::kS2T1:
        *output = media::SVCScalabilityMode::kS2T1;
        return true;
      case media::mojom::SVCScalabilityMode::kS2T2:
        *output = media::SVCScalabilityMode::kS2T2;
        return true;
      case media::mojom::SVCScalabilityMode::kS2T3:
        *output = media::SVCScalabilityMode::kS2T3;
        return true;
      case media::mojom::SVCScalabilityMode::kS3T1:
        *output = media::SVCScalabilityMode::kS3T1;
        return true;
      case media::mojom::SVCScalabilityMode::kS3T2:
        *output = media::SVCScalabilityMode::kS3T2;
        return true;
      case media::mojom::SVCScalabilityMode::kS3T3:
        *output = media::SVCScalabilityMode::kS3T3;
        return true;
    }

    NOTREACHED();
  }
};

template <>
struct EnumTraits<media::mojom::SVCInterLayerPredMode,
                  media::SVCInterLayerPredMode> {
  static media::mojom::SVCInterLayerPredMode ToMojom(
      media::SVCInterLayerPredMode input) {
    switch (input) {
      case media::SVCInterLayerPredMode::kOff:
        return media::mojom::SVCInterLayerPredMode::kOff;
      case media::SVCInterLayerPredMode::kOn:
        return media::mojom::SVCInterLayerPredMode::kOn;
      case media::SVCInterLayerPredMode::kOnKeyPic:
        return media::mojom::SVCInterLayerPredMode::kOnKeyPic;
    }
    NOTREACHED();
  }

  static bool FromMojom(media::mojom::SVCInterLayerPredMode input,
                        media::SVCInterLayerPredMode* output) {
    switch (input) {
      case media::mojom::SVCInterLayerPredMode::kOff:
        *output = media::SVCInterLayerPredMode::kOff;
        return true;
      case media::mojom::SVCInterLayerPredMode::kOn:
        *output = media::SVCInterLayerPredMode::kOn;
        return true;
      case media::mojom::SVCInterLayerPredMode::kOnKeyPic:
        *output = media::SVCInterLayerPredMode::kOnKeyPic;
        return true;
    }
    NOTREACHED();
  }
};

template <>
struct EnumTraits<media::mojom::VideoRotation, ::media::VideoRotation> {
  static media::mojom::VideoRotation ToMojom(::media::VideoRotation input) {
    switch (input) {
      case ::media::VideoRotation::VIDEO_ROTATION_0:
        return media::mojom::VideoRotation::kVideoRotation0;
      case ::media::VideoRotation::VIDEO_ROTATION_90:
        return media::mojom::VideoRotation::kVideoRotation90;
      case ::media::VideoRotation::VIDEO_ROTATION_180:
        return media::mojom::VideoRotation::kVideoRotation180;
      case ::media::VideoRotation::VIDEO_ROTATION_270:
        return media::mojom::VideoRotation::kVideoRotation270;
    }

    NOTREACHED();
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static bool FromMojom(media::mojom::VideoRotation input,
                        media::VideoRotation* output) {
    switch (input) {
      case media::mojom::VideoRotation::kVideoRotation0:
        *output = ::media::VideoRotation::VIDEO_ROTATION_0;
        return true;
      case media::mojom::VideoRotation::kVideoRotation90:
        *output = ::media::VideoRotation::VIDEO_ROTATION_90;
        return true;
      case media::mojom::VideoRotation::kVideoRotation180:
        *output = ::media::VideoRotation::VIDEO_ROTATION_180;
        return true;
      case media::mojom::VideoRotation::kVideoRotation270:
        *output = ::media::VideoRotation::VIDEO_ROTATION_270;
        return true;
    }

    NOTREACHED();
  }
};

template <>
struct EnumTraits<media::mojom::RendererType, ::media::RendererType> {
  static media::mojom::RendererType ToMojom(::media::RendererType input) {
    switch (input) {
      case ::media::RendererType::kRendererImpl:
        return media::mojom::RendererType::kRendererImpl;
      case ::media::RendererType::kMojo:
        return media::mojom::RendererType::kMojo;
      case ::media::RendererType::kMediaPlayer:
        return media::mojom::RendererType::kMediaPlayer;
      case ::media::RendererType::kCourier:
        return media::mojom::RendererType::kCourier;
      case ::media::RendererType::kFlinging:
        return media::mojom::RendererType::kFlinging;
      case ::media::RendererType::kCast:
        return media::mojom::RendererType::kCast;
      case ::media::RendererType::kMediaFoundation:
        return media::mojom::RendererType::kMediaFoundation;
      case ::media::RendererType::kRemoting:
        return media::mojom::RendererType::kRemoting;
      case ::media::RendererType::kCastStreaming:
        return media::mojom::RendererType::kCastStreaming;
      case ::media::RendererType::kContentEmbedderDefined:
        return media::mojom::RendererType::kContentEmbedderDefined;
      case ::media::RendererType::kTest:
        return media::mojom::RendererType::kTest;
    }

    NOTREACHED();
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static bool FromMojom(media::mojom::RendererType input,
                        ::media::RendererType* output) {
    switch (input) {
      case media::mojom::RendererType::kRendererImpl:
        *output = ::media::RendererType::kRendererImpl;
        return true;
      case media::mojom::RendererType::kMojo:
        *output = ::media::RendererType::kMojo;
        return true;
      case media::mojom::RendererType::kMediaPlayer:
        *output = ::media::RendererType::kMediaPlayer;
        return true;
      case media::mojom::RendererType::kCourier:
        *output = ::media::RendererType::kCourier;
        return true;
      case media::mojom::RendererType::kFlinging:
        *output = ::media::RendererType::kFlinging;
        return true;
      case media::mojom::RendererType::kCast:
        *output = ::media::RendererType::kCast;
        return true;
      case media::mojom::RendererType::kMediaFoundation:
        *output = ::media::RendererType::kMediaFoundation;
        return true;
      case media::mojom::RendererType::kRemoting:
        *output = ::media::RendererType::kRemoting;
        return true;
      case media::mojom::RendererType::kCastStreaming:
        *output = ::media::RendererType::kCastStreaming;
        return true;
      case media::mojom::RendererType::kContentEmbedderDefined:
        *output = ::media::RendererType::kContentEmbedderDefined;
        return true;
      case media::mojom::RendererType::kTest:
        *output = ::media::RendererType::kTest;
        return true;
    }

    NOTREACHED();
  }
};

template <>
struct EnumTraits<media::mojom::CreateCdmStatus, media::CreateCdmStatus> {
  static media::mojom::CreateCdmStatus ToMojom(media::CreateCdmStatus input) {
    switch (input) {
      case media::CreateCdmStatus::kSuccess:
        return media::mojom::CreateCdmStatus::kSuccess;
      case media::CreateCdmStatus::kUnknownError:
        return media::mojom::CreateCdmStatus::kUnknownError;
      case media::CreateCdmStatus::kCdmCreationAborted:
        return media::mojom::CreateCdmStatus::kCdmCreationAborted;
      case media::CreateCdmStatus::kLoadCdmFailed:
        return media::mojom::CreateCdmStatus::kLoadCdmFailed;
      case media::CreateCdmStatus::kCreateCdmFuncNotAvailable:
        return media::mojom::CreateCdmStatus::kCreateCdmFuncNotAvailable;
      case media::CreateCdmStatus::kCdmHelperCreationFailed:
        return media::mojom::CreateCdmStatus::kCdmHelperCreationFailed;
      case media::CreateCdmStatus::kGetCdmPrefDataFailed:
        return media::mojom::CreateCdmStatus::kGetCdmPrefDataFailed;
      case media::CreateCdmStatus::kGetCdmOriginIdFailed:
        return media::mojom::CreateCdmStatus::kGetCdmOriginIdFailed;
      case media::CreateCdmStatus::kInitCdmFailed:
        return media::mojom::CreateCdmStatus::kInitCdmFailed;
      case media::CreateCdmStatus::kCdmFactoryCreationFailed:
        return media::mojom::CreateCdmStatus::kCdmFactoryCreationFailed;
      case media::CreateCdmStatus::kCdmNotSupported:
        return media::mojom::CreateCdmStatus::kCdmNotSupported;
      case media::CreateCdmStatus::kInvalidCdmConfig:
        return media::mojom::CreateCdmStatus::kInvalidCdmConfig;
      case media::CreateCdmStatus::kUnsupportedKeySystem:
        return media::mojom::CreateCdmStatus::kUnsupportedKeySystem;
      case media::CreateCdmStatus::kDisconnectionError:
        return media::mojom::CreateCdmStatus::kDisconnectionError;
      case media::CreateCdmStatus::kNotAllowedOnUniqueOrigin:
        return media::mojom::CreateCdmStatus::kNotAllowedOnUniqueOrigin;
      case media::CreateCdmStatus::kMediaDrmBridgeCreationFailed:
        return media::mojom::CreateCdmStatus::kMediaDrmBridgeCreationFailed;
      case media::CreateCdmStatus::kMediaCryptoNotAvailable:
        return media::mojom::CreateCdmStatus::kMediaCryptoNotAvailable;
      case media::CreateCdmStatus::kNoMoreInstances:
        return media::mojom::CreateCdmStatus::kNoMoreInstances;
      case media::CreateCdmStatus::kInsufficientGpuResources:
        return media::mojom::CreateCdmStatus::kInsufficientGpuResources;
      case media::CreateCdmStatus::kCrOsVerifiedAccessDisabled:
        return media::mojom::CreateCdmStatus::kCrOsVerifiedAccessDisabled;
      case media::CreateCdmStatus::kCrOsRemoteFactoryCreationFailed:
        return media::mojom::CreateCdmStatus::kCrOsRemoteFactoryCreationFailed;
      case media::CreateCdmStatus::kAndroidMediaDrmIllegalArgument:
        return media::mojom::CreateCdmStatus::kAndroidMediaDrmIllegalArgument;
      case media::CreateCdmStatus::kAndroidMediaDrmIllegalState:
        return media::mojom::CreateCdmStatus::kAndroidMediaDrmIllegalState;
      case media::CreateCdmStatus::kAndroidFailedL1SecurityLevel:
        return media::mojom::CreateCdmStatus::kAndroidFailedL1SecurityLevel;
      case media::CreateCdmStatus::kAndroidFailedL3SecurityLevel:
        return media::mojom::CreateCdmStatus::kAndroidFailedL3SecurityLevel;
      case media::CreateCdmStatus::kAndroidFailedSecurityOrigin:
        return media::mojom::CreateCdmStatus::kAndroidFailedSecurityOrigin;
      case media::CreateCdmStatus::kAndroidFailedMediaCryptoSession:
        return media::mojom::CreateCdmStatus::kAndroidFailedMediaCryptoSession;
      case media::CreateCdmStatus::kAndroidFailedToStartProvisioning:
        return media::mojom::CreateCdmStatus::kAndroidFailedToStartProvisioning;
      case media::CreateCdmStatus::kAndroidFailedMediaCryptoCreate:
        return media::mojom::CreateCdmStatus::kAndroidFailedMediaCryptoCreate;
      case media::CreateCdmStatus::kAndroidUnsupportedMediaCryptoScheme:
        return media::mojom::CreateCdmStatus::
            kAndroidUnsupportedMediaCryptoScheme;
    }

    NOTREACHED();
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static bool FromMojom(media::mojom::CreateCdmStatus input,
                        media::CreateCdmStatus* output) {
    switch (input) {
      case media::mojom::CreateCdmStatus::kSuccess:
        *output = media::CreateCdmStatus::kSuccess;
        return true;
      case media::mojom::CreateCdmStatus::kUnknownError:
        *output = media::CreateCdmStatus::kUnknownError;
        return true;
      case media::mojom::CreateCdmStatus::kCdmCreationAborted:
        *output = media::CreateCdmStatus::kCdmCreationAborted;
        return true;
      case media::mojom::CreateCdmStatus::kLoadCdmFailed:
        *output = media::CreateCdmStatus::kLoadCdmFailed;
        return true;
      case media::mojom::CreateCdmStatus::kCreateCdmFuncNotAvailable:
        *output = media::CreateCdmStatus::kCreateCdmFuncNotAvailable;
        return true;
      case media::mojom::CreateCdmStatus::kCdmHelperCreationFailed:
        *output = media::CreateCdmStatus::kCdmHelperCreationFailed;
        return true;
      case media::mojom::CreateCdmStatus::kGetCdmPrefDataFailed:
        *output = media::CreateCdmStatus::kGetCdmPrefDataFailed;
        return true;
      case media::mojom::CreateCdmStatus::kGetCdmOriginIdFailed:
        *output = media::CreateCdmStatus::kGetCdmOriginIdFailed;
        return true;
      case media::mojom::CreateCdmStatus::kInitCdmFailed:
        *output = media::CreateCdmStatus::kInitCdmFailed;
        return true;
      case media::mojom::CreateCdmStatus::kCdmFactoryCreationFailed:
        *output = media::CreateCdmStatus::kCdmFactoryCreationFailed;
        return true;
      case media::mojom::CreateCdmStatus::kCdmNotSupported:
        *output = media::CreateCdmStatus::kCdmNotSupported;
        return true;
      case media::mojom::CreateCdmStatus::kInvalidCdmConfig:
        *output = media::CreateCdmStatus::kInvalidCdmConfig;
        return true;
      case media::mojom::CreateCdmStatus::kUnsupportedKeySystem:
        *output = media::CreateCdmStatus::kUnsupportedKeySystem;
        return true;
      case media::mojom::CreateCdmStatus::kDisconnectionError:
        *output = media::CreateCdmStatus::kDisconnectionError;
        return true;
      case media::mojom::CreateCdmStatus::kNotAllowedOnUniqueOrigin:
        *output = media::CreateCdmStatus::kNotAllowedOnUniqueOrigin;
        return true;
      case media::mojom::CreateCdmStatus::kMediaDrmBridgeCreationFailed:
        *output = media::CreateCdmStatus::kMediaDrmBridgeCreationFailed;
        return true;
      case media::mojom::CreateCdmStatus::kMediaCryptoNotAvailable:
        *output = media::CreateCdmStatus::kMediaCryptoNotAvailable;
        return true;
      case media::mojom::CreateCdmStatus::kNoMoreInstances:
        *output = media::CreateCdmStatus::kNoMoreInstances;
        return true;
      case media::mojom::CreateCdmStatus::kInsufficientGpuResources:
        *output = media::CreateCdmStatus::kInsufficientGpuResources;
        return true;
      case media::mojom::CreateCdmStatus::kCrOsVerifiedAccessDisabled:
        *output = media::CreateCdmStatus::kCrOsVerifiedAccessDisabled;
        return true;
      case media::mojom::CreateCdmStatus::kCrOsRemoteFactoryCreationFailed:
        *output = media::CreateCdmStatus::kCrOsRemoteFactoryCreationFailed;
        return true;
      case media::mojom::CreateCdmStatus::kAndroidMediaDrmIllegalArgument:
        *output = media::CreateCdmStatus::kAndroidMediaDrmIllegalArgument;
        return true;
      case media::mojom::CreateCdmStatus::kAndroidMediaDrmIllegalState:
        *output = media::CreateCdmStatus::kAndroidMediaDrmIllegalState;
        return true;
      case media::mojom::CreateCdmStatus::kAndroidFailedL1SecurityLevel:
        *output = media::CreateCdmStatus::kAndroidFailedL1SecurityLevel;
        return true;
      case media::mojom::CreateCdmStatus::kAndroidFailedL3SecurityLevel:
        *output = media::CreateCdmStatus::kAndroidFailedL3SecurityLevel;
        return true;
      case media::mojom::CreateCdmStatus::kAndroidFailedSecurityOrigin:
        *output = media::CreateCdmStatus::kAndroidFailedSecurityOrigin;
        return true;
      case media::mojom::CreateCdmStatus::kAndroidFailedMediaCryptoSession:
        *output = media::CreateCdmStatus::kAndroidFailedMediaCryptoSession;
        return true;
      case media::mojom::CreateCdmStatus::kAndroidFailedToStartProvisioning:
        *output = media::CreateCdmStatus::kAndroidFailedToStartProvisioning;
        return true;
      case media::mojom::CreateCdmStatus::kAndroidFailedMediaCryptoCreate:
        *output = media::CreateCdmStatus::kAndroidFailedMediaCryptoCreate;
        return true;
      case media::mojom::CreateCdmStatus::kAndroidUnsupportedMediaCryptoScheme:
        *output = media::CreateCdmStatus::kAndroidUnsupportedMediaCryptoScheme;
        return true;
    }

    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_MEDIA_TYPES_ENUM_MOJOM_TRAITS_H_
