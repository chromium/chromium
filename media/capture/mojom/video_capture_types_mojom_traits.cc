// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/mojom/video_capture_types_mojom_traits.h"

#include <optional>

#include "base/notreached.h"
#include "media/capture/mojom/video_capture_types.mojom-shared.h"
#include "media/capture/video_capture_types.h"
#include "ui/gfx/geometry/mojom/geometry.mojom.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

// static
media::mojom::CaptureSourceRequestType EnumTraits<
    media::mojom::CaptureSourceRequestType,
    media::CaptureSourceRequestType>::ToMojom(media::CaptureSourceRequestType
                                                  input) {
  switch (input) {
    case media::CaptureSourceRequestType::kUnknown:
      return media::mojom::CaptureSourceRequestType::kUnknown;
    case media::CaptureSourceRequestType::kGetUserMedia:
      return media::mojom::CaptureSourceRequestType::kGetUserMedia;
    case media::CaptureSourceRequestType::kGetDisplayMedia:
      return media::mojom::CaptureSourceRequestType::kGetDisplayMedia;
  }
  NOTREACHED();
}

// static
media::CaptureSourceRequestType
EnumTraits<media::mojom::CaptureSourceRequestType,
           media::CaptureSourceRequestType>::
    FromMojom(media::mojom::CaptureSourceRequestType input) {
  switch (input) {
    case media::mojom::CaptureSourceRequestType::kUnknown:
      return media::CaptureSourceRequestType::kUnknown;
    case media::mojom::CaptureSourceRequestType::kGetUserMedia:
      return media::CaptureSourceRequestType::kGetUserMedia;
    case media::mojom::CaptureSourceRequestType::kGetDisplayMedia:
      return media::CaptureSourceRequestType::kGetDisplayMedia;
  }
  NOTREACHED();
}

// static
media::mojom::ResolutionChangePolicy
EnumTraits<media::mojom::ResolutionChangePolicy,
           media::ResolutionChangePolicy>::ToMojom(media::ResolutionChangePolicy
                                                       input) {
  switch (input) {
    case media::ResolutionChangePolicy::FIXED_RESOLUTION:
      return media::mojom::ResolutionChangePolicy::FIXED_RESOLUTION;
    case media::ResolutionChangePolicy::FIXED_ASPECT_RATIO:
      return media::mojom::ResolutionChangePolicy::FIXED_ASPECT_RATIO;
    case media::ResolutionChangePolicy::ANY_WITHIN_LIMIT:
      return media::mojom::ResolutionChangePolicy::ANY_WITHIN_LIMIT;
  }
  NOTREACHED();
}

// static
media::ResolutionChangePolicy EnumTraits<media::mojom::ResolutionChangePolicy,
                                         media::ResolutionChangePolicy>::
    FromMojom(media::mojom::ResolutionChangePolicy input) {
  switch (input) {
    case media::mojom::ResolutionChangePolicy::FIXED_RESOLUTION:
      return media::ResolutionChangePolicy::FIXED_RESOLUTION;
    case media::mojom::ResolutionChangePolicy::FIXED_ASPECT_RATIO:
      return media::ResolutionChangePolicy::FIXED_ASPECT_RATIO;
    case media::mojom::ResolutionChangePolicy::ANY_WITHIN_LIMIT:
      return media::ResolutionChangePolicy::ANY_WITHIN_LIMIT;
  }
  NOTREACHED();
}

// static
media::mojom::PowerLineFrequency EnumTraits<
    media::mojom::PowerLineFrequency,
    media::PowerLineFrequency>::ToMojom(media::PowerLineFrequency input) {
  switch (input) {
    case media::PowerLineFrequency::kDefault:
      return media::mojom::PowerLineFrequency::DEFAULT;
    case media::PowerLineFrequency::k50Hz:
      return media::mojom::PowerLineFrequency::HZ_50;
    case media::PowerLineFrequency::k60Hz:
      return media::mojom::PowerLineFrequency::HZ_60;
  }
  NOTREACHED();
}

// static
media::PowerLineFrequency
EnumTraits<media::mojom::PowerLineFrequency, media::PowerLineFrequency>::
    FromMojom(media::mojom::PowerLineFrequency input) {
  switch (input) {
    case media::mojom::PowerLineFrequency::DEFAULT:
      return media::PowerLineFrequency::kDefault;
    case media::mojom::PowerLineFrequency::HZ_50:
      return media::PowerLineFrequency::k50Hz;
    case media::mojom::PowerLineFrequency::HZ_60:
      return media::PowerLineFrequency::k60Hz;
  }
  NOTREACHED();
}

// static
media::mojom::VideoCapturePixelFormat
EnumTraits<media::mojom::VideoCapturePixelFormat,
           media::VideoPixelFormat>::ToMojom(media::VideoPixelFormat input) {
  switch (input) {
    case media::VideoPixelFormat::PIXEL_FORMAT_UNKNOWN:
      return media::mojom::VideoCapturePixelFormat::UNKNOWN;
    case media::VideoPixelFormat::PIXEL_FORMAT_I420:
      return media::mojom::VideoCapturePixelFormat::I420;
    case media::VideoPixelFormat::PIXEL_FORMAT_YV12:
      return media::mojom::VideoCapturePixelFormat::YV12;
    case media::VideoPixelFormat::PIXEL_FORMAT_I422:
      return media::mojom::VideoCapturePixelFormat::I422;
    case media::VideoPixelFormat::PIXEL_FORMAT_I420A:
      return media::mojom::VideoCapturePixelFormat::I420A;
    case media::VideoPixelFormat::PIXEL_FORMAT_I444:
      return media::mojom::VideoCapturePixelFormat::I444;
    case media::VideoPixelFormat::PIXEL_FORMAT_NV12:
      return media::mojom::VideoCapturePixelFormat::NV12;
    case media::VideoPixelFormat::PIXEL_FORMAT_NV21:
      return media::mojom::VideoCapturePixelFormat::NV21;
    case media::VideoPixelFormat::PIXEL_FORMAT_UYVY:
      return media::mojom::VideoCapturePixelFormat::UYVY;
    case media::VideoPixelFormat::PIXEL_FORMAT_YUY2:
      return media::mojom::VideoCapturePixelFormat::YUY2;
    case media::VideoPixelFormat::PIXEL_FORMAT_ARGB:
      return media::mojom::VideoCapturePixelFormat::ARGB;
    case media::VideoPixelFormat::PIXEL_FORMAT_BGRA:
      return media::mojom::VideoCapturePixelFormat::BGRA;
    case media::VideoPixelFormat::PIXEL_FORMAT_XRGB:
      return media::mojom::VideoCapturePixelFormat::XRGB;
    case media::VideoPixelFormat::PIXEL_FORMAT_RGB24:
      return media::mojom::VideoCapturePixelFormat::RGB24;
    case media::VideoPixelFormat::PIXEL_FORMAT_MJPEG:
      return media::mojom::VideoCapturePixelFormat::MJPEG;
    case media::VideoPixelFormat::PIXEL_FORMAT_YUV420P10:
      return media::mojom::VideoCapturePixelFormat::YUV420P10;
    case media::VideoPixelFormat::PIXEL_FORMAT_YUV422P10:
      return media::mojom::VideoCapturePixelFormat::YUV422P10;
    case media::VideoPixelFormat::PIXEL_FORMAT_YUV444P10:
      return media::mojom::VideoCapturePixelFormat::YUV444P10;
    case media::VideoPixelFormat::PIXEL_FORMAT_YUV420P12:
      return media::mojom::VideoCapturePixelFormat::YUV420P12;
    case media::VideoPixelFormat::PIXEL_FORMAT_YUV422P12:
      return media::mojom::VideoCapturePixelFormat::YUV422P12;
    case media::VideoPixelFormat::PIXEL_FORMAT_YUV444P12:
      return media::mojom::VideoCapturePixelFormat::YUV444P12;
    case media::VideoPixelFormat::PIXEL_FORMAT_Y16:
      return media::mojom::VideoCapturePixelFormat::Y16;
    case media::VideoPixelFormat::PIXEL_FORMAT_ABGR:
      return media::mojom::VideoCapturePixelFormat::ABGR;
    case media::VideoPixelFormat::PIXEL_FORMAT_XBGR:
      return media::mojom::VideoCapturePixelFormat::XBGR;
    case media::VideoPixelFormat::PIXEL_FORMAT_P010LE:
      return media::mojom::VideoCapturePixelFormat::P010LE;
    case media::VideoPixelFormat::PIXEL_FORMAT_XR30:
      return media::mojom::VideoCapturePixelFormat::XR30;
    case media::VideoPixelFormat::PIXEL_FORMAT_XB30:
      return media::mojom::VideoCapturePixelFormat::XB30;
    case media::VideoPixelFormat::PIXEL_FORMAT_RGBAF16:
      return media::mojom::VideoCapturePixelFormat::RGBAF16;
    case media::VideoPixelFormat::PIXEL_FORMAT_I422A:
      return media::mojom::VideoCapturePixelFormat::I422A;
    case media::VideoPixelFormat::PIXEL_FORMAT_I444A:
      return media::mojom::VideoCapturePixelFormat::I444A;
    case media::VideoPixelFormat::PIXEL_FORMAT_YUV420AP10:
      return media::mojom::VideoCapturePixelFormat::YUV420AP10;
    case media::VideoPixelFormat::PIXEL_FORMAT_YUV422AP10:
      return media::mojom::VideoCapturePixelFormat::YUV422AP10;
    case media::VideoPixelFormat::PIXEL_FORMAT_YUV444AP10:
      return media::mojom::VideoCapturePixelFormat::YUV444AP10;
    case media::VideoPixelFormat::PIXEL_FORMAT_NV12A:
      return media::mojom::VideoCapturePixelFormat::NV12A;
    case media::VideoPixelFormat::PIXEL_FORMAT_NV16:
      return media::mojom::VideoCapturePixelFormat::NV16;
    case media::VideoPixelFormat::PIXEL_FORMAT_NV24:
      return media::mojom::VideoCapturePixelFormat::NV24;
    case media::VideoPixelFormat::PIXEL_FORMAT_P210LE:
      return media::mojom::VideoCapturePixelFormat::P210LE;
    case media::VideoPixelFormat::PIXEL_FORMAT_P410LE:
      return media::mojom::VideoCapturePixelFormat::P410LE;
  }
  NOTREACHED();
}

