// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_MEDIA_TYPES_ENUM_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_MEDIA_TYPES_ENUM_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "build/build_config.h"
#include "media/base/audio_codecs.h"
#include "media/base/cdm_factory.h"
#include "media/base/channel_layout.h"
#include "media/base/demuxer.h"
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
  static ::media::CdmEvent FromMojom(media::mojom::CdmEvent input) {
    switch (input) {
      case media::mojom::CdmEvent::kSignificantPlayback:
        return ::media::CdmEvent::kSignificantPlayback;
      case media::mojom::CdmEvent::kPlaybackError:
        return ::media::CdmEvent::kPlaybackError;
      case media::mojom::CdmEvent::kCdmError:
        return ::media::CdmEvent::kCdmError;
      case media::mojom::CdmEvent::kHardwareContextReset:
        return ::media::CdmEvent::kHardwareContextReset;
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
  static ::media::CdmSessionClosedReason FromMojom(
      media::mojom::CdmSessionClosedReason input) {
    switch (input) {
      case media::mojom::CdmSessionClosedReason::kInternalError:
        return ::media::CdmSessionClosedReason::kInternalError;
      case media::mojom::CdmSessionClosedReason::kClose:
        return ::media::CdmSessionClosedReason::kClose;
      case media::mojom::CdmSessionClosedReason::kReleaseAcknowledged:
        return ::media::CdmSessionClosedReason::kReleaseAcknowledged;
      case media::mojom::CdmSessionClosedReason::kHardwareContextReset:
        return ::media::CdmSessionClosedReason::kHardwareContextReset;
      case media::mojom::CdmSessionClosedReason::kResourceEvicted:
        return ::media::CdmSessionClosedReason::kResourceEvicted;
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
  static ::media::EncryptionType FromMojom(media::mojom::EncryptionType input) {
    switch (input) {
      case media::mojom::EncryptionType::kNone:
        return ::media::EncryptionType::kNone;
      case media::mojom::EncryptionType::kClear:
        return ::media::EncryptionType::kClear;
      case media::mojom::EncryptionType::kEncrypted:
        return ::media::EncryptionType::kEncrypted;
      case media::mojom::EncryptionType::kEncryptedWithClearLead:
        return ::media::EncryptionType::kEncryptedWithClearLead;
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
      case media::SVCScalabilityMode::kL2T1h:
        return media::mojom::SVCScalabilityMode::kL2T1h;
      case media::SVCScalabilityMode::kL2T2h:
        return media::mojom::SVCScalabilityMode::kL2T2h;
      case media::SVCScalabilityMode::kL2T3h:
        return media::mojom::SVCScalabilityMode::kL2T3h;
      case media::SVCScalabilityMode::kS2T1:
        return media::mojom::SVCScalabilityMode::kS2T1;
      case media::SVCScalabilityMode::kS2T2:
        return media::mojom::SVCScalabilityMode::kS2T2;
      case media::SVCScalabilityMode::kS2T3:
        return media::mojom::SVCScalabilityMode::kS2T3;
      case media::SVCScalabilityMode::kS2T1h:
        return media::mojom::SVCScalabilityMode::kS2T1h;
      case media::SVCScalabilityMode::kS2T2h:
        return media::mojom::SVCScalabilityMode::kS2T2h;
      case media::SVCScalabilityMode::kS2T3h:
        return media::mojom::SVCScalabilityMode::kS2T3h;
      case media::SVCScalabilityMode::kS3T1:
        return media::mojom::SVCScalabilityMode::kS3T1;
      case media::SVCScalabilityMode::kS3T2:
        return media::mojom::SVCScalabilityMode::kS3T2;
      case media::SVCScalabilityMode::kS3T3:
        return media::mojom::SVCScalabilityMode::kS3T3;
      case media::SVCScalabilityMode::kS3T1h:
        return media::mojom::SVCScalabilityMode::kS3T1h;
      case media::SVCScalabilityMode::kS3T2h:
        return media::mojom::SVCScalabilityMode::kS3T2h;
      case media::SVCScalabilityMode::kS3T3h:
        return media::mojom::SVCScalabilityMode::kS3T3h;
      case media::SVCScalabilityMode::kL2T1Key:
        return media::mojom::SVCScalabilityMode::kL2T1Key;
      case media::SVCScalabilityMode::kL2T2Key:
        return media::mojom::SVCScalabilityMode::kL2T2Key;
      case media::SVCScalabilityMode::kL2T2KeyShift:
        return media::mojom::SVCScalabilityMode::kL2T2KeyShift;
      case media::SVCScalabilityMode::kL2T3Key:
        return media::mojom::SVCScalabilityMode::kL2T3Key;
      case media::SVCScalabilityMode::kL2T3KeyShift:
        return media::mojom::SVCScalabilityMode::kL2T3KeyShift;
      case media::SVCScalabilityMode::kL3T1Key:
        return media::mojom::SVCScalabilityMode::kL3T1Key;
      case media::SVCScalabilityMode::kL3T2Key:
        return media::mojom::SVCScalabilityMode::kL3T2Key;
      case media::SVCScalabilityMode::kL3T2KeyShift:
        return media::mojom::SVCScalabilityMode::kL3T2KeyShift;
      case media::SVCScalabilityMode::kL3T3Key:
        return media::mojom::SVCScalabilityMode::kL3T3Key;
      case media::SVCScalabilityMode::kL3T3KeyShift:
        return media::mojom::SVCScalabilityMode::kL3T3KeyShift;
      case media::SVCScalabilityMode::kL3T1h:
        return media::mojom::SVCScalabilityMode::kL3T1h;
      case media::SVCScalabilityMode::kL3T2h:
        return media::mojom::SVCScalabilityMode::kL3T2h;
      case media::SVCScalabilityMode::kL3T3h:
        return media::mojom::SVCScalabilityMode::kL3T3h;
    }
    NOTREACHED();
  }

  static media::SVCScalabilityMode FromMojom(
      media::mojom::SVCScalabilityMode input) {
    switch (input) {
      case media::mojom::SVCScalabilityMode::kL1T1:
        return media::SVCScalabilityMode::kL1T1;
      case media::mojom::SVCScalabilityMode::kL1T2:
        return media::SVCScalabilityMode::kL1T2;
      case media::mojom::SVCScalabilityMode::kL1T3:
        return media::SVCScalabilityMode::kL1T3;
      case media::mojom::SVCScalabilityMode::kL2T1:
        return media::SVCScalabilityMode::kL2T1;
      case media::mojom::SVCScalabilityMode::kL2T2:
        return media::SVCScalabilityMode::kL2T2;
      case media::mojom::SVCScalabilityMode::kL2T3:
        return media::SVCScalabilityMode::kL2T3;
      case media::mojom::SVCScalabilityMode::kL3T1:
        return media::SVCScalabilityMode::kL3T1;
      case media::mojom::SVCScalabilityMode::kL3T2:
        return media::SVCScalabilityMode::kL3T2;
      case media::mojom::SVCScalabilityMode::kL3T3:
        return media::SVCScalabilityMode::kL3T3;
      case media::mojom::SVCScalabilityMode::kL2T1h:
        return media::SVCScalabilityMode::kL2T1h;
      case media::mojom::SVCScalabilityMode::kL2T2h:
        return media::SVCScalabilityMode::kL2T2h;
      case media::mojom::SVCScalabilityMode::kL2T3h:
        return media::SVCScalabilityMode::kL2T3h;
      case media::mojom::SVCScalabilityMode::kS2T1:
        return media::SVCScalabilityMode::kS2T1;
      case media::mojom::SVCScalabilityMode::kS2T2:
        return media::SVCScalabilityMode::kS2T2;
      case media::mojom::SVCScalabilityMode::kS2T3:
        return media::SVCScalabilityMode::kS2T3;
      case media::mojom::SVCScalabilityMode::kS2T1h:
        return media::SVCScalabilityMode::kS2T1h;
      case media::mojom::SVCScalabilityMode::kS2T2h:
        return media::SVCScalabilityMode::kS2T2h;
      case media::mojom::SVCScalabilityMode::kS2T3h:
        return media::SVCScalabilityMode::kS2T3h;
      case media::mojom::SVCScalabilityMode::kS3T1:
        return media::SVCScalabilityMode::kS3T1;
      case media::mojom::SVCScalabilityMode::kS3T2:
        return media::SVCScalabilityMode::kS3T2;
      case media::mojom::SVCScalabilityMode::kS3T3:
        return media::SVCScalabilityMode::kS3T3;
      case media::mojom::SVCScalabilityMode::kS3T1h:
        return media::SVCScalabilityMode::kS3T1h;
      case media::mojom::SVCScalabilityMode::kS3T2h:
        return media::SVCScalabilityMode::kS3T2h;
      case media::mojom::SVCScalabilityMode::kS3T3h:
        return media::SVCScalabilityMode::kS3T3h;
      case media::mojom::SVCScalabilityMode::kL2T1Key:
        return media::SVCScalabilityMode::kL2T1Key;
      case media::mojom::SVCScalabilityMode::kL2T2Key:
        return media::SVCScalabilityMode::kL2T2Key;
      case media::mojom::SVCScalabilityMode::kL2T2KeyShift:
        return media::SVCScalabilityMode::kL2T2KeyShift;
      case media::mojom::SVCScalabilityMode::kL2T3Key:
        return media::SVCScalabilityMode::kL2T3Key;
      case media::mojom::SVCScalabilityMode::kL2T3KeyShift:
        return media::SVCScalabilityMode::kL2T3KeyShift;
      case media::mojom::SVCScalabilityMode::kL3T1Key:
        return media::SVCScalabilityMode::kL3T1Key;
      case media::mojom::SVCScalabilityMode::kL3T2Key:
        return media::SVCScalabilityMode::kL3T2Key;
      case media::mojom::SVCScalabilityMode::kL3T2KeyShift:
        return media::SVCScalabilityMode::kL3T2KeyShift;
      case media::mojom::SVCScalabilityMode::kL3T3Key:
        return media::SVCScalabilityMode::kL3T3Key;
      case media::mojom::SVCScalabilityMode::kL3T3KeyShift:
        return media::SVCScalabilityMode::kL3T3KeyShift;
      case media::mojom::SVCScalabilityMode::kL3T1h:
        return media::SVCScalabilityMode::kL3T1h;
      case media::mojom::SVCScalabilityMode::kL3T2h:
        return media::SVCScalabilityMode::kL3T2h;
      case media::mojom::SVCScalabilityMode::kL3T3h:
        return media::SVCScalabilityMode::kL3T3h;
    }
    NOTREACHED();
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

  static media::SVCInterLayerPredMode FromMojom(
      media::mojom::SVCInterLayerPredMode input) {
    switch (input) {
      case media::mojom::SVCInterLayerPredMode::kOff:
        return media::SVCInterLayerPredMode::kOff;
      case media::mojom::SVCInterLayerPredMode::kOn:
        return media::SVCInterLayerPredMode::kOn;
      case media::mojom::SVCInterLayerPredMode::kOnKeyPic:
        return media::SVCInterLayerPredMode::kOnKeyPic;
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
  static media::VideoRotation FromMojom(media::mojom::VideoRotation input) {
    switch (input) {
      case media::mojom::VideoRotation::kVideoRotation0:
        return ::media::VideoRotation::VIDEO_ROTATION_0;
      case media::mojom::VideoRotation::kVideoRotation90:
        return ::media::VideoRotation::VIDEO_ROTATION_90;
      case media::mojom::VideoRotation::kVideoRotation180:
        return ::media::VideoRotation::VIDEO_ROTATION_180;
      case media::mojom::VideoRotation::kVideoRotation270:
        return ::media::VideoRotation::VIDEO_ROTATION_270;
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
  static ::media::RendererType FromMojom(media::mojom::RendererType input) {
    switch (input) {
      case media::mojom::RendererType::kRendererImpl:
        return ::media::RendererType::kRendererImpl;
      case media::mojom::RendererType::kMojo:
        return ::media::RendererType::kMojo;
      case media::mojom::RendererType::kCourier:
        return ::media::RendererType::kCourier;
      case media::mojom::RendererType::kFlinging:
        return ::media::RendererType::kFlinging;
      case media::mojom::RendererType::kCast:
        return ::media::RendererType::kCast;
      case media::mojom::RendererType::kMediaFoundation:
        return ::media::RendererType::kMediaFoundation;
      case media::mojom::RendererType::kRemoting:
        return ::media::RendererType::kRemoting;
      case media::mojom::RendererType::kCastStreaming:
        return ::media::RendererType::kCastStreaming;
      case media::mojom::RendererType::kContentEmbedderDefined:
        return ::media::RendererType::kContentEmbedderDefined;
      case media::mojom::RendererType::kTest:
        return ::media::RendererType::kTest;
    }

    NOTREACHED();
  }
};

template <>
struct EnumTraits<media::mojom::DemuxerType, ::media::DemuxerType> {
  static media::mojom::DemuxerType ToMojom(::media::DemuxerType input) {
    switch (input) {
      case ::media::DemuxerType::kUnknownDemuxer:
        return media::mojom::DemuxerType::kUnknownDemuxer;
      case ::media::DemuxerType::kMockDemuxer:
        return media::mojom::DemuxerType::kMockDemuxer;
      case ::media::DemuxerType::kFFmpegDemuxer:
        return media::mojom::DemuxerType::kFFmpegDemuxer;
      case ::media::DemuxerType::kChunkDemuxer:
        return media::mojom::DemuxerType::kChunkDemuxer;
      case ::media::DemuxerType::kFrameInjectingDemuxer:
        return media::mojom::DemuxerType::kFrameInjectingDemuxer;
      case ::media::DemuxerType::kStreamProviderDemuxer:
        return media::mojom::DemuxerType::kStreamProviderDemuxer;
      case ::media::DemuxerType::kManifestDemuxer:
        return media::mojom::DemuxerType::kManifestDemuxer;
    }

    NOTREACHED();
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static ::media::DemuxerType FromMojom(media::mojom::DemuxerType input) {
    switch (input) {
      case media::mojom::DemuxerType::kUnknownDemuxer:
        return ::media::DemuxerType::kUnknownDemuxer;
      case media::mojom::DemuxerType::kMockDemuxer:
        return ::media::DemuxerType::kMockDemuxer;
      case media::mojom::DemuxerType::kFFmpegDemuxer:
        return ::media::DemuxerType::kFFmpegDemuxer;
      case media::mojom::DemuxerType::kChunkDemuxer:
        return ::media::DemuxerType::kChunkDemuxer;
      case media::mojom::DemuxerType::kFrameInjectingDemuxer:
        return ::media::DemuxerType::kFrameInjectingDemuxer;
      case media::mojom::DemuxerType::kStreamProviderDemuxer:
        return ::media::DemuxerType::kStreamProviderDemuxer;
      case media::mojom::DemuxerType::kManifestDemuxer:
        return ::media::DemuxerType::kManifestDemuxer;
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
  static media::CreateCdmStatus FromMojom(media::mojom::CreateCdmStatus input) {
    switch (input) {
      case media::mojom::CreateCdmStatus::kSuccess:
        return media::CreateCdmStatus::kSuccess;
      case media::mojom::CreateCdmStatus::kUnknownError:
        return media::CreateCdmStatus::kUnknownError;
      case media::mojom::CreateCdmStatus::kCdmCreationAborted:
        return media::CreateCdmStatus::kCdmCreationAborted;
      case media::mojom::CreateCdmStatus::kCreateCdmFuncNotAvailable:
        return media::CreateCdmStatus::kCreateCdmFuncNotAvailable;
      case media::mojom::CreateCdmStatus::kCdmHelperCreationFailed:
        return media::CreateCdmStatus::kCdmHelperCreationFailed;
      case media::mojom::CreateCdmStatus::kGetCdmPrefDataFailed:
        return media::CreateCdmStatus::kGetCdmPrefDataFailed;
      case media::mojom::CreateCdmStatus::kGetCdmOriginIdFailed:
        return media::CreateCdmStatus::kGetCdmOriginIdFailed;
      case media::mojom::CreateCdmStatus::kInitCdmFailed:
        return media::CreateCdmStatus::kInitCdmFailed;
      case media::mojom::CreateCdmStatus::kCdmFactoryCreationFailed:
        return media::CreateCdmStatus::kCdmFactoryCreationFailed;
      case media::mojom::CreateCdmStatus::kCdmNotSupported:
        return media::CreateCdmStatus::kCdmNotSupported;
      case media::mojom::CreateCdmStatus::kInvalidCdmConfig:
        return media::CreateCdmStatus::kInvalidCdmConfig;
      case media::mojom::CreateCdmStatus::kUnsupportedKeySystem:
        return media::CreateCdmStatus::kUnsupportedKeySystem;
      case media::mojom::CreateCdmStatus::kDisconnectionError:
        return media::CreateCdmStatus::kDisconnectionError;
      case media::mojom::CreateCdmStatus::kNotAllowedOnUniqueOrigin:
        return media::CreateCdmStatus::kNotAllowedOnUniqueOrigin;
      case media::mojom::CreateCdmStatus::kMediaCryptoNotAvailable:
        return media::CreateCdmStatus::kMediaCryptoNotAvailable;
      case media::mojom::CreateCdmStatus::kNoMoreInstances:
        return media::CreateCdmStatus::kNoMoreInstances;
      case media::mojom::CreateCdmStatus::kInsufficientGpuResources:
        return media::CreateCdmStatus::kInsufficientGpuResources;
      case media::mojom::CreateCdmStatus::kCrOsVerifiedAccessDisabled:
        return media::CreateCdmStatus::kCrOsVerifiedAccessDisabled;
      case media::mojom::CreateCdmStatus::kCrOsRemoteFactoryCreationFailed:
        return media::CreateCdmStatus::kCrOsRemoteFactoryCreationFailed;
      case media::mojom::CreateCdmStatus::kAndroidMediaDrmIllegalArgument:
        return media::CreateCdmStatus::kAndroidMediaDrmIllegalArgument;
      case media::mojom::CreateCdmStatus::kAndroidMediaDrmIllegalState:
        return media::CreateCdmStatus::kAndroidMediaDrmIllegalState;
      case media::mojom::CreateCdmStatus::kAndroidFailedL1SecurityLevel:
        return media::CreateCdmStatus::kAndroidFailedL1SecurityLevel;
      case media::mojom::CreateCdmStatus::kAndroidFailedL3SecurityLevel:
        return media::CreateCdmStatus::kAndroidFailedL3SecurityLevel;
      case media::mojom::CreateCdmStatus::kAndroidFailedSecurityOrigin:
        return media::CreateCdmStatus::kAndroidFailedSecurityOrigin;
      case media::mojom::CreateCdmStatus::kAndroidFailedMediaCryptoSession:
        return media::CreateCdmStatus::kAndroidFailedMediaCryptoSession;
      case media::mojom::CreateCdmStatus::kAndroidFailedToStartProvisioning:
        return media::CreateCdmStatus::kAndroidFailedToStartProvisioning;
      case media::mojom::CreateCdmStatus::kAndroidFailedMediaCryptoCreate:
        return media::CreateCdmStatus::kAndroidFailedMediaCryptoCreate;
      case media::mojom::CreateCdmStatus::kAndroidUnsupportedMediaCryptoScheme:
        return media::CreateCdmStatus::kAndroidUnsupportedMediaCryptoScheme;
    }

    NOTREACHED();
  }
};

template <>
struct EnumTraits<media::mojom::ChannelLayout, ::media::ChannelLayout> {
  static media::mojom::ChannelLayout ToMojom(::media::ChannelLayout input) {
    switch (input) {
      case ::media::CHANNEL_LAYOUT_NONE:
        return media::mojom::ChannelLayout::kNone;
      case ::media::CHANNEL_LAYOUT_UNSUPPORTED:
        return media::mojom::ChannelLayout::kUnsupported;
      case ::media::CHANNEL_LAYOUT_MONO:
        return media::mojom::ChannelLayout::kMono;
      case ::media::CHANNEL_LAYOUT_STEREO:
        return media::mojom::ChannelLayout::kStereo;
      case ::media::CHANNEL_LAYOUT_2_1:
        return media::mojom::ChannelLayout::k2_1;
      case ::media::CHANNEL_LAYOUT_SURROUND:
        return media::mojom::ChannelLayout::kSurround;
      case ::media::CHANNEL_LAYOUT_4_0:
        return media::mojom::ChannelLayout::k4_0;
      case ::media::CHANNEL_LAYOUT_2_2:
        return media::mojom::ChannelLayout::k2_2;
      case ::media::CHANNEL_LAYOUT_QUAD:
        return media::mojom::ChannelLayout::kQuad;
      case ::media::CHANNEL_LAYOUT_5_0:
        return media::mojom::ChannelLayout::k5_0;
      case ::media::CHANNEL_LAYOUT_5_1:
        return media::mojom::ChannelLayout::k5_1;
      case ::media::CHANNEL_LAYOUT_5_0_BACK:
        return media::mojom::ChannelLayout::k5_0Back;
      case ::media::CHANNEL_LAYOUT_5_1_BACK:
        return media::mojom::ChannelLayout::k5_1Back;
      case ::media::CHANNEL_LAYOUT_7_0:
        return media::mojom::ChannelLayout::k7_0;
      case ::media::CHANNEL_LAYOUT_7_1:
        return media::mojom::ChannelLayout::k7_1;
      case ::media::CHANNEL_LAYOUT_7_1_WIDE:
        return media::mojom::ChannelLayout::k7_1Wide;
      case ::media::CHANNEL_LAYOUT_STEREO_DOWNMIX:
        return media::mojom::ChannelLayout::kStereoDownmix;
      case ::media::CHANNEL_LAYOUT_2POINT1:
        return media::mojom::ChannelLayout::k2Point1;
      case ::media::CHANNEL_LAYOUT_3_1:
        return media::mojom::ChannelLayout::k3_1;
      case ::media::CHANNEL_LAYOUT_4_1:
        return media::mojom::ChannelLayout::k4_1;
      case ::media::CHANNEL_LAYOUT_6_0:
        return media::mojom::ChannelLayout::k6_0;
      case ::media::CHANNEL_LAYOUT_6_0_FRONT:
        return media::mojom::ChannelLayout::k6_0Front;
      case ::media::CHANNEL_LAYOUT_HEXAGONAL:
        return media::mojom::ChannelLayout::kHexagonal;
      case ::media::CHANNEL_LAYOUT_6_1:
        return media::mojom::ChannelLayout::k6_1;
      case ::media::CHANNEL_LAYOUT_6_1_BACK:
        return media::mojom::ChannelLayout::k6_1Back;
      case ::media::CHANNEL_LAYOUT_6_1_FRONT:
        return media::mojom::ChannelLayout::k6_1Front;
      case ::media::CHANNEL_LAYOUT_7_0_FRONT:
        return media::mojom::ChannelLayout::k7_0Front;
      case ::media::CHANNEL_LAYOUT_7_1_WIDE_BACK:
        return media::mojom::ChannelLayout::k7_1WideBack;
      case ::media::CHANNEL_LAYOUT_OCTAGONAL:
        return media::mojom::ChannelLayout::kOctagonal;
      case ::media::CHANNEL_LAYOUT_DISCRETE:
        return media::mojom::ChannelLayout::kDiscrete;
      case ::media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC:
        return media::mojom::ChannelLayout::kStereoAndKeyboardMic;
      case ::media::CHANNEL_LAYOUT_4_1_QUAD_SIDE:
        return media::mojom::ChannelLayout::k4_1QuadSide;
      case ::media::CHANNEL_LAYOUT_BITSTREAM:
        return media::mojom::ChannelLayout::kBitstream;
      case ::media::CHANNEL_LAYOUT_5_1_4_DOWNMIX:
        return media::mojom::ChannelLayout::k5_1_4Downmix;
      case ::media::CHANNEL_LAYOUT_1_1:
        return media::mojom::ChannelLayout::k1_1;
      case ::media::CHANNEL_LAYOUT_3_1_BACK:
        return media::mojom::ChannelLayout::k3_1Back;
      case ::media::CHANNEL_LAYOUT_5_1_4:
        return media::mojom::ChannelLayout::k5_1_4;
      case ::media::CHANNEL_LAYOUT_7_1_4:
        return media::mojom::ChannelLayout::k7_1_4;
    }
    NOTREACHED();
  }

  static ::media::ChannelLayout FromMojom(media::mojom::ChannelLayout input) {
    switch (input) {
      case media::mojom::ChannelLayout::kNone:
        return ::media::CHANNEL_LAYOUT_NONE;
      case media::mojom::ChannelLayout::kUnsupported:
        return ::media::CHANNEL_LAYOUT_UNSUPPORTED;
      case media::mojom::ChannelLayout::kMono:
        return ::media::CHANNEL_LAYOUT_MONO;
      case media::mojom::ChannelLayout::kStereo:
        return ::media::CHANNEL_LAYOUT_STEREO;
      case media::mojom::ChannelLayout::k2_1:
        return ::media::CHANNEL_LAYOUT_2_1;
      case media::mojom::ChannelLayout::kSurround:
        return ::media::CHANNEL_LAYOUT_SURROUND;
      case media::mojom::ChannelLayout::k4_0:
        return ::media::CHANNEL_LAYOUT_4_0;
      case media::mojom::ChannelLayout::k2_2:
        return ::media::CHANNEL_LAYOUT_2_2;
      case media::mojom::ChannelLayout::kQuad:
        return ::media::CHANNEL_LAYOUT_QUAD;
      case media::mojom::ChannelLayout::k5_0:
        return ::media::CHANNEL_LAYOUT_5_0;
      case media::mojom::ChannelLayout::k5_1:
        return ::media::CHANNEL_LAYOUT_5_1;
      case media::mojom::ChannelLayout::k5_0Back:
        return ::media::CHANNEL_LAYOUT_5_0_BACK;
      case media::mojom::ChannelLayout::k5_1Back:
        return ::media::CHANNEL_LAYOUT_5_1_BACK;
      case media::mojom::ChannelLayout::k7_0:
        return ::media::CHANNEL_LAYOUT_7_0;
      case media::mojom::ChannelLayout::k7_1:
        return ::media::CHANNEL_LAYOUT_7_1;
      case media::mojom::ChannelLayout::k7_1Wide:
        return ::media::CHANNEL_LAYOUT_7_1_WIDE;
      case media::mojom::ChannelLayout::kStereoDownmix:
        return ::media::CHANNEL_LAYOUT_STEREO_DOWNMIX;
      case media::mojom::ChannelLayout::k2Point1:
        return ::media::CHANNEL_LAYOUT_2POINT1;
      case media::mojom::ChannelLayout::k3_1:
        return ::media::CHANNEL_LAYOUT_3_1;
      case media::mojom::ChannelLayout::k4_1:
        return ::media::CHANNEL_LAYOUT_4_1;
      case media::mojom::ChannelLayout::k6_0:
        return ::media::CHANNEL_LAYOUT_6_0;
      case media::mojom::ChannelLayout::k6_0Front:
        return ::media::CHANNEL_LAYOUT_6_0_FRONT;
      case media::mojom::ChannelLayout::kHexagonal:
        return ::media::CHANNEL_LAYOUT_HEXAGONAL;
      case media::mojom::ChannelLayout::k6_1:
        return ::media::CHANNEL_LAYOUT_6_1;
      case media::mojom::ChannelLayout::k6_1Back:
        return ::media::CHANNEL_LAYOUT_6_1_BACK;
      case media::mojom::ChannelLayout::k6_1Front:
        return ::media::CHANNEL_LAYOUT_6_1_FRONT;
      case media::mojom::ChannelLayout::k7_0Front:
        return ::media::CHANNEL_LAYOUT_7_0_FRONT;
      case media::mojom::ChannelLayout::k7_1WideBack:
        return ::media::CHANNEL_LAYOUT_7_1_WIDE_BACK;
      case media::mojom::ChannelLayout::kOctagonal:
        return ::media::CHANNEL_LAYOUT_OCTAGONAL;
      case media::mojom::ChannelLayout::kDiscrete:
        return ::media::CHANNEL_LAYOUT_DISCRETE;
      case media::mojom::ChannelLayout::kStereoAndKeyboardMic:
        return ::media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC;
      case media::mojom::ChannelLayout::k4_1QuadSide:
        return ::media::CHANNEL_LAYOUT_4_1_QUAD_SIDE;
      case media::mojom::ChannelLayout::kBitstream:
        return ::media::CHANNEL_LAYOUT_BITSTREAM;
      case media::mojom::ChannelLayout::k5_1_4Downmix:
        return ::media::CHANNEL_LAYOUT_5_1_4_DOWNMIX;
      case media::mojom::ChannelLayout::k1_1:
        return ::media::CHANNEL_LAYOUT_1_1;
      case media::mojom::ChannelLayout::k3_1Back:
        return ::media::CHANNEL_LAYOUT_3_1_BACK;
      case media::mojom::ChannelLayout::k5_1_4:
        return ::media::CHANNEL_LAYOUT_5_1_4;
      case media::mojom::ChannelLayout::k7_1_4:
        return ::media::CHANNEL_LAYOUT_7_1_4;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_MEDIA_TYPES_ENUM_MOJOM_TRAITS_H_