// static
media::VideoPixelFormat
EnumTraits<media::mojom::VideoCapturePixelFormat, media::VideoPixelFormat>::
    FromMojom(media::mojom::VideoCapturePixelFormat input) {
  switch (input) {
    case media::mojom::VideoCapturePixelFormat::UNKNOWN:
      return media::PIXEL_FORMAT_UNKNOWN;
    case media::mojom::VideoCapturePixelFormat::I420:
      return media::PIXEL_FORMAT_I420;
    case media::mojom::VideoCapturePixelFormat::YV12:
      return media::PIXEL_FORMAT_YV12;
    case media::mojom::VideoCapturePixelFormat::I422:
      return media::PIXEL_FORMAT_I422;
    case media::mojom::VideoCapturePixelFormat::I420A:
      return media::PIXEL_FORMAT_I420A;
    case media::mojom::VideoCapturePixelFormat::I444:
      return media::PIXEL_FORMAT_I444;
    case media::mojom::VideoCapturePixelFormat::NV12:
      return media::PIXEL_FORMAT_NV12;
    case media::mojom::VideoCapturePixelFormat::NV21:
      return media::PIXEL_FORMAT_NV21;
    case media::mojom::VideoCapturePixelFormat::UYVY:
      return media::PIXEL_FORMAT_UYVY;
    case media::mojom::VideoCapturePixelFormat::YUY2:
      return media::PIXEL_FORMAT_YUY2;
    case media::mojom::VideoCapturePixelFormat::ARGB:
      return media::PIXEL_FORMAT_ARGB;
    case media::mojom::VideoCapturePixelFormat::XRGB:
      return media::PIXEL_FORMAT_XRGB;
    case media::mojom::VideoCapturePixelFormat::RGB24:
      return media::PIXEL_FORMAT_RGB24;
    case media::mojom::VideoCapturePixelFormat::MJPEG:
      return media::PIXEL_FORMAT_MJPEG;
    case media::mojom::VideoCapturePixelFormat::YUV420P9_DEPRECATED:
      NOTREACHED();
    case media::mojom::VideoCapturePixelFormat::YUV420P10:
      return media::PIXEL_FORMAT_YUV420P10;
    case media::mojom::VideoCapturePixelFormat::YUV422P9_DEPRECATED:
      NOTREACHED();
    case media::mojom::VideoCapturePixelFormat::YUV422P10:
      return media::PIXEL_FORMAT_YUV422P10;
    case media::mojom::VideoCapturePixelFormat::YUV444P9_DEPRECATED:
      NOTREACHED();
    case media::mojom::VideoCapturePixelFormat::YUV444P10:
      return media::PIXEL_FORMAT_YUV444P10;
    case media::mojom::VideoCapturePixelFormat::YUV420P12:
      return media::PIXEL_FORMAT_YUV420P12;
    case media::mojom::VideoCapturePixelFormat::YUV422P12:
      return media::PIXEL_FORMAT_YUV422P12;
    case media::mojom::VideoCapturePixelFormat::YUV444P12:
      return media::PIXEL_FORMAT_YUV444P12;
    case media::mojom::VideoCapturePixelFormat::Y16:
      return media::PIXEL_FORMAT_Y16;
    case media::mojom::VideoCapturePixelFormat::ABGR:
      return media::PIXEL_FORMAT_ABGR;
    case media::mojom::VideoCapturePixelFormat::XBGR:
      return media::PIXEL_FORMAT_XBGR;
    case media::mojom::VideoCapturePixelFormat::P010LE:
      return media::PIXEL_FORMAT_P010LE;
    case media::mojom::VideoCapturePixelFormat::XR30:
      return media::PIXEL_FORMAT_XR30;
    case media::mojom::VideoCapturePixelFormat::XB30:
      return media::PIXEL_FORMAT_XB30;
    case media::mojom::VideoCapturePixelFormat::BGRA:
      return media::PIXEL_FORMAT_BGRA;
    case media::mojom::VideoCapturePixelFormat::RGBAF16:
      return media::PIXEL_FORMAT_RGBAF16;
    case media::mojom::VideoCapturePixelFormat::I422A:
      return media::PIXEL_FORMAT_I422A;
    case media::mojom::VideoCapturePixelFormat::I444A:
      return media::PIXEL_FORMAT_I444A;
    case media::mojom::VideoCapturePixelFormat::YUV420AP10:
      return media::PIXEL_FORMAT_YUV420AP10;
    case media::mojom::VideoCapturePixelFormat::YUV422AP10:
      return media::PIXEL_FORMAT_YUV422AP10;
    case media::mojom::VideoCapturePixelFormat::YUV444AP10:
      return media::PIXEL_FORMAT_YUV444AP10;
    case media::mojom::VideoCapturePixelFormat::NV12A:
      return media::PIXEL_FORMAT_NV12A;
    case media::mojom::VideoCapturePixelFormat::NV16:
      return media::PIXEL_FORMAT_NV16;
    case media::mojom::VideoCapturePixelFormat::NV24:
      return media::PIXEL_FORMAT_NV16;
    case media::mojom::VideoCapturePixelFormat::P210LE:
      return media::PIXEL_FORMAT_P210LE;
    case media::mojom::VideoCapturePixelFormat::P410LE:
      return media::PIXEL_FORMAT_P410LE;
  }
  NOTREACHED();
}

// static
media::mojom::VideoCaptureBufferType
EnumTraits<media::mojom::VideoCaptureBufferType,
           media::VideoCaptureBufferType>::ToMojom(media::VideoCaptureBufferType
                                                       input) {
  switch (input) {
    case media::VideoCaptureBufferType::kSharedMemory:
      return media::mojom::VideoCaptureBufferType::kSharedMemory;
    case media::VideoCaptureBufferType::kGpuMemoryBuffer:
      return media::mojom::VideoCaptureBufferType::kGpuMemoryBuffer;
    case media::VideoCaptureBufferType::kSharedImage:
      return media::mojom::VideoCaptureBufferType::kSharedImage;
  }
  NOTREACHED();
}

// static
media::VideoCaptureBufferType EnumTraits<media::mojom::VideoCaptureBufferType,
                                         media::VideoCaptureBufferType>::
    FromMojom(media::mojom::VideoCaptureBufferType input) {
  switch (input) {
    case media::mojom::VideoCaptureBufferType::kSharedMemory:
      return media::VideoCaptureBufferType::kSharedMemory;
    case media::mojom::VideoCaptureBufferType::kGpuMemoryBuffer:
      return media::VideoCaptureBufferType::kGpuMemoryBuffer;
    case media::mojom::VideoCaptureBufferType::kSharedImage:
      return media::VideoCaptureBufferType::kSharedImage;
  }
  NOTREACHED();
}

// static
media::mojom::VideoCaptureError
EnumTraits<media::mojom::VideoCaptureError, media::VideoCaptureError>::ToMojom(
    media::VideoCaptureError input) {
  switch (input) {
    case media::VideoCaptureError::kNone:
      return media::mojom::VideoCaptureError::kNone;
    case media::VideoCaptureError::
        kVideoCaptureControllerInvalidOrUnsupportedVideoCaptureParametersRequested:
      return media::mojom::VideoCaptureError::
          kVideoCaptureControllerInvalidOrUnsupportedVideoCaptureParametersRequested;
    case media::VideoCaptureError::kVideoCaptureControllerIsAlreadyInErrorState:
      return media::mojom::VideoCaptureError::
          kVideoCaptureControllerIsAlreadyInErrorState;
    case media::VideoCaptureError::kVideoCaptureManagerDeviceConnectionLost:
      return media::mojom::VideoCaptureError::
          kVideoCaptureManagerDeviceConnectionLost;
    case media::VideoCaptureError::
        kFrameSinkVideoCaptureDeviceAlreadyEndedOnFatalError:
      return media::mojom::VideoCaptureError::
          kFrameSinkVideoCaptureDeviceAlreadyEndedOnFatalError;
    case media::VideoCaptureError::
        kFrameSinkVideoCaptureDeviceEncounteredFatalError:
      return media::mojom::VideoCaptureError::
          kFrameSinkVideoCaptureDeviceEncounteredFatalError;
    case media::VideoCaptureError::kV4L2FailedToOpenV4L2DeviceDriverFile:
      return media::mojom::VideoCaptureError::
          kV4L2FailedToOpenV4L2DeviceDriverFile;
    case media::VideoCaptureError::kV4L2ThisIsNotAV4L2VideoCaptureDevice:
      return media::mojom::VideoCaptureError::
          kV4L2ThisIsNotAV4L2VideoCaptureDevice;
    case media::VideoCaptureError::kV4L2FailedToFindASupportedCameraFormat:
      return media::mojom::VideoCaptureError::
          kV4L2FailedToFindASupportedCameraFormat;
    case media::VideoCaptureError::kV4L2FailedToSetVideoCaptureFormat:
      return media::mojom::VideoCaptureError::
          kV4L2FailedToSetVideoCaptureFormat;
    case media::VideoCaptureError::kV4L2UnsupportedPixelFormat:
      return media::mojom::VideoCaptureError::kV4L2UnsupportedPixelFormat;
    case media::VideoCaptureError::kV4L2FailedToSetCameraFramerate:
      return media::mojom::VideoCaptureError::kV4L2FailedToSetCameraFramerate;
    case media::VideoCaptureError::kV4L2ErrorRequestingMmapBuffers:
      return media::mojom::VideoCaptureError::kV4L2ErrorRequestingMmapBuffers;
    case media::VideoCaptureError::kV4L2AllocateBufferFailed:
      return media::mojom::VideoCaptureError::kV4L2AllocateBufferFailed;
    case media::VideoCaptureError::kV4L2VidiocStreamonFailed:
      return media::mojom::VideoCaptureError::kV4L2VidiocStreamonFailed;
    case media::VideoCaptureError::kV4L2VidiocStreamoffFailed:
      return media::mojom::VideoCaptureError::kV4L2VidiocStreamoffFailed;
    case media::VideoCaptureError::kV4L2FailedToVidiocReqbufsWithCount0:
      return media::mojom::VideoCaptureError::
          kV4L2FailedToVidiocReqbufsWithCount0;
    case media::VideoCaptureError::kV4L2PollFailed:
      return media::mojom::VideoCaptureError::kV4L2PollFailed;
    case media::VideoCaptureError::
        kV4L2MultipleContinuousTimeoutsWhileReadPolling:
      return media::mojom::VideoCaptureError::
          kV4L2MultipleContinuousTimeoutsWhileReadPolling;
    case media::VideoCaptureError::kV4L2FailedToDequeueCaptureBuffer:
      return media::mojom::VideoCaptureError::kV4L2FailedToDequeueCaptureBuffer;
    case media::VideoCaptureError::kV4L2FailedToEnqueueCaptureBuffer:
      return media::mojom::VideoCaptureError::kV4L2FailedToEnqueueCaptureBuffer;
    case media::VideoCaptureError::
        kSingleClientVideoCaptureHostLostConnectionToDevice:
      return media::mojom::VideoCaptureError::
          kSingleClientVideoCaptureHostLostConnectionToDevice;
    case media::VideoCaptureError::kSingleClientVideoCaptureDeviceLaunchAborted:
      return media::mojom::VideoCaptureError::
          kSingleClientVideoCaptureDeviceLaunchAborted;
    case media::VideoCaptureError::
        kDesktopCaptureDeviceWebrtcDesktopCapturerHasFailed:
      return media::mojom::VideoCaptureError::
          kDesktopCaptureDeviceWebrtcDesktopCapturerHasFailed;
    case media::VideoCaptureError::kFileVideoCaptureDeviceCouldNotOpenVideoFile:
      return media::mojom::VideoCaptureError::
          kFileVideoCaptureDeviceCouldNotOpenVideoFile;
    case media::VideoCaptureError::
        kDeviceCaptureLinuxFailedToCreateVideoCaptureDelegate:
      return media::mojom::VideoCaptureError::
          kDeviceCaptureLinuxFailedToCreateVideoCaptureDelegate;
    case media::VideoCaptureError::
        kErrorFakeDeviceIntentionallyEmittingErrorEvent:
      return media::mojom::VideoCaptureError::
          kErrorFakeDeviceIntentionallyEmittingErrorEvent;
    case media::VideoCaptureError::kDeviceClientTooManyFramesDroppedY16:
      return media::mojom::VideoCaptureError::
          kDeviceClientTooManyFramesDroppedY16;
    case media::VideoCaptureError::
        kDeviceMediaToMojoAdapterEncounteredUnsupportedBufferType:
      return media::mojom::VideoCaptureError::
          kDeviceMediaToMojoAdapterEncounteredUnsupportedBufferType;
    case media::VideoCaptureError::
        kVideoCaptureManagerProcessDeviceStartQueueDeviceInfoNotFound:
      return media::mojom::VideoCaptureError::
          kVideoCaptureManagerProcessDeviceStartQueueDeviceInfoNotFound;
    case media::VideoCaptureError::
        kInProcessDeviceLauncherFailedToCreateDeviceInstance:
      return media::mojom::VideoCaptureError::
          kInProcessDeviceLauncherFailedToCreateDeviceInstance;
    case media::VideoCaptureError::
        kServiceDeviceLauncherLostConnectionToDeviceFactoryDuringDeviceStart:
      return media::mojom::VideoCaptureError::
          kServiceDeviceLauncherLostConnectionToDeviceFactoryDuringDeviceStart;
    case media::VideoCaptureError::
        kServiceDeviceLauncherServiceRespondedWithDeviceNotFound:
      return media::mojom::VideoCaptureError::
          kServiceDeviceLauncherServiceRespondedWithDeviceNotFound;
    case media::VideoCaptureError::
        kServiceDeviceLauncherConnectionLostWhileWaitingForCallback:
      return media::mojom::VideoCaptureError::
          kServiceDeviceLauncherConnectionLostWhileWaitingForCallback;
    case media::VideoCaptureError::kIntentionalErrorRaisedByUnitTest:
      return media::mojom::VideoCaptureError::kIntentionalErrorRaisedByUnitTest;
    case media::VideoCaptureError::kCrosHalV3FailedToStartDeviceThread:
      return media::mojom::VideoCaptureError::
          kCrosHalV3FailedToStartDeviceThread;
    case media::VideoCaptureError::kCrosHalV3DeviceDelegateMojoConnectionError:
      return media::mojom::VideoCaptureError::
          kCrosHalV3DeviceDelegateMojoConnectionError;
    case media::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToGetCameraInfo:
      return media::mojom::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToGetCameraInfo;
    case media::VideoCaptureError::
        kCrosHalV3DeviceDelegateMissingSensorOrientationInfo:
      return media::mojom::VideoCaptureError::
          kCrosHalV3DeviceDelegateMissingSensorOrientationInfo;
    case media::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToOpenCameraDevice:
      return media::mojom::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToOpenCameraDevice;
    case media::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToInitializeCameraDevice:
      return media::mojom::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToInitializeCameraDevice;
    case media::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToConfigureStreams:
      return media::mojom::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToConfigureStreams;
    case media::VideoCaptureError::
        kCrosHalV3DeviceDelegateWrongNumberOfStreamsConfigured:
      return media::mojom::VideoCaptureError::
          kCrosHalV3DeviceDelegateWrongNumberOfStreamsConfigured;
    case media::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToGetDefaultRequestSettings:
      return media::mojom::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToGetDefaultRequestSettings;
    case media::VideoCaptureError::
        kCrosHalV3BufferManagerHalRequestedTooManyBuffers:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerHalRequestedTooManyBuffers;
    case media::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToCreateMappableSI:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToCreateMappableSI;
    case media::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToMapGpuMemoryBuffer:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToMapGpuMemoryBuffer;
    case media::VideoCaptureError::
        kCrosHalV3BufferManagerUnsupportedVideoPixelFormat:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerUnsupportedVideoPixelFormat;
    case media::VideoCaptureError::kCrosHalV3BufferManagerFailedToDupFd:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToDupFd;
    case media::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToWrapGpuMemoryHandle:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToWrapGpuMemoryHandle;
    case media::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToRegisterBuffer:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToRegisterBuffer;
    case media::VideoCaptureError::
        kCrosHalV3BufferManagerProcessCaptureRequestFailed:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerProcessCaptureRequestFailed;
    case media::VideoCaptureError::
        kCrosHalV3BufferManagerInvalidPendingResultId:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerInvalidPendingResultId;
    case media::VideoCaptureError::
        kCrosHalV3BufferManagerReceivedDuplicatedPartialMetadata:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerReceivedDuplicatedPartialMetadata;
    case media::VideoCaptureError::
        kCrosHalV3BufferManagerIncorrectNumberOfOutputBuffersReceived:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerIncorrectNumberOfOutputBuffersReceived;
    case media::VideoCaptureError::
        kCrosHalV3BufferManagerInvalidTypeOfOutputBuffersReceived:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerInvalidTypeOfOutputBuffersReceived;
    case media::VideoCaptureError::
        kCrosHalV3BufferManagerReceivedMultipleResultBuffersForFrame:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerReceivedMultipleResultBuffersForFrame;
    case media::VideoCaptureError::
        kCrosHalV3BufferManagerUnknownStreamInCamera3NotifyMsg:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerUnknownStreamInCamera3NotifyMsg;
    case media::VideoCaptureError::
        kCrosHalV3BufferManagerReceivedInvalidShutterTime:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerReceivedInvalidShutterTime;
    case media::VideoCaptureError::kCrosHalV3BufferManagerFatalDeviceError:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerFatalDeviceError;
    case media::VideoCaptureError::
        kCrosHalV3BufferManagerReceivedFrameIsOutOfOrder:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerReceivedFrameIsOutOfOrder;
    case media::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToUnwrapReleaseFenceFd:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToUnwrapReleaseFenceFd;
    case media::VideoCaptureError::
        kCrosHalV3BufferManagerSyncWaitOnReleaseFenceTimedOut:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerSyncWaitOnReleaseFenceTimedOut;
    case media::VideoCaptureError::kCrosHalV3BufferManagerInvalidJpegBlob:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerInvalidJpegBlob;
    case media::VideoCaptureError::kAndroidFailedToAllocate:
      return media::mojom::VideoCaptureError::kAndroidFailedToAllocate;
    case media::VideoCaptureError::kAndroidFailedToStartCapture:
      return media::mojom::VideoCaptureError::kAndroidFailedToStartCapture;
    case media::VideoCaptureError::kAndroidFailedToStopCapture:
      return media::mojom::VideoCaptureError::kAndroidFailedToStopCapture;
    case media::VideoCaptureError::kAndroidApi1CameraErrorCallbackReceived:
      return media::mojom::VideoCaptureError::
          kAndroidApi1CameraErrorCallbackReceived;
    case media::VideoCaptureError::kAndroidApi2CameraDeviceErrorReceived:
      return media::mojom::VideoCaptureError::
          kAndroidApi2CameraDeviceErrorReceived;
    case media::VideoCaptureError::kAndroidApi2CaptureSessionConfigureFailed:
      return media::mojom::VideoCaptureError::
          kAndroidApi2CaptureSessionConfigureFailed;
    case media::VideoCaptureError::kAndroidApi2ImageReaderUnexpectedImageFormat:
      return media::mojom::VideoCaptureError::
          kAndroidApi2ImageReaderUnexpectedImageFormat;
    case media::VideoCaptureError::
        kAndroidApi2ImageReaderSizeDidNotMatchImageSize:
      return media::mojom::VideoCaptureError::
          kAndroidApi2ImageReaderSizeDidNotMatchImageSize;
    case media::VideoCaptureError::kAndroidApi2ErrorRestartingPreview:
      return media::mojom::VideoCaptureError::
          kAndroidApi2ErrorRestartingPreview;
    case media::VideoCaptureError::kAndroidScreenCaptureUnsupportedFormat:
      return media::mojom::VideoCaptureError::
          kAndroidScreenCaptureUnsupportedFormat;
    case media::VideoCaptureError::
        kAndroidScreenCaptureFailedToStartCaptureMachine:
      return media::mojom::VideoCaptureError::
          kAndroidScreenCaptureFailedToStartCaptureMachine;
    case media::VideoCaptureError::
        kAndroidScreenCaptureTheUserDeniedScreenCapture:
      return media::mojom::VideoCaptureError::
          kAndroidScreenCaptureTheUserDeniedScreenCapture;
    case media::VideoCaptureError::
        kAndroidScreenCaptureFailedToStartScreenCapture:
      return media::mojom::VideoCaptureError::
          kAndroidScreenCaptureFailedToStartScreenCapture;
    case media::VideoCaptureError::kWinDirectShowCantGetCaptureFormatSettings:
      return media::mojom::VideoCaptureError::
          kWinDirectShowCantGetCaptureFormatSettings;
    case media::VideoCaptureError::
        kWinDirectShowFailedToGetNumberOfCapabilities:
      return media::mojom::VideoCaptureError::
          kWinDirectShowFailedToGetNumberOfCapabilities;
    case media::VideoCaptureError::
        kWinDirectShowFailedToGetCaptureDeviceCapabilities:
      return media::mojom::VideoCaptureError::
          kWinDirectShowFailedToGetCaptureDeviceCapabilities;
    case media::VideoCaptureError::
        kWinDirectShowFailedToSetCaptureDeviceOutputFormat:
      return media::mojom::VideoCaptureError::
          kWinDirectShowFailedToSetCaptureDeviceOutputFormat;
    case media::VideoCaptureError::kWinDirectShowFailedToConnectTheCaptureGraph:
      return media::mojom::VideoCaptureError::
          kWinDirectShowFailedToConnectTheCaptureGraph;
    case media::VideoCaptureError::kWinDirectShowFailedToPauseTheCaptureDevice:
      return media::mojom::VideoCaptureError::
          kWinDirectShowFailedToPauseTheCaptureDevice;
    case media::VideoCaptureError::kWinDirectShowFailedToStartTheCaptureDevice:
      return media::mojom::VideoCaptureError::
          kWinDirectShowFailedToStartTheCaptureDevice;
    case media::VideoCaptureError::kWinDirectShowFailedToStopTheCaptureGraph:
      return media::mojom::VideoCaptureError::
          kWinDirectShowFailedToStopTheCaptureGraph;
    case media::VideoCaptureError::kWinMediaFoundationEngineIsNull:
      return media::mojom::VideoCaptureError::kWinMediaFoundationEngineIsNull;
    case media::VideoCaptureError::kWinMediaFoundationEngineGetSourceFailed:
      return media::mojom::VideoCaptureError::
          kWinMediaFoundationEngineGetSourceFailed;
    case media::VideoCaptureError::
        kWinMediaFoundationFillPhotoCapabilitiesFailed:
      return media::mojom::VideoCaptureError::
          kWinMediaFoundationFillPhotoCapabilitiesFailed;
    case media::VideoCaptureError::
        kWinMediaFoundationFillVideoCapabilitiesFailed:
      return media::mojom::VideoCaptureError::
          kWinMediaFoundationFillVideoCapabilitiesFailed;
    case media::VideoCaptureError::kWinMediaFoundationNoVideoCapabilityFound:
      return media::mojom::VideoCaptureError::
          kWinMediaFoundationNoVideoCapabilityFound;
    case media::VideoCaptureError::
        kWinMediaFoundationGetAvailableDeviceMediaTypeFailed:
      return media::mojom::VideoCaptureError::
          kWinMediaFoundationGetAvailableDeviceMediaTypeFailed;
    case media::VideoCaptureError::
        kWinMediaFoundationSetCurrentDeviceMediaTypeFailed:
      return media::mojom::VideoCaptureError::
          kWinMediaFoundationSetCurrentDeviceMediaTypeFailed;
    case media::VideoCaptureError::kWinMediaFoundationEngineGetSinkFailed:
      return media::mojom::VideoCaptureError::
          kWinMediaFoundationEngineGetSinkFailed;
    case media::VideoCaptureError::
        kWinMediaFoundationSinkQueryCapturePreviewInterfaceFailed:
      return media::mojom::VideoCaptureError::
          kWinMediaFoundationSinkQueryCapturePreviewInterfaceFailed;
    case media::VideoCaptureError::
        kWinMediaFoundationSinkRemoveAllStreamsFailed:
      return media::mojom::VideoCaptureError::
          kWinMediaFoundationSinkRemoveAllStreamsFailed;
    case media::VideoCaptureError::
        kWinMediaFoundationCreateSinkVideoMediaTypeFailed:
      return media::mojom::VideoCaptureError::
          kWinMediaFoundationCreateSinkVideoMediaTypeFailed;
    case media::VideoCaptureError::
        kWinMediaFoundationConvertToVideoSinkMediaTypeFailed:
      return media::mojom::VideoCaptureError::
          kWinMediaFoundationConvertToVideoSinkMediaTypeFailed;
    case media::VideoCaptureError::kWinMediaFoundationSinkAddStreamFailed:
      return media::mojom::VideoCaptureError::
          kWinMediaFoundationSinkAddStreamFailed;
    case media::VideoCaptureError::
        kWinMediaFoundationSinkSetSampleCallbackFailed:
      return media::mojom::VideoCaptureError::
          kWinMediaFoundationSinkSetSampleCallbackFailed;
    case media::VideoCaptureError::kWinMediaFoundationEngineStartPreviewFailed:
      return media::mojom::VideoCaptureError::
          kWinMediaFoundationEngineStartPreviewFailed;
    case media::VideoCaptureError::kWinMediaFoundationGetMediaEventStatusFailed:
      return media::mojom::VideoCaptureError::
          kWinMediaFoundationGetMediaEventStatusFailed;
    case media::VideoCaptureError::kMacSetCaptureDeviceFailed:
      return media::mojom::VideoCaptureError::kMacSetCaptureDeviceFailed;
    case media::VideoCaptureError::kMacCouldNotStartCaptureDevice:
      return media::mojom::VideoCaptureError::kMacCouldNotStartCaptureDevice;
    case media::VideoCaptureError::kMacReceivedFrameWithUnexpectedResolution:
      return media::mojom::VideoCaptureError::
          kMacReceivedFrameWithUnexpectedResolution;
    case media::VideoCaptureError::kMacUpdateCaptureResolutionFailed:
      return media::mojom::VideoCaptureError::kMacUpdateCaptureResolutionFailed;
    case media::VideoCaptureError::kMacDeckLinkDeviceIdNotFoundInTheSystem:
      return media::mojom::VideoCaptureError::
          kMacDeckLinkDeviceIdNotFoundInTheSystem;
    case media::VideoCaptureError::kMacDeckLinkErrorQueryingInputInterface:
      return media::mojom::VideoCaptureError::
          kMacDeckLinkErrorQueryingInputInterface;
    case media::VideoCaptureError::kMacDeckLinkErrorCreatingDisplayModeIterator:
      return media::mojom::VideoCaptureError::
          kMacDeckLinkErrorCreatingDisplayModeIterator;
    case media::VideoCaptureError::kMacDeckLinkCouldNotFindADisplayMode:
      return media::mojom::VideoCaptureError::
          kMacDeckLinkCouldNotFindADisplayMode;
    case media::VideoCaptureError::
        kMacDeckLinkCouldNotSelectTheVideoFormatWeLike:
      return media::mojom::VideoCaptureError::
          kMacDeckLinkCouldNotSelectTheVideoFormatWeLike;
    case media::VideoCaptureError::kMacDeckLinkCouldNotStartCapturing:
      return media::mojom::VideoCaptureError::
          kMacDeckLinkCouldNotStartCapturing;
    case media::VideoCaptureError::kMacDeckLinkUnsupportedPixelFormat:
      return media::mojom::VideoCaptureError::
          kMacDeckLinkUnsupportedPixelFormat;
    case media::VideoCaptureError::
        kMacAvFoundationReceivedAVCaptureSessionRuntimeErrorNotification:
      return media::mojom::VideoCaptureError::
          kMacAvFoundationReceivedAVCaptureSessionRuntimeErrorNotification;
    case media::VideoCaptureError::kAndroidApi2ErrorConfiguringCamera:
      return media::mojom::VideoCaptureError::
          kAndroidApi2ErrorConfiguringCamera;
    case media::VideoCaptureError::kCrosHalV3DeviceDelegateFailedToFlush:
      return media::mojom::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToFlush;
    case media::VideoCaptureError::kFuchsiaCameraDeviceDisconnected:
      return media::mojom::VideoCaptureError::kFuchsiaCameraDeviceDisconnected;
    case media::VideoCaptureError::kFuchsiaCameraStreamDisconnected:
      return media::mojom::VideoCaptureError::kFuchsiaCameraStreamDisconnected;
    case media::VideoCaptureError::kFuchsiaSysmemDidNotSetImageFormat:
      return media::mojom::VideoCaptureError::
          kFuchsiaSysmemDidNotSetImageFormat;
    case media::VideoCaptureError::kFuchsiaSysmemInvalidBufferIndex:
      return media::mojom::VideoCaptureError::kFuchsiaSysmemInvalidBufferIndex;
    case media::VideoCaptureError::kFuchsiaSysmemInvalidBufferSize:
      return media::mojom::VideoCaptureError::kFuchsiaSysmemInvalidBufferSize;
    case media::VideoCaptureError::kFuchsiaUnsupportedPixelFormat:
      return media::mojom::VideoCaptureError::kFuchsiaUnsupportedPixelFormat;
    case media::VideoCaptureError::kFuchsiaFailedToMapSysmemBuffer:
      return media::mojom::VideoCaptureError::kFuchsiaFailedToMapSysmemBuffer;
    case media::VideoCaptureError::kCrosHalV3DeviceContextDuplicatedClient:
      return media::mojom::VideoCaptureError::
          kCrosHalV3DeviceContextDuplicatedClient;
    case media::VideoCaptureError::kDesktopCaptureDeviceMacFailedStreamCreate:
      return media::mojom::VideoCaptureError::
          kDesktopCaptureDeviceMacFailedStreamCreate;
    case media::VideoCaptureError::kDesktopCaptureDeviceMacFailedStreamStart:
      return media::mojom::VideoCaptureError::
          kDesktopCaptureDeviceMacFailedStreamStart;
    case media::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToReserveBuffers:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToReserveBuffers;
    case media::VideoCaptureError::kWinMediaFoundationSystemPermissionDenied:
      return media::mojom::VideoCaptureError::
          kWinMediaFoundationSystemPermissionDenied;
    case media::VideoCaptureError::kVideoCaptureImplTimedOutOnStart:
      return media::mojom::VideoCaptureError::kVideoCaptureImplTimedOutOnStart;
    case media::VideoCaptureError::
        kLacrosVideoCaptureDeviceProxyAlreadyEndedOnFatalError:
      return media::mojom::VideoCaptureError::
          kLacrosVideoCaptureDeviceProxyAlreadyEndedOnFatalError;
    case media::VideoCaptureError::
        kLacrosVideoCaptureDeviceProxyEncounteredFatalError:
      return media::mojom::VideoCaptureError::
          kLacrosVideoCaptureDeviceProxyEncounteredFatalError;
    case media::VideoCaptureError::kScreenCaptureKitFailedGetShareableContent:
      return media::mojom::VideoCaptureError::
          kScreenCaptureKitFailedGetShareableContent;
    case media::VideoCaptureError::kScreenCaptureKitFailedAddStreamOutput:
      return media::mojom::VideoCaptureError::
          kScreenCaptureKitFailedAddStreamOutput;
    case media::VideoCaptureError::kScreenCaptureKitFailedStartCapture:
      return media::mojom::VideoCaptureError::
          kScreenCaptureKitFailedStartCapture;
    case media::VideoCaptureError::kScreenCaptureKitFailedStopCapture:
      return media::mojom::VideoCaptureError::
          kScreenCaptureKitFailedStopCapture;
    case media::VideoCaptureError::kScreenCaptureKitStreamError:
      return media::mojom::VideoCaptureError::kScreenCaptureKitStreamError;
    case media::VideoCaptureError::kScreenCaptureKitFailedToFindSCDisplay:
      return media::mojom::VideoCaptureError::
          kScreenCaptureKitFailedToFindSCDisplay;
    case media::VideoCaptureError::
        kVideoCaptureControllerUnsupportedPixelFormat:
      return media::mojom::VideoCaptureError::
          kVideoCaptureControllerUnsupportedPixelFormat;
    case media::VideoCaptureError::kVideoCaptureControllerInvalid:
      return media::mojom::VideoCaptureError::kVideoCaptureControllerInvalid;
    case media::VideoCaptureError::
        kVideoCaptureDeviceFactoryChromeOSCreateDeviceFailed:
      return media::mojom::VideoCaptureError::
          kVideoCaptureDeviceFactoryChromeOSCreateDeviceFailed;
    case media::VideoCaptureError::kVideoCaptureDeviceAlreadyReleased:
      return media::mojom::VideoCaptureError::
          kVideoCaptureDeviceAlreadyReleased;
    case media::VideoCaptureError::kVideoCaptureSystemDeviceIdNotFound:
      return media::mojom::VideoCaptureError::
          kVideoCaptureSystemDeviceIdNotFound;
    case media::VideoCaptureError::kVideoCaptureDeviceFactoryWinUnknownError:
      return media::mojom::VideoCaptureError::
          kVideoCaptureDeviceFactoryWinUnknownError;
    case media::VideoCaptureError::
        kWinMediaFoundationDeviceInitializationFailed:
      return media::mojom::VideoCaptureError::
          kWinMediaFoundationDeviceInitializationFailed;
    case media::VideoCaptureError::kWinMediaFoundationSourceCreationFailed:
      return media::mojom::VideoCaptureError::
          kWinMediaFoundationSourceCreationFailed;
    case media::VideoCaptureError::kWinDirectShowDeviceFilterCreationFailed:
      return media::mojom::VideoCaptureError::
          kWinDirectShowDeviceFilterCreationFailed;
    case media::VideoCaptureError::kWinDirectShowDeviceInitializationFailed:
      return media::mojom::VideoCaptureError::
          kWinDirectShowDeviceInitializationFailed;
    case media::VideoCaptureError::kVideoCaptureDeviceFactorySecondCreateDenied:
      return media::mojom::VideoCaptureError::
          kVideoCaptureDeviceFactorySecondCreateDenied;
    case media::VideoCaptureError::kScreenCaptureKitResetStreamError:
      return media::mojom::VideoCaptureError::kScreenCaptureKitResetStreamError;
    case media::VideoCaptureError::kWinMediaFoundationCameraBusy:
      return media::mojom::VideoCaptureError::kWinMediaFoundationCameraBusy;
    case media::VideoCaptureError::kWebRtcStartCaptureFailed:
      return media::mojom::VideoCaptureError::kWebRtcStartCaptureFailed;
  }
  NOTREACHED();
}

// static
media::VideoCaptureError
EnumTraits<media::mojom::VideoCaptureError, media::VideoCaptureError>::
    FromMojom(media::mojom::VideoCaptureError input) {
  switch (input) {
    case media::mojom::VideoCaptureError::kNone:
      return media::VideoCaptureError::kNone;
    case media::mojom::VideoCaptureError::
        kVideoCaptureControllerInvalidOrUnsupportedVideoCaptureParametersRequested:
      return media::VideoCaptureError::
          kVideoCaptureControllerInvalidOrUnsupportedVideoCaptureParametersRequested;
    case media::mojom::VideoCaptureError::
        kVideoCaptureControllerIsAlreadyInErrorState:
      return media::VideoCaptureError::
          kVideoCaptureControllerIsAlreadyInErrorState;
    case media::mojom::VideoCaptureError::
        kVideoCaptureManagerDeviceConnectionLost:
      return media::VideoCaptureError::kVideoCaptureManagerDeviceConnectionLost;
    case media::mojom::VideoCaptureError::
        kFrameSinkVideoCaptureDeviceAlreadyEndedOnFatalError:
      return media::VideoCaptureError::
          kFrameSinkVideoCaptureDeviceAlreadyEndedOnFatalError;
    case media::mojom::VideoCaptureError::
        kFrameSinkVideoCaptureDeviceEncounteredFatalError:
      return media::VideoCaptureError::
          kFrameSinkVideoCaptureDeviceEncounteredFatalError;
    case media::mojom::VideoCaptureError::kV4L2FailedToOpenV4L2DeviceDriverFile:
      return media::VideoCaptureError::kV4L2FailedToOpenV4L2DeviceDriverFile;
    case media::mojom::VideoCaptureError::kV4L2ThisIsNotAV4L2VideoCaptureDevice:
      return media::VideoCaptureError::kV4L2ThisIsNotAV4L2VideoCaptureDevice;
    case media::mojom::VideoCaptureError::
        kV4L2FailedToFindASupportedCameraFormat:
      return media::VideoCaptureError::kV4L2FailedToFindASupportedCameraFormat;
    case media::mojom::VideoCaptureError::kV4L2FailedToSetVideoCaptureFormat:
      return media::VideoCaptureError::kV4L2FailedToSetVideoCaptureFormat;
    case media::mojom::VideoCaptureError::kV4L2UnsupportedPixelFormat:
      return media::VideoCaptureError::kV4L2UnsupportedPixelFormat;
    case media::mojom::VideoCaptureError::kV4L2FailedToSetCameraFramerate:
      return media::VideoCaptureError::kV4L2FailedToSetCameraFramerate;
    case media::mojom::VideoCaptureError::kV4L2ErrorRequestingMmapBuffers:
      return media::VideoCaptureError::kV4L2ErrorRequestingMmapBuffers;
    case media::mojom::VideoCaptureError::kV4L2AllocateBufferFailed:
      return media::VideoCaptureError::kV4L2AllocateBufferFailed;
    case media::mojom::VideoCaptureError::kV4L2VidiocStreamonFailed:
      return media::VideoCaptureError::kV4L2VidiocStreamonFailed;
    case media::mojom::VideoCaptureError::kV4L2VidiocStreamoffFailed:
      return media::VideoCaptureError::kV4L2VidiocStreamoffFailed;
    case media::mojom::VideoCaptureError::kV4L2FailedToVidiocReqbufsWithCount0:
      return media::VideoCaptureError::kV4L2FailedToVidiocReqbufsWithCount0;
    case media::mojom::VideoCaptureError::kV4L2PollFailed:
      return media::VideoCaptureError::kV4L2PollFailed;
    case media::mojom::VideoCaptureError::
        kV4L2MultipleContinuousTimeoutsWhileReadPolling:
      return media::VideoCaptureError::
          kV4L2MultipleContinuousTimeoutsWhileReadPolling;
    case media::mojom::VideoCaptureError::kV4L2FailedToDequeueCaptureBuffer:
      return media::VideoCaptureError::kV4L2FailedToDequeueCaptureBuffer;
    case media::mojom::VideoCaptureError::kV4L2FailedToEnqueueCaptureBuffer:
      return media::VideoCaptureError::kV4L2FailedToEnqueueCaptureBuffer;
    case media::mojom::VideoCaptureError::
        kSingleClientVideoCaptureHostLostConnectionToDevice:
      return media::VideoCaptureError::
          kSingleClientVideoCaptureHostLostConnectionToDevice;
    case media::mojom::VideoCaptureError::
        kSingleClientVideoCaptureDeviceLaunchAborted:
      return media::VideoCaptureError::
          kSingleClientVideoCaptureDeviceLaunchAborted;
    case media::mojom::VideoCaptureError::
        kDesktopCaptureDeviceWebrtcDesktopCapturerHasFailed:
      return media::VideoCaptureError::
          kDesktopCaptureDeviceWebrtcDesktopCapturerHasFailed;
    case media::mojom::VideoCaptureError::
        kFileVideoCaptureDeviceCouldNotOpenVideoFile:
      return media::VideoCaptureError::
          kFileVideoCaptureDeviceCouldNotOpenVideoFile;
    case media::mojom::VideoCaptureError::
        kDeviceCaptureLinuxFailedToCreateVideoCaptureDelegate:
      return media::VideoCaptureError::
          kDeviceCaptureLinuxFailedToCreateVideoCaptureDelegate;
    case media::mojom::VideoCaptureError::
        kErrorFakeDeviceIntentionallyEmittingErrorEvent:
      return media::VideoCaptureError::
          kErrorFakeDeviceIntentionallyEmittingErrorEvent;
    case media::mojom::VideoCaptureError::kDeviceClientTooManyFramesDroppedY16:
      return media::VideoCaptureError::kDeviceClientTooManyFramesDroppedY16;
    case media::mojom::VideoCaptureError::
        kDeviceMediaToMojoAdapterEncounteredUnsupportedBufferType:
      return media::VideoCaptureError::
          kDeviceMediaToMojoAdapterEncounteredUnsupportedBufferType;
    case media::mojom::VideoCaptureError::
        kVideoCaptureManagerProcessDeviceStartQueueDeviceInfoNotFound:
      return media::VideoCaptureError::
          kVideoCaptureManagerProcessDeviceStartQueueDeviceInfoNotFound;
    case media::mojom::VideoCaptureError::
        kInProcessDeviceLauncherFailedToCreateDeviceInstance:
      return media::VideoCaptureError::
          kInProcessDeviceLauncherFailedToCreateDeviceInstance;
    case media::mojom::VideoCaptureError::
        kServiceDeviceLauncherLostConnectionToDeviceFactoryDuringDeviceStart:
      return media::VideoCaptureError::
          kServiceDeviceLauncherLostConnectionToDeviceFactoryDuringDeviceStart;
    case media::mojom::VideoCaptureError::
        kServiceDeviceLauncherServiceRespondedWithDeviceNotFound:
      return media::VideoCaptureError::
          kServiceDeviceLauncherServiceRespondedWithDeviceNotFound;
    case media::mojom::VideoCaptureError::
        kServiceDeviceLauncherConnectionLostWhileWaitingForCallback:
      return media::VideoCaptureError::
          kServiceDeviceLauncherConnectionLostWhileWaitingForCallback;
    case media::mojom::VideoCaptureError::kIntentionalErrorRaisedByUnitTest:
      return media::VideoCaptureError::kIntentionalErrorRaisedByUnitTest;
    case media::mojom::VideoCaptureError::kCrosHalV3FailedToStartDeviceThread:
      return media::VideoCaptureError::kCrosHalV3FailedToStartDeviceThread;
    case media::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateMojoConnectionError:
      return media::VideoCaptureError::
          kCrosHalV3DeviceDelegateMojoConnectionError;
    case media::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToGetCameraInfo:
      return media::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToGetCameraInfo;
    case media::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateMissingSensorOrientationInfo:
      return media::VideoCaptureError::
          kCrosHalV3DeviceDelegateMissingSensorOrientationInfo;
    case media::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToOpenCameraDevice:
      return media::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToOpenCameraDevice;
    case media::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToInitializeCameraDevice:
      return media::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToInitializeCameraDevice;
    case media::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToConfigureStreams:
      return media::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToConfigureStreams;
    case media::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateWrongNumberOfStreamsConfigured:
      return media::VideoCaptureError::
          kCrosHalV3DeviceDelegateWrongNumberOfStreamsConfigured;
    case media::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToGetDefaultRequestSettings:
      return media::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToGetDefaultRequestSettings;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerHalRequestedTooManyBuffers:
      return media::VideoCaptureError::
          kCrosHalV3BufferManagerHalRequestedTooManyBuffers;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToCreateMappableSI:
      return media::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToCreateMappableSI;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToMapGpuMemoryBuffer:
      return media::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToMapGpuMemoryBuffer;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerUnsupportedVideoPixelFormat:
      return media::VideoCaptureError::
          kCrosHalV3BufferManagerUnsupportedVideoPixelFormat;
    case media::mojom::VideoCaptureError::kCrosHalV3BufferManagerFailedToDupFd:
      return media::VideoCaptureError::kCrosHalV3BufferManagerFailedToDupFd;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToWrapGpuMemoryHandle:
      return media::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToWrapGpuMemoryHandle;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToRegisterBuffer:
      return media::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToRegisterBuffer;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerProcessCaptureRequestFailed:
      return media::VideoCaptureError::
          kCrosHalV3BufferManagerProcessCaptureRequestFailed;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerInvalidPendingResultId:
      return media::VideoCaptureError::
          kCrosHalV3BufferManagerInvalidPendingResultId;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerReceivedDuplicatedPartialMetadata:
      return media::VideoCaptureError::
          kCrosHalV3BufferManagerReceivedDuplicatedPartialMetadata;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerIncorrectNumberOfOutputBuffersReceived:
      return media::VideoCaptureError::
          kCrosHalV3BufferManagerIncorrectNumberOfOutputBuffersReceived;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerInvalidTypeOfOutputBuffersReceived:
      return media::VideoCaptureError::
          kCrosHalV3BufferManagerInvalidTypeOfOutputBuffersReceived;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerReceivedMultipleResultBuffersForFrame:
      return media::VideoCaptureError::
          kCrosHalV3BufferManagerReceivedMultipleResultBuffersForFrame;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerUnknownStreamInCamera3NotifyMsg:
      return media::VideoCaptureError::
          kCrosHalV3BufferManagerUnknownStreamInCamera3NotifyMsg;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerReceivedInvalidShutterTime:
      return media::VideoCaptureError::
          kCrosHalV3BufferManagerReceivedInvalidShutterTime;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerFatalDeviceError:
      return media::VideoCaptureError::kCrosHalV3BufferManagerFatalDeviceError;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerReceivedFrameIsOutOfOrder:
      return media::VideoCaptureError::
          kCrosHalV3BufferManagerReceivedFrameIsOutOfOrder;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToUnwrapReleaseFenceFd:
      return media::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToUnwrapReleaseFenceFd;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerSyncWaitOnReleaseFenceTimedOut:
      return media::VideoCaptureError::
          kCrosHalV3BufferManagerSyncWaitOnReleaseFenceTimedOut;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerInvalidJpegBlob:
      return media::VideoCaptureError::kCrosHalV3BufferManagerInvalidJpegBlob;
    case media::mojom::VideoCaptureError::kAndroidFailedToAllocate:
      return media::VideoCaptureError::kAndroidFailedToAllocate;
    case media::mojom::VideoCaptureError::kAndroidFailedToStartCapture:
      return media::VideoCaptureError::kAndroidFailedToStartCapture;
    case media::mojom::VideoCaptureError::kAndroidFailedToStopCapture:
      return media::VideoCaptureError::kAndroidFailedToStopCapture;
    case media::mojom::VideoCaptureError::
        kAndroidApi1CameraErrorCallbackReceived:
      return media::VideoCaptureError::kAndroidApi1CameraErrorCallbackReceived;
    case media::mojom::VideoCaptureError::kAndroidApi2CameraDeviceErrorReceived:
      return media::VideoCaptureError::kAndroidApi2CameraDeviceErrorReceived;
    case media::mojom::VideoCaptureError::
        kAndroidApi2CaptureSessionConfigureFailed:
      return media::VideoCaptureError::
          kAndroidApi2CaptureSessionConfigureFailed;
    case media::mojom::VideoCaptureError::
        kAndroidApi2ImageReaderUnexpectedImageFormat:
      return media::VideoCaptureError::
          kAndroidApi2ImageReaderUnexpectedImageFormat;
    case media::mojom::VideoCaptureError::
        kAndroidApi2ImageReaderSizeDidNotMatchImageSize:
      return media::VideoCaptureError::
          kAndroidApi2ImageReaderSizeDidNotMatchImageSize;
    case media::mojom::VideoCaptureError::kAndroidApi2ErrorRestartingPreview:
      return media::VideoCaptureError::kAndroidApi2ErrorRestartingPreview;
    case media::mojom::VideoCaptureError::
        kAndroidScreenCaptureUnsupportedFormat:
      return media::VideoCaptureError::kAndroidScreenCaptureUnsupportedFormat;
    case media::mojom::VideoCaptureError::
        kAndroidScreenCaptureFailedToStartCaptureMachine:
      return media::VideoCaptureError::
          kAndroidScreenCaptureFailedToStartCaptureMachine;
    case media::mojom::VideoCaptureError::
        kAndroidScreenCaptureTheUserDeniedScreenCapture:
      return media::VideoCaptureError::
          kAndroidScreenCaptureTheUserDeniedScreenCapture;
    case media::mojom::VideoCaptureError::
        kAndroidScreenCaptureFailedToStartScreenCapture:
      return media::VideoCaptureError::
          kAndroidScreenCaptureFailedToStartScreenCapture;
    case media::mojom::VideoCaptureError::
        kWinDirectShowCantGetCaptureFormatSettings:
      return media::VideoCaptureError::
          kWinDirectShowCantGetCaptureFormatSettings;
    case media::mojom::VideoCaptureError::
        kWinDirectShowFailedToGetNumberOfCapabilities:
      return media::VideoCaptureError::
          kWinDirectShowFailedToGetNumberOfCapabilities;
    case media::mojom::VideoCaptureError::
        kWinDirectShowFailedToGetCaptureDeviceCapabilities:
      return media::VideoCaptureError::
          kWinDirectShowFailedToGetCaptureDeviceCapabilities;
    case media::mojom::VideoCaptureError::
        kWinDirectShowFailedToSetCaptureDeviceOutputFormat:
      return media::VideoCaptureError::
          kWinDirectShowFailedToSetCaptureDeviceOutputFormat;
    case media::mojom::VideoCaptureError::
        kWinDirectShowFailedToConnectTheCaptureGraph:
      return media::VideoCaptureError::
          kWinDirectShowFailedToConnectTheCaptureGraph;
    case media::mojom::VideoCaptureError::
        kWinDirectShowFailedToPauseTheCaptureDevice:
      return media::VideoCaptureError::
          kWinDirectShowFailedToPauseTheCaptureDevice;
    case media::mojom::VideoCaptureError::
        kWinDirectShowFailedToStartTheCaptureDevice:
      return media::VideoCaptureError::
          kWinDirectShowFailedToStartTheCaptureDevice;
    case media::mojom::VideoCaptureError::
        kWinDirectShowFailedToStopTheCaptureGraph:
      return media::VideoCaptureError::
          kWinDirectShowFailedToStopTheCaptureGraph;
    case media::mojom::VideoCaptureError::kWinMediaFoundationEngineIsNull:
      return media::VideoCaptureError::kWinMediaFoundationEngineIsNull;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationEngineGetSourceFailed:
      return media::VideoCaptureError::kWinMediaFoundationEngineGetSourceFailed;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationFillPhotoCapabilitiesFailed:
      return media::VideoCaptureError::
          kWinMediaFoundationFillPhotoCapabilitiesFailed;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationFillVideoCapabilitiesFailed:
      return media::VideoCaptureError::
          kWinMediaFoundationFillVideoCapabilitiesFailed;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationNoVideoCapabilityFound:
      return media::VideoCaptureError::
          kWinMediaFoundationNoVideoCapabilityFound;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationGetAvailableDeviceMediaTypeFailed:
      return media::VideoCaptureError::
          kWinMediaFoundationGetAvailableDeviceMediaTypeFailed;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationSetCurrentDeviceMediaTypeFailed:
      return media::VideoCaptureError::
          kWinMediaFoundationSetCurrentDeviceMediaTypeFailed;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationEngineGetSinkFailed:
      return media::VideoCaptureError::kWinMediaFoundationEngineGetSinkFailed;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationSinkQueryCapturePreviewInterfaceFailed:
      return media::VideoCaptureError::
          kWinMediaFoundationSinkQueryCapturePreviewInterfaceFailed;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationSinkRemoveAllStreamsFailed:
      return media::VideoCaptureError::
          kWinMediaFoundationSinkRemoveAllStreamsFailed;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationCreateSinkVideoMediaTypeFailed:
      return media::VideoCaptureError::
          kWinMediaFoundationCreateSinkVideoMediaTypeFailed;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationConvertToVideoSinkMediaTypeFailed:
      return media::VideoCaptureError::
          kWinMediaFoundationConvertToVideoSinkMediaTypeFailed;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationSinkAddStreamFailed:
      return media::VideoCaptureError::kWinMediaFoundationSinkAddStreamFailed;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationSinkSetSampleCallbackFailed:
      return media::VideoCaptureError::
          kWinMediaFoundationSinkSetSampleCallbackFailed;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationEngineStartPreviewFailed:
      return media::VideoCaptureError::
          kWinMediaFoundationEngineStartPreviewFailed;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationGetMediaEventStatusFailed:
      return media::VideoCaptureError::
          kWinMediaFoundationGetMediaEventStatusFailed;
    case media::mojom::VideoCaptureError::kMacSetCaptureDeviceFailed:
      return media::VideoCaptureError::kMacSetCaptureDeviceFailed;
    case media::mojom::VideoCaptureError::kMacCouldNotStartCaptureDevice:
      return media::VideoCaptureError::kMacCouldNotStartCaptureDevice;
    case media::mojom::VideoCaptureError::
        kMacReceivedFrameWithUnexpectedResolution:
      return media::VideoCaptureError::
          kMacReceivedFrameWithUnexpectedResolution;
    case media::mojom::VideoCaptureError::kMacUpdateCaptureResolutionFailed:
      return media::VideoCaptureError::kMacUpdateCaptureResolutionFailed;
    case media::mojom::VideoCaptureError::
        kMacDeckLinkDeviceIdNotFoundInTheSystem:
      return media::VideoCaptureError::kMacDeckLinkDeviceIdNotFoundInTheSystem;
    case media::mojom::VideoCaptureError::
        kMacDeckLinkErrorQueryingInputInterface:
      return media::VideoCaptureError::kMacDeckLinkErrorQueryingInputInterface;
    case media::mojom::VideoCaptureError::
        kMacDeckLinkErrorCreatingDisplayModeIterator:
      return media::VideoCaptureError::
          kMacDeckLinkErrorCreatingDisplayModeIterator;
    case media::mojom::VideoCaptureError::kMacDeckLinkCouldNotFindADisplayMode:
      return media::VideoCaptureError::kMacDeckLinkCouldNotFindADisplayMode;
    case media::mojom::VideoCaptureError::
        kMacDeckLinkCouldNotSelectTheVideoFormatWeLike:
      return media::VideoCaptureError::
          kMacDeckLinkCouldNotSelectTheVideoFormatWeLike;
    case media::mojom::VideoCaptureError::kMacDeckLinkCouldNotStartCapturing:
      return media::VideoCaptureError::kMacDeckLinkCouldNotStartCapturing;
    case media::mojom::VideoCaptureError::kMacDeckLinkUnsupportedPixelFormat:
      return media::VideoCaptureError::kMacDeckLinkUnsupportedPixelFormat;
    case media::mojom::VideoCaptureError::
        kMacAvFoundationReceivedAVCaptureSessionRuntimeErrorNotification:
      return media::VideoCaptureError::
          kMacAvFoundationReceivedAVCaptureSessionRuntimeErrorNotification;
    case media::mojom::VideoCaptureError::kAndroidApi2ErrorConfiguringCamera:
      return media::VideoCaptureError::kAndroidApi2ErrorConfiguringCamera;
    case media::mojom::VideoCaptureError::kCrosHalV3DeviceDelegateFailedToFlush:
      return media::VideoCaptureError::kCrosHalV3DeviceDelegateFailedToFlush;
    case media::mojom::VideoCaptureError::kFuchsiaCameraDeviceDisconnected:
      return media::VideoCaptureError::kFuchsiaCameraDeviceDisconnected;
    case media::mojom::VideoCaptureError::kFuchsiaCameraStreamDisconnected:
      return media::VideoCaptureError::kFuchsiaCameraStreamDisconnected;
    case media::mojom::VideoCaptureError::kFuchsiaSysmemDidNotSetImageFormat:
      return media::VideoCaptureError::kFuchsiaSysmemDidNotSetImageFormat;
    case media::mojom::VideoCaptureError::kFuchsiaSysmemInvalidBufferIndex:
      return media::VideoCaptureError::kFuchsiaSysmemInvalidBufferIndex;
    case media::mojom::VideoCaptureError::kFuchsiaSysmemInvalidBufferSize:
      return media::VideoCaptureError::kFuchsiaSysmemInvalidBufferSize;
    case media::mojom::VideoCaptureError::kFuchsiaUnsupportedPixelFormat:
      return media::VideoCaptureError::kFuchsiaUnsupportedPixelFormat;
    case media::mojom::VideoCaptureError::kFuchsiaFailedToMapSysmemBuffer:
      return media::VideoCaptureError::kFuchsiaFailedToMapSysmemBuffer;
    case media::mojom::VideoCaptureError::
        kCrosHalV3DeviceContextDuplicatedClient:
      return media::VideoCaptureError::kCrosHalV3DeviceContextDuplicatedClient;
    case media::mojom::VideoCaptureError::
        kDesktopCaptureDeviceMacFailedStreamCreate:
      return media::VideoCaptureError::
          kDesktopCaptureDeviceMacFailedStreamCreate;
    case media::mojom::VideoCaptureError::
        kDesktopCaptureDeviceMacFailedStreamStart:
      return media::VideoCaptureError::
          kDesktopCaptureDeviceMacFailedStreamStart;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToReserveBuffers:
      return media::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToReserveBuffers;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationSystemPermissionDenied:
      return media::VideoCaptureError::
          kWinMediaFoundationSystemPermissionDenied;
    case media::mojom::VideoCaptureError::kVideoCaptureImplTimedOutOnStart:
      return media::VideoCaptureError::kVideoCaptureImplTimedOutOnStart;
    case media::mojom::VideoCaptureError::
        kLacrosVideoCaptureDeviceProxyAlreadyEndedOnFatalError:
      return media::VideoCaptureError::
          kLacrosVideoCaptureDeviceProxyAlreadyEndedOnFatalError;
    case media::mojom::VideoCaptureError::
        kLacrosVideoCaptureDeviceProxyEncounteredFatalError:
      return media::VideoCaptureError::
          kLacrosVideoCaptureDeviceProxyEncounteredFatalError;
    case media::mojom::VideoCaptureError::
        kScreenCaptureKitFailedGetShareableContent:
      return media::VideoCaptureError::
          kScreenCaptureKitFailedGetShareableContent;
    case media::mojom::VideoCaptureError::
        kScreenCaptureKitFailedAddStreamOutput:
      return media::VideoCaptureError::kScreenCaptureKitFailedAddStreamOutput;
    case media::mojom::VideoCaptureError::kScreenCaptureKitFailedStartCapture:
      return media::VideoCaptureError::kScreenCaptureKitFailedStartCapture;
    case media::mojom::VideoCaptureError::kScreenCaptureKitFailedStopCapture:
      return media::VideoCaptureError::kScreenCaptureKitFailedStopCapture;
    case media::mojom::VideoCaptureError::kScreenCaptureKitStreamError:
      return media::VideoCaptureError::kScreenCaptureKitStreamError;
    case media::mojom::VideoCaptureError::
        kScreenCaptureKitFailedToFindSCDisplay:
      return media::VideoCaptureError::kScreenCaptureKitFailedToFindSCDisplay;
    case media::mojom::VideoCaptureError::
        kVideoCaptureControllerUnsupportedPixelFormat:
      return media::VideoCaptureError::
          kVideoCaptureControllerUnsupportedPixelFormat;
    case media::mojom::VideoCaptureError::kVideoCaptureControllerInvalid:
      return media::VideoCaptureError::kVideoCaptureControllerInvalid;
    case media::mojom::VideoCaptureError::
        kVideoCaptureDeviceFactoryChromeOSCreateDeviceFailed:
      return media::VideoCaptureError::
          kVideoCaptureDeviceFactoryChromeOSCreateDeviceFailed;
    case media::mojom::VideoCaptureError::kVideoCaptureDeviceAlreadyReleased:
      return media::VideoCaptureError::kVideoCaptureDeviceAlreadyReleased;
    case media::mojom::VideoCaptureError::kVideoCaptureSystemDeviceIdNotFound:
      return media::VideoCaptureError::kVideoCaptureSystemDeviceIdNotFound;
    case media::mojom::VideoCaptureError::
        kVideoCaptureDeviceFactoryWinUnknownError:
      return media::VideoCaptureError::
          kVideoCaptureDeviceFactoryWinUnknownError;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationDeviceInitializationFailed:
      return media::VideoCaptureError::
          kWinMediaFoundationDeviceInitializationFailed;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationSourceCreationFailed:
      return media::VideoCaptureError::kWinMediaFoundationSourceCreationFailed;
    case media::mojom::VideoCaptureError::
        kWinDirectShowDeviceFilterCreationFailed:
      return media::VideoCaptureError::kWinDirectShowDeviceFilterCreationFailed;
    case media::mojom::VideoCaptureError::
        kWinDirectShowDeviceInitializationFailed:
      return media::VideoCaptureError::kWinDirectShowDeviceInitializationFailed;
    case media::mojom::VideoCaptureError::
        kVideoCaptureDeviceFactorySecondCreateDenied:
      return media::VideoCaptureError::
          kVideoCaptureDeviceFactorySecondCreateDenied;
    case media::mojom::VideoCaptureError::kScreenCaptureKitResetStreamError:
      return media::VideoCaptureError::kScreenCaptureKitResetStreamError;
    case media::mojom::VideoCaptureError::kWinMediaFoundationCameraBusy:
      return media::VideoCaptureError::kWinMediaFoundationCameraBusy;
    case media::mojom::VideoCaptureError::kWebRtcStartCaptureFailed:
      return media::VideoCaptureError::kWebRtcStartCaptureFailed;
  }
  NOTREACHED();
}

// static
media::mojom::VideoCaptureFrameDropReason
EnumTraits<media::mojom::VideoCaptureFrameDropReason,
           media::VideoCaptureFrameDropReason>::
    ToMojom(media::VideoCaptureFrameDropReason input) {
  switch (input) {
    case media::VideoCaptureFrameDropReason::kNone:
      return media::mojom::VideoCaptureFrameDropReason::kNone;
    case media::VideoCaptureFrameDropReason::kDeviceClientFrameHasInvalidFormat:
      return media::mojom::VideoCaptureFrameDropReason::
          kDeviceClientFrameHasInvalidFormat;
    case media::VideoCaptureFrameDropReason::
        kDeviceClientLibyuvConvertToI420Failed:
      return media::mojom::VideoCaptureFrameDropReason::
          kDeviceClientLibyuvConvertToI420Failed;
    case media::VideoCaptureFrameDropReason::kV4L2BufferErrorFlagWasSet:
      return media::mojom::VideoCaptureFrameDropReason::
          kV4L2BufferErrorFlagWasSet;
    case media::VideoCaptureFrameDropReason::kV4L2InvalidNumberOfBytesInBuffer:
      return media::mojom::VideoCaptureFrameDropReason::
          kV4L2InvalidNumberOfBytesInBuffer;
    case media::VideoCaptureFrameDropReason::kAndroidThrottling:
      return media::mojom::VideoCaptureFrameDropReason::kAndroidThrottling;
    case media::VideoCaptureFrameDropReason::kAndroidGetByteArrayElementsFailed:
      return media::mojom::VideoCaptureFrameDropReason::
          kAndroidGetByteArrayElementsFailed;
    case media::VideoCaptureFrameDropReason::kAndroidApi1UnexpectedDataLength:
      return media::mojom::VideoCaptureFrameDropReason::
          kAndroidApi1UnexpectedDataLength;
    case media::VideoCaptureFrameDropReason::kAndroidApi2AcquiredImageIsNull:
      return media::mojom::VideoCaptureFrameDropReason::
          kAndroidApi2AcquiredImageIsNull;
    case media::VideoCaptureFrameDropReason::
        kWinDirectShowUnexpectedSampleLength:
      return media::mojom::VideoCaptureFrameDropReason::
          kWinDirectShowUnexpectedSampleLength;
    case media::VideoCaptureFrameDropReason::
        kWinDirectShowFailedToGetMemoryPointerFromMediaSample:
      return media::mojom::VideoCaptureFrameDropReason::
          kWinDirectShowFailedToGetMemoryPointerFromMediaSample;
    case media::VideoCaptureFrameDropReason::
        kWinMediaFoundationReceivedSampleIsNull:
      return media::mojom::VideoCaptureFrameDropReason::
          kWinMediaFoundationReceivedSampleIsNull;
    case media::VideoCaptureFrameDropReason::
        kWinMediaFoundationLockingBufferDelieveredNullptr:
      return media::mojom::VideoCaptureFrameDropReason::
          kWinMediaFoundationLockingBufferDelieveredNullptr;
    case media::VideoCaptureFrameDropReason::
        kWinMediaFoundationGetBufferByIndexReturnedNull:
      return media::mojom::VideoCaptureFrameDropReason::
          kWinMediaFoundationGetBufferByIndexReturnedNull;
    case media::VideoCaptureFrameDropReason::kBufferPoolMaxBufferCountExceeded:
      return media::mojom::VideoCaptureFrameDropReason::
          kBufferPoolMaxBufferCountExceeded;
    case media::VideoCaptureFrameDropReason::kBufferPoolBufferAllocationFailed:
      return media::mojom::VideoCaptureFrameDropReason::
          kBufferPoolBufferAllocationFailed;
    case media::VideoCaptureFrameDropReason::kVideoCaptureImplNotInStartedState:
      return media::mojom::VideoCaptureFrameDropReason::
          kVideoCaptureImplNotInStartedState;
    case media::VideoCaptureFrameDropReason::
        kVideoCaptureImplFailedToWrapDataAsMediaVideoFrame:
      return media::mojom::VideoCaptureFrameDropReason::
          kVideoCaptureImplFailedToWrapDataAsMediaVideoFrame;
    case media::VideoCaptureFrameDropReason::
        kVideoTrackAdapterHasNoResolutionAdapters:
      return media::mojom::VideoCaptureFrameDropReason::
          kVideoTrackAdapterHasNoResolutionAdapters;
    case media::VideoCaptureFrameDropReason::kResolutionAdapterFrameIsNotValid:
      return media::mojom::VideoCaptureFrameDropReason::
          kResolutionAdapterFrameIsNotValid;
    case media::VideoCaptureFrameDropReason::
        kResolutionAdapterWrappingFrameForCroppingFailed:
      return media::mojom::VideoCaptureFrameDropReason::
          kResolutionAdapterWrappingFrameForCroppingFailed;
    case media::VideoCaptureFrameDropReason::
        kResolutionAdapterFrameRateIsHigherThanRequested:
      return media::mojom::VideoCaptureFrameDropReason::
          kResolutionAdapterFrameRateIsHigherThanRequested;
    case media::VideoCaptureFrameDropReason::kResolutionAdapterHasNoCallbacks:
      return media::mojom::VideoCaptureFrameDropReason::
          kResolutionAdapterHasNoCallbacks;
    case media::VideoCaptureFrameDropReason::
        kVideoTrackFrameDelivererNotEnabledReplacingWithBlackFrame:
      return media::mojom::VideoCaptureFrameDropReason::
          kVideoTrackFrameDelivererNotEnabledReplacingWithBlackFrame;
    case media::VideoCaptureFrameDropReason::
        kRendererSinkFrameDelivererIsNotStarted:
      return media::mojom::VideoCaptureFrameDropReason::
          kRendererSinkFrameDelivererIsNotStarted;
    case media::VideoCaptureFrameDropReason::kCropVersionNotCurrent_DEPRECATED:
      return media::mojom::VideoCaptureFrameDropReason::
          kCropVersionNotCurrent_DEPRECATED;
    case media::VideoCaptureFrameDropReason::kGpuMemoryBufferMapFailed:
      return media::mojom::VideoCaptureFrameDropReason::
          kGpuMemoryBufferMapFailed;
    case media::VideoCaptureFrameDropReason::
        kSubCaptureTargetVersionNotCurrent_DEPRECATED:
      return media::mojom::VideoCaptureFrameDropReason::
          kSubCaptureTargetVersionNotCurrent_DEPRECATED;
    case media::VideoCaptureFrameDropReason::kPostProcessingFailed:
      return media::mojom::VideoCaptureFrameDropReason::kPostProcessingFailed;
    case media::VideoCaptureFrameDropReason::
        kResolutionAdapterFrameIsNotMappable:
      return media::mojom::VideoCaptureFrameDropReason::
          kResolutionAdapterFrameIsNotMappable;
    case media::VideoCaptureFrameDropReason::
        kResolutionAdapterCannotCreateConvertFrame:
      return media::mojom::VideoCaptureFrameDropReason::
          kResolutionAdapterCannotCreateConvertFrame;
    case media::VideoCaptureFrameDropReason::
        kResolutionAdapterConvertAndScaleFailed:
      return media::mojom::VideoCaptureFrameDropReason::
          kResolutionAdapterConvertAndScaleFailed;
    case media::VideoCaptureFrameDropReason::kOldCaptureVersion:
      return media::mojom::VideoCaptureFrameDropReason::kOldCaptureVersion;
  }
  NOTREACHED();
}

// static
media::VideoCaptureFrameDropReason
EnumTraits<media::mojom::VideoCaptureFrameDropReason,
           media::VideoCaptureFrameDropReason>::
    FromMojom(media::mojom::VideoCaptureFrameDropReason input) {
  switch (input) {
    case media::mojom::VideoCaptureFrameDropReason::kNone:
      return media::VideoCaptureFrameDropReason::kNone;
    case media::mojom::VideoCaptureFrameDropReason::
        kDeviceClientFrameHasInvalidFormat:
      return media::VideoCaptureFrameDropReason::
          kDeviceClientFrameHasInvalidFormat;
    case media::mojom::VideoCaptureFrameDropReason::
        kDeviceClientLibyuvConvertToI420Failed:
      return media::VideoCaptureFrameDropReason::
          kDeviceClientLibyuvConvertToI420Failed;
    case media::mojom::VideoCaptureFrameDropReason::kV4L2BufferErrorFlagWasSet:
      return media::VideoCaptureFrameDropReason::kV4L2BufferErrorFlagWasSet;
    case media::mojom::VideoCaptureFrameDropReason::
        kV4L2InvalidNumberOfBytesInBuffer:
      return media::VideoCaptureFrameDropReason::
          kV4L2InvalidNumberOfBytesInBuffer;
    case media::mojom::VideoCaptureFrameDropReason::kAndroidThrottling:
      return media::VideoCaptureFrameDropReason::kAndroidThrottling;
    case media::mojom::VideoCaptureFrameDropReason::
        kAndroidGetByteArrayElementsFailed:
      return media::VideoCaptureFrameDropReason::
          kAndroidGetByteArrayElementsFailed;
    case media::mojom::VideoCaptureFrameDropReason::
        kAndroidApi1UnexpectedDataLength:
      return media::VideoCaptureFrameDropReason::
          kAndroidApi1UnexpectedDataLength;
    case media::mojom::VideoCaptureFrameDropReason::
        kAndroidApi2AcquiredImageIsNull:
      return media::VideoCaptureFrameDropReason::
          kAndroidApi2AcquiredImageIsNull;
    case media::mojom::VideoCaptureFrameDropReason::
        kWinDirectShowUnexpectedSampleLength:
      return media::VideoCaptureFrameDropReason::
          kWinDirectShowUnexpectedSampleLength;
    case media::mojom::VideoCaptureFrameDropReason::
        kWinDirectShowFailedToGetMemoryPointerFromMediaSample:
      return media::VideoCaptureFrameDropReason::
          kWinDirectShowFailedToGetMemoryPointerFromMediaSample;
    case media::mojom::VideoCaptureFrameDropReason::
        kWinMediaFoundationReceivedSampleIsNull:
      return media::VideoCaptureFrameDropReason::
          kWinMediaFoundationReceivedSampleIsNull;
    case media::mojom::VideoCaptureFrameDropReason::
        kWinMediaFoundationLockingBufferDelieveredNullptr:
      return media::VideoCaptureFrameDropReason::
          kWinMediaFoundationLockingBufferDelieveredNullptr;
    case media::mojom::VideoCaptureFrameDropReason::
        kWinMediaFoundationGetBufferByIndexReturnedNull:
      return media::VideoCaptureFrameDropReason::
          kWinMediaFoundationGetBufferByIndexReturnedNull;
    case media::mojom::VideoCaptureFrameDropReason::
        kBufferPoolMaxBufferCountExceeded:
      return media::VideoCaptureFrameDropReason::
          kBufferPoolMaxBufferCountExceeded;
    case media::mojom::VideoCaptureFrameDropReason::
        kBufferPoolBufferAllocationFailed:
      return media::VideoCaptureFrameDropReason::
          kBufferPoolBufferAllocationFailed;
    case media::mojom::VideoCaptureFrameDropReason::
        kVideoCaptureImplNotInStartedState:
      return media::VideoCaptureFrameDropReason::
          kVideoCaptureImplNotInStartedState;
    case media::mojom::VideoCaptureFrameDropReason::
        kVideoCaptureImplFailedToWrapDataAsMediaVideoFrame:
      return media::VideoCaptureFrameDropReason::
          kVideoCaptureImplFailedToWrapDataAsMediaVideoFrame;
    case media::mojom::VideoCaptureFrameDropReason::
        kVideoTrackAdapterHasNoResolutionAdapters:
      return media::VideoCaptureFrameDropReason::
          kVideoTrackAdapterHasNoResolutionAdapters;
    case media::mojom::VideoCaptureFrameDropReason::
        kResolutionAdapterFrameIsNotValid:
      return media::VideoCaptureFrameDropReason::
          kResolutionAdapterFrameIsNotValid;
    case media::mojom::VideoCaptureFrameDropReason::
        kResolutionAdapterWrappingFrameForCroppingFailed:
      return media::VideoCaptureFrameDropReason::
          kResolutionAdapterWrappingFrameForCroppingFailed;
    case media::mojom::VideoCaptureFrameDropReason::
        kResolutionAdapterTimestampTooCloseToPrevious_DEPRECATED:
      NOTREACHED();
    case media::mojom::VideoCaptureFrameDropReason::
        kResolutionAdapterFrameRateIsHigherThanRequested:
      return media::VideoCaptureFrameDropReason::
          kResolutionAdapterFrameRateIsHigherThanRequested;
    case media::mojom::VideoCaptureFrameDropReason::
        kResolutionAdapterHasNoCallbacks:
      return media::VideoCaptureFrameDropReason::
          kResolutionAdapterHasNoCallbacks;
    case media::mojom::VideoCaptureFrameDropReason::
        kVideoTrackFrameDelivererNotEnabledReplacingWithBlackFrame:
      return media::VideoCaptureFrameDropReason::
          kVideoTrackFrameDelivererNotEnabledReplacingWithBlackFrame;
    case media::mojom::VideoCaptureFrameDropReason::
        kRendererSinkFrameDelivererIsNotStarted:
      return media::VideoCaptureFrameDropReason::
          kRendererSinkFrameDelivererIsNotStarted;
    case media::mojom::VideoCaptureFrameDropReason::
        kCropVersionNotCurrent_DEPRECATED:
      return media::VideoCaptureFrameDropReason::
          kCropVersionNotCurrent_DEPRECATED;
    case media::mojom::VideoCaptureFrameDropReason::kGpuMemoryBufferMapFailed:
      return media::VideoCaptureFrameDropReason::kGpuMemoryBufferMapFailed;
    case media::mojom::VideoCaptureFrameDropReason::
        kSubCaptureTargetVersionNotCurrent_DEPRECATED:
      return media::VideoCaptureFrameDropReason::
          kSubCaptureTargetVersionNotCurrent_DEPRECATED;
    case media::mojom::VideoCaptureFrameDropReason::kPostProcessingFailed:
      return media::VideoCaptureFrameDropReason::kPostProcessingFailed;
    case media::mojom::VideoCaptureFrameDropReason::
        kResolutionAdapterFrameIsNotMappable:
      return media::VideoCaptureFrameDropReason::
          kResolutionAdapterFrameIsNotMappable;
    case media::mojom::VideoCaptureFrameDropReason::
        kResolutionAdapterCannotCreateConvertFrame:
      return media::VideoCaptureFrameDropReason::
          kResolutionAdapterCannotCreateConvertFrame;
    case media::mojom::VideoCaptureFrameDropReason::
        kResolutionAdapterConvertAndScaleFailed:
      return media::VideoCaptureFrameDropReason::
          kResolutionAdapterConvertAndScaleFailed;
    case media::mojom::VideoCaptureFrameDropReason::kOldCaptureVersion:
      return media::VideoCaptureFrameDropReason::kOldCaptureVersion;
  }
  NOTREACHED();
}

// static
media::mojom::VideoFacingMode
EnumTraits<media::mojom::VideoFacingMode, media::VideoFacingMode>::ToMojom(
    media::VideoFacingMode input) {
  switch (input) {
    case media::VideoFacingMode::MEDIA_VIDEO_FACING_NONE:
      return media::mojom::VideoFacingMode::NONE;
    case media::VideoFacingMode::MEDIA_VIDEO_FACING_USER:
      return media::mojom::VideoFacingMode::USER;
    case media::VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT:
      return media::mojom::VideoFacingMode::ENVIRONMENT;
    case media::VideoFacingMode::NUM_MEDIA_VIDEO_FACING_MODES:
      NOTREACHED();
  }
  NOTREACHED();
}

// static
media::VideoFacingMode
EnumTraits<media::mojom::VideoFacingMode, media::VideoFacingMode>::FromMojom(
    media::mojom::VideoFacingMode input) {
  switch (input) {
    case media::mojom::VideoFacingMode::NONE:
      return media::VideoFacingMode::MEDIA_VIDEO_FACING_NONE;
    case media::mojom::VideoFacingMode::USER:
      return media::VideoFacingMode::MEDIA_VIDEO_FACING_USER;
    case media::mojom::VideoFacingMode::ENVIRONMENT:
      return media::VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT;
  }
  NOTREACHED();
}

// static
media::mojom::VideoCaptureApi
EnumTraits<media::mojom::VideoCaptureApi, media::VideoCaptureApi>::ToMojom(
    media::VideoCaptureApi input) {
  switch (input) {
    case media::VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE:
      return media::mojom::VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE;
    case media::VideoCaptureApi::WIN_MEDIA_FOUNDATION:
      return media::mojom::VideoCaptureApi::WIN_MEDIA_FOUNDATION;
    case media::VideoCaptureApi::WIN_MEDIA_FOUNDATION_SENSOR:
      return media::mojom::VideoCaptureApi::WIN_MEDIA_FOUNDATION_SENSOR;
    case media::VideoCaptureApi::WIN_DIRECT_SHOW:
      return media::mojom::VideoCaptureApi::WIN_DIRECT_SHOW;
    case media::VideoCaptureApi::MACOSX_AVFOUNDATION:
      return media::mojom::VideoCaptureApi::MACOSX_AVFOUNDATION;
    case media::VideoCaptureApi::MACOSX_DECKLINK:
      return media::mojom::VideoCaptureApi::MACOSX_DECKLINK;
    case media::VideoCaptureApi::ANDROID_API1:
      return media::mojom::VideoCaptureApi::ANDROID_API1;
    case media::VideoCaptureApi::ANDROID_API2_LEGACY:
      return media::mojom::VideoCaptureApi::ANDROID_API2_LEGACY;
    case media::VideoCaptureApi::ANDROID_API2_FULL:
      return media::mojom::VideoCaptureApi::ANDROID_API2_FULL;
    case media::VideoCaptureApi::ANDROID_API2_LIMITED:
      return media::mojom::VideoCaptureApi::ANDROID_API2_LIMITED;
    case media::VideoCaptureApi::FUCHSIA_CAMERA3:
      return media::mojom::VideoCaptureApi::FUCHSIA_CAMERA3;
    case media::VideoCaptureApi::VIRTUAL_DEVICE:
      return media::mojom::VideoCaptureApi::VIRTUAL_DEVICE;
    case media::VideoCaptureApi::UNKNOWN:
      return media::mojom::VideoCaptureApi::UNKNOWN;
    case media::VideoCaptureApi::WEBRTC_LINUX_PIPEWIRE_SINGLE_PLANE:
      return media::mojom::VideoCaptureApi::WEBRTC_LINUX_PIPEWIRE_SINGLE_PLANE;
  }
  NOTREACHED();
}

// static
media::mojom::CameraAvailability EnumTraits<
    media::mojom::CameraAvailability,
    media::CameraAvailability>::ToMojom(media::CameraAvailability input) {
  switch (input) {
    case media::CameraAvailability::kAvailable:
      return media::mojom::CameraAvailability::kAvailable;
    case media::CameraAvailability::
        kUnavailableExclusivelyUsedByOtherApplication:
      return media::mojom::CameraAvailability::
          kUnavailableExclusivelyUsedByOtherApplication;
  }
  NOTREACHED();
}

// static
media::VideoCaptureApi
EnumTraits<media::mojom::VideoCaptureApi, media::VideoCaptureApi>::FromMojom(
    media::mojom::VideoCaptureApi input) {
  switch (input) {
    case media::mojom::VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE:
      return media::VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE;
    case media::mojom::VideoCaptureApi::WIN_MEDIA_FOUNDATION:
      return media::VideoCaptureApi::WIN_MEDIA_FOUNDATION;
    case media::mojom::VideoCaptureApi::WIN_MEDIA_FOUNDATION_SENSOR:
      return media::VideoCaptureApi::WIN_MEDIA_FOUNDATION_SENSOR;
    case media::mojom::VideoCaptureApi::WIN_DIRECT_SHOW:
      return media::VideoCaptureApi::WIN_DIRECT_SHOW;
    case media::mojom::VideoCaptureApi::MACOSX_AVFOUNDATION:
      return media::VideoCaptureApi::MACOSX_AVFOUNDATION;
    case media::mojom::VideoCaptureApi::MACOSX_DECKLINK:
      return media::VideoCaptureApi::MACOSX_DECKLINK;
    case media::mojom::VideoCaptureApi::ANDROID_API1:
      return media::VideoCaptureApi::ANDROID_API1;
    case media::mojom::VideoCaptureApi::ANDROID_API2_LEGACY:
      return media::VideoCaptureApi::ANDROID_API2_LEGACY;
    case media::mojom::VideoCaptureApi::ANDROID_API2_FULL:
      return media::VideoCaptureApi::ANDROID_API2_FULL;
    case media::mojom::VideoCaptureApi::ANDROID_API2_LIMITED:
      return media::VideoCaptureApi::ANDROID_API2_LIMITED;
    case media::mojom::VideoCaptureApi::FUCHSIA_CAMERA3:
      return media::VideoCaptureApi::FUCHSIA_CAMERA3;
    case media::mojom::VideoCaptureApi::VIRTUAL_DEVICE:
      return media::VideoCaptureApi::VIRTUAL_DEVICE;
    case media::mojom::VideoCaptureApi::UNKNOWN:
      return media::VideoCaptureApi::UNKNOWN;
    case media::mojom::VideoCaptureApi::WEBRTC_LINUX_PIPEWIRE_SINGLE_PLANE:
      return media::VideoCaptureApi::WEBRTC_LINUX_PIPEWIRE_SINGLE_PLANE;
  }
  NOTREACHED();
}

// static
media::CameraAvailability
EnumTraits<media::mojom::CameraAvailability, media::CameraAvailability>::
    FromMojom(media::mojom::CameraAvailability input) {
  switch (input) {
    case media::mojom::CameraAvailability::kAvailable:
      return media::CameraAvailability::kAvailable;
    case media::mojom::CameraAvailability::
        kUnavailableExclusivelyUsedByOtherApplication:
      return media::CameraAvailability::
          kUnavailableExclusivelyUsedByOtherApplication;
  }
  NOTREACHED();
}

// static
media::mojom::VideoCaptureTransportType EnumTraits<
    media::mojom::VideoCaptureTransportType,
    media::VideoCaptureTransportType>::ToMojom(media::VideoCaptureTransportType
                                                   input) {
  switch (input) {
    case media::VideoCaptureTransportType::APPLE_USB_OR_BUILT_IN:
      return media::mojom::VideoCaptureTransportType::APPLE_USB_OR_BUILT_IN;
    case media::VideoCaptureTransportType::OTHER_TRANSPORT:
      return media::mojom::VideoCaptureTransportType::OTHER_TRANSPORT;
  }
  NOTREACHED();
}

// static
media::VideoCaptureTransportType
EnumTraits<media::mojom::VideoCaptureTransportType,
           media::VideoCaptureTransportType>::
    FromMojom(media::mojom::VideoCaptureTransportType input) {
  switch (input) {
    case media::mojom::VideoCaptureTransportType::APPLE_USB_OR_BUILT_IN:
      return media::VideoCaptureTransportType::APPLE_USB_OR_BUILT_IN;
    case media::mojom::VideoCaptureTransportType::OTHER_TRANSPORT:
      return media::VideoCaptureTransportType::OTHER_TRANSPORT;
  }
  NOTREACHED();
}

// static
bool StructTraits<media::mojom::VideoCaptureControlSupportDataView,
                  media::VideoCaptureControlSupport>::
    Read(media::mojom::VideoCaptureControlSupportDataView data,
         media::VideoCaptureControlSupport* out) {
  out->pan = data.pan();
  out->tilt = data.tilt();
  out->zoom = data.zoom();
  return true;
}

// static
bool StructTraits<media::mojom::VideoCaptureFormatDataView,
                  media::VideoCaptureFormat>::
    Read(media::mojom::VideoCaptureFormatDataView data,
         media::VideoCaptureFormat* out) {
  if (!data.ReadFrameSize(&out->frame_size))
    return false;
  out->frame_rate = data.frame_rate();
  if (!data.ReadPixelFormat(&out->pixel_format))
    return false;
  return true;
}

// static
bool StructTraits<media::mojom::VideoCaptureParamsDataView,
                  media::VideoCaptureParams>::
    Read(media::mojom::VideoCaptureParamsDataView data,
         media::VideoCaptureParams* out) {
  if (!data.ReadRequestedFormat(&out->requested_format))
    return false;
  if (!data.ReadBufferType(&out->buffer_type))
    return false;
  if (!data.ReadResolutionChangePolicy(&out->resolution_change_policy))
    return false;
  if (!data.ReadPowerLineFrequency(&out->power_line_frequency))
    return false;
  if (!data.ReadRequestType(&out->request_type)) {
    return false;
  }
  out->enable_face_detection = data.enable_face_detection();
  out->is_high_dpi_enabled = data.is_high_dpi_enabled();
  out->capture_version_source = data.capture_version_source();
  return true;
}

// static
bool StructTraits<media::mojom::VideoCaptureDeviceDescriptorDataView,
                  media::VideoCaptureDeviceDescriptor>::
    Read(media::mojom::VideoCaptureDeviceDescriptorDataView data,
         media::VideoCaptureDeviceDescriptor* output) {
  std::string display_name;
  if (!data.ReadDisplayName(&display_name))
    return false;
  output->set_display_name(display_name);
  if (!data.ReadDeviceId(&(output->device_id)))
    return false;
  if (!data.ReadModelId(&(output->model_id)))
    return false;
  if (!data.ReadFacingMode(&(output->facing)))
    return false;
  if (!data.ReadAvailability(&(output->availability))) {
    return false;
  }
  if (!data.ReadCaptureApi(&(output->capture_api)))
    return false;
  media::VideoCaptureControlSupport control_support;
  if (!data.ReadControlSupport(&control_support))
    return false;
  output->set_control_support(control_support);
  if (!data.ReadTransportType(&(output->transport_type)))
    return false;
  return true;
}

// static
bool StructTraits<media::mojom::VideoCaptureDeviceInfoDataView,
                  media::VideoCaptureDeviceInfo>::
    Read(media::mojom::VideoCaptureDeviceInfoDataView data,
         media::VideoCaptureDeviceInfo* output) {
  if (!data.ReadDescriptor(&(output->descriptor)))
    return false;
  if (!data.ReadSupportedFormats(&(output->supported_formats)))
    return false;
  return true;
}

// static
bool StructTraits<media::mojom::VideoCaptureFeedbackDataView,
                  media::VideoCaptureFeedback>::
    Read(media::mojom::VideoCaptureFeedbackDataView data,
         media::VideoCaptureFeedback* output) {
  output->max_framerate_fps = data.max_framerate_fps();
  output->max_pixels = data.max_pixels();
  output->resource_utilization = data.resource_utilization();
  output->require_mapped_frame = data.require_mapped_frame();
  output->frame_id = data.frame_id();
  return true;
}

}  // namespace mojo
