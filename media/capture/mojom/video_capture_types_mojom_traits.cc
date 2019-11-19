// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/mojom/video_capture_types_mojom_traits.h"

#include "media/base/ipc/media_param_traits_macros.h"
#include "ui/gfx/geometry/mojom/geometry.mojom.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

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
  return media::mojom::ResolutionChangePolicy::FIXED_RESOLUTION;
}

// static
bool EnumTraits<media::mojom::ResolutionChangePolicy,
                media::ResolutionChangePolicy>::
    FromMojom(media::mojom::ResolutionChangePolicy input,
              media::ResolutionChangePolicy* output) {
  switch (input) {
    case media::mojom::ResolutionChangePolicy::FIXED_RESOLUTION:
      *output = media::ResolutionChangePolicy::FIXED_RESOLUTION;
      return true;
    case media::mojom::ResolutionChangePolicy::FIXED_ASPECT_RATIO:
      *output = media::ResolutionChangePolicy::FIXED_ASPECT_RATIO;
      return true;
    case media::mojom::ResolutionChangePolicy::ANY_WITHIN_LIMIT:
      *output = media::ResolutionChangePolicy::ANY_WITHIN_LIMIT;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
media::mojom::PowerLineFrequency EnumTraits<
    media::mojom::PowerLineFrequency,
    media::PowerLineFrequency>::ToMojom(media::PowerLineFrequency input) {
  switch (input) {
    case media::PowerLineFrequency::FREQUENCY_DEFAULT:
      return media::mojom::PowerLineFrequency::DEFAULT;
    case media::PowerLineFrequency::FREQUENCY_50HZ:
      return media::mojom::PowerLineFrequency::HZ_50;
    case media::PowerLineFrequency::FREQUENCY_60HZ:
      return media::mojom::PowerLineFrequency::HZ_60;
  }
  NOTREACHED();
  return media::mojom::PowerLineFrequency::DEFAULT;
}

// static
bool EnumTraits<media::mojom::PowerLineFrequency, media::PowerLineFrequency>::
    FromMojom(media::mojom::PowerLineFrequency input,
              media::PowerLineFrequency* output) {
  switch (input) {
    case media::mojom::PowerLineFrequency::DEFAULT:
      *output = media::PowerLineFrequency::FREQUENCY_DEFAULT;
      return true;
    case media::mojom::PowerLineFrequency::HZ_50:
      *output = media::PowerLineFrequency::FREQUENCY_50HZ;
      return true;
    case media::mojom::PowerLineFrequency::HZ_60:
      *output = media::PowerLineFrequency::FREQUENCY_60HZ;
      return true;
  }
  NOTREACHED();
  return false;
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
    case media::VideoPixelFormat::PIXEL_FORMAT_YUV420P9:
      return media::mojom::VideoCapturePixelFormat::YUV420P9;
    case media::VideoPixelFormat::PIXEL_FORMAT_YUV420P10:
      return media::mojom::VideoCapturePixelFormat::YUV420P10;
    case media::VideoPixelFormat::PIXEL_FORMAT_YUV422P9:
      return media::mojom::VideoCapturePixelFormat::YUV422P9;
    case media::VideoPixelFormat::PIXEL_FORMAT_YUV422P10:
      return media::mojom::VideoCapturePixelFormat::YUV422P10;
    case media::VideoPixelFormat::PIXEL_FORMAT_YUV444P9:
      return media::mojom::VideoCapturePixelFormat::YUV444P9;
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
    case media::VideoPixelFormat::PIXEL_FORMAT_P016LE:
      return media::mojom::VideoCapturePixelFormat::P016LE;
    case media::VideoPixelFormat::PIXEL_FORMAT_XR30:
      return media::mojom::VideoCapturePixelFormat::XR30;
    case media::VideoPixelFormat::PIXEL_FORMAT_XB30:
      return media::mojom::VideoCapturePixelFormat::XB30;
  }
  NOTREACHED();
  return media::mojom::VideoCapturePixelFormat::I420;
}

// static
bool EnumTraits<media::mojom::VideoCapturePixelFormat,
                media::VideoPixelFormat>::
    FromMojom(media::mojom::VideoCapturePixelFormat input,
              media::VideoPixelFormat* output) {
  switch (input) {
    case media::mojom::VideoCapturePixelFormat::UNKNOWN:
      *output = media::PIXEL_FORMAT_UNKNOWN;
      return true;
    case media::mojom::VideoCapturePixelFormat::I420:
      *output = media::PIXEL_FORMAT_I420;
      return true;
    case media::mojom::VideoCapturePixelFormat::YV12:
      *output = media::PIXEL_FORMAT_YV12;
      return true;
    case media::mojom::VideoCapturePixelFormat::I422:
      *output = media::PIXEL_FORMAT_I422;
      return true;
    case media::mojom::VideoCapturePixelFormat::I420A:
      *output = media::PIXEL_FORMAT_I420A;
      return true;
    case media::mojom::VideoCapturePixelFormat::I444:
      *output = media::PIXEL_FORMAT_I444;
      return true;
    case media::mojom::VideoCapturePixelFormat::NV12:
      *output = media::PIXEL_FORMAT_NV12;
      return true;
    case media::mojom::VideoCapturePixelFormat::NV21:
      *output = media::PIXEL_FORMAT_NV21;
      return true;
    case media::mojom::VideoCapturePixelFormat::YUY2:
      *output = media::PIXEL_FORMAT_YUY2;
      return true;
    case media::mojom::VideoCapturePixelFormat::ARGB:
      *output = media::PIXEL_FORMAT_ARGB;
      return true;
    case media::mojom::VideoCapturePixelFormat::XRGB:
      *output = media::PIXEL_FORMAT_XRGB;
      return true;
    case media::mojom::VideoCapturePixelFormat::RGB24:
      *output = media::PIXEL_FORMAT_RGB24;
      return true;
    case media::mojom::VideoCapturePixelFormat::MJPEG:
      *output = media::PIXEL_FORMAT_MJPEG;
      return true;
    case media::mojom::VideoCapturePixelFormat::YUV420P9:
      *output = media::PIXEL_FORMAT_YUV420P9;
      return true;
    case media::mojom::VideoCapturePixelFormat::YUV420P10:
      *output = media::PIXEL_FORMAT_YUV420P10;
      return true;
    case media::mojom::VideoCapturePixelFormat::YUV422P9:
      *output = media::PIXEL_FORMAT_YUV422P9;
      return true;
    case media::mojom::VideoCapturePixelFormat::YUV422P10:
      *output = media::PIXEL_FORMAT_YUV422P10;
      return true;
    case media::mojom::VideoCapturePixelFormat::YUV444P9:
      *output = media::PIXEL_FORMAT_YUV444P9;
      return true;
    case media::mojom::VideoCapturePixelFormat::YUV444P10:
      *output = media::PIXEL_FORMAT_YUV444P10;
      return true;
    case media::mojom::VideoCapturePixelFormat::YUV420P12:
      *output = media::PIXEL_FORMAT_YUV420P12;
      return true;
    case media::mojom::VideoCapturePixelFormat::YUV422P12:
      *output = media::PIXEL_FORMAT_YUV422P12;
      return true;
    case media::mojom::VideoCapturePixelFormat::YUV444P12:
      *output = media::PIXEL_FORMAT_YUV444P12;
      return true;
    case media::mojom::VideoCapturePixelFormat::Y16:
      *output = media::PIXEL_FORMAT_Y16;
      return true;
    case media::mojom::VideoCapturePixelFormat::ABGR:
      *output = media::PIXEL_FORMAT_ABGR;
      return true;
    case media::mojom::VideoCapturePixelFormat::XBGR:
      *output = media::PIXEL_FORMAT_XBGR;
      return true;
    case media::mojom::VideoCapturePixelFormat::P016LE:
      *output = media::PIXEL_FORMAT_P016LE;
      return true;
    case media::mojom::VideoCapturePixelFormat::XR30:
      *output = media::PIXEL_FORMAT_XR30;
      return true;
    case media::mojom::VideoCapturePixelFormat::XB30:
      *output = media::PIXEL_FORMAT_XB30;
      return true;
    case media::mojom::VideoCapturePixelFormat::BGRA:
      *output = media::PIXEL_FORMAT_BGRA;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
media::mojom::VideoCaptureBufferType
EnumTraits<media::mojom::VideoCaptureBufferType,
           media::VideoCaptureBufferType>::ToMojom(media::VideoCaptureBufferType
                                                       input) {
  switch (input) {
    case media::VideoCaptureBufferType::kSharedMemory:
      return media::mojom::VideoCaptureBufferType::kSharedMemory;
    case media::VideoCaptureBufferType::kSharedMemoryViaRawFileDescriptor:
      return media::mojom::VideoCaptureBufferType::
          kSharedMemoryViaRawFileDescriptor;
    case media::VideoCaptureBufferType::kMailboxHolder:
      return media::mojom::VideoCaptureBufferType::kMailboxHolder;
    case media::VideoCaptureBufferType::kGpuMemoryBuffer:
      return media::mojom::VideoCaptureBufferType::kGpuMemoryBuffer;
  }
  NOTREACHED();
  return media::mojom::VideoCaptureBufferType::kSharedMemory;
}

// static
bool EnumTraits<media::mojom::VideoCaptureBufferType,
                media::VideoCaptureBufferType>::
    FromMojom(media::mojom::VideoCaptureBufferType input,
              media::VideoCaptureBufferType* output) {
  switch (input) {
    case media::mojom::VideoCaptureBufferType::kSharedMemory:
      *output = media::VideoCaptureBufferType::kSharedMemory;
      return true;
    case media::mojom::VideoCaptureBufferType::
        kSharedMemoryViaRawFileDescriptor:
      *output =
          media::VideoCaptureBufferType::kSharedMemoryViaRawFileDescriptor;
      return true;
    case media::mojom::VideoCaptureBufferType::kMailboxHolder:
      *output = media::VideoCaptureBufferType::kMailboxHolder;
      return true;
    case media::mojom::VideoCaptureBufferType::kGpuMemoryBuffer:
      *output = media::VideoCaptureBufferType::kGpuMemoryBuffer;
      return true;
  }
  NOTREACHED();
  return false;
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
        kFrameSinkVideoCaptureDeviceAleradyEndedOnFatalError:
      return media::mojom::VideoCaptureError::
          kFrameSinkVideoCaptureDeviceAleradyEndedOnFatalError;
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
        kCrosHalV3BufferManagerFailedToCreateGpuMemoryBuffer:
      return media::mojom::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToCreateGpuMemoryBuffer;
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
  }
  NOTREACHED();
  return media::mojom::VideoCaptureError::kNone;
}

// static
bool EnumTraits<media::mojom::VideoCaptureError, media::VideoCaptureError>::
    FromMojom(media::mojom::VideoCaptureError input,
              media::VideoCaptureError* output) {
  switch (input) {
    case media::mojom::VideoCaptureError::kNone:
      *output = media::VideoCaptureError::kNone;
      return true;
    case media::mojom::VideoCaptureError::
        kVideoCaptureControllerInvalidOrUnsupportedVideoCaptureParametersRequested:
      *output = media::VideoCaptureError::
          kVideoCaptureControllerInvalidOrUnsupportedVideoCaptureParametersRequested;
      return true;
    case media::mojom::VideoCaptureError::
        kVideoCaptureControllerIsAlreadyInErrorState:
      *output = media::VideoCaptureError::
          kVideoCaptureControllerIsAlreadyInErrorState;
      return true;
    case media::mojom::VideoCaptureError::
        kVideoCaptureManagerDeviceConnectionLost:
      *output =
          media::VideoCaptureError::kVideoCaptureManagerDeviceConnectionLost;
      return true;
    case media::mojom::VideoCaptureError::
        kFrameSinkVideoCaptureDeviceAleradyEndedOnFatalError:
      *output = media::VideoCaptureError::
          kFrameSinkVideoCaptureDeviceAleradyEndedOnFatalError;
      return true;
    case media::mojom::VideoCaptureError::
        kFrameSinkVideoCaptureDeviceEncounteredFatalError:
      *output = media::VideoCaptureError::
          kFrameSinkVideoCaptureDeviceEncounteredFatalError;
      return true;
    case media::mojom::VideoCaptureError::kV4L2FailedToOpenV4L2DeviceDriverFile:
      *output = media::VideoCaptureError::kV4L2FailedToOpenV4L2DeviceDriverFile;
      return true;
    case media::mojom::VideoCaptureError::kV4L2ThisIsNotAV4L2VideoCaptureDevice:
      *output = media::VideoCaptureError::kV4L2ThisIsNotAV4L2VideoCaptureDevice;
      return true;
    case media::mojom::VideoCaptureError::
        kV4L2FailedToFindASupportedCameraFormat:
      *output =
          media::VideoCaptureError::kV4L2FailedToFindASupportedCameraFormat;
      return true;
    case media::mojom::VideoCaptureError::kV4L2FailedToSetVideoCaptureFormat:
      *output = media::VideoCaptureError::kV4L2FailedToSetVideoCaptureFormat;
      return true;
    case media::mojom::VideoCaptureError::kV4L2UnsupportedPixelFormat:
      *output = media::VideoCaptureError::kV4L2UnsupportedPixelFormat;
      return true;
    case media::mojom::VideoCaptureError::kV4L2FailedToSetCameraFramerate:
      *output = media::VideoCaptureError::kV4L2FailedToSetCameraFramerate;
      return true;
    case media::mojom::VideoCaptureError::kV4L2ErrorRequestingMmapBuffers:
      *output = media::VideoCaptureError::kV4L2ErrorRequestingMmapBuffers;
      return true;
    case media::mojom::VideoCaptureError::kV4L2AllocateBufferFailed:
      *output = media::VideoCaptureError::kV4L2AllocateBufferFailed;
      return true;
    case media::mojom::VideoCaptureError::kV4L2VidiocStreamonFailed:
      *output = media::VideoCaptureError::kV4L2VidiocStreamonFailed;
      return true;
    case media::mojom::VideoCaptureError::kV4L2VidiocStreamoffFailed:
      *output = media::VideoCaptureError::kV4L2VidiocStreamoffFailed;
      return true;
    case media::mojom::VideoCaptureError::kV4L2FailedToVidiocReqbufsWithCount0:
      *output = media::VideoCaptureError::kV4L2FailedToVidiocReqbufsWithCount0;
      return true;
    case media::mojom::VideoCaptureError::kV4L2PollFailed:
      *output = media::VideoCaptureError::kV4L2PollFailed;
      return true;
    case media::mojom::VideoCaptureError::
        kV4L2MultipleContinuousTimeoutsWhileReadPolling:
      *output = media::VideoCaptureError::
          kV4L2MultipleContinuousTimeoutsWhileReadPolling;
      return true;
    case media::mojom::VideoCaptureError::kV4L2FailedToDequeueCaptureBuffer:
      *output = media::VideoCaptureError::kV4L2FailedToDequeueCaptureBuffer;
      return true;
    case media::mojom::VideoCaptureError::kV4L2FailedToEnqueueCaptureBuffer:
      *output = media::VideoCaptureError::kV4L2FailedToEnqueueCaptureBuffer;
      return true;
    case media::mojom::VideoCaptureError::
        kSingleClientVideoCaptureHostLostConnectionToDevice:
      *output = media::VideoCaptureError::
          kSingleClientVideoCaptureHostLostConnectionToDevice;
      return true;
    case media::mojom::VideoCaptureError::
        kSingleClientVideoCaptureDeviceLaunchAborted:
      *output = media::VideoCaptureError::
          kSingleClientVideoCaptureDeviceLaunchAborted;
      return true;
    case media::mojom::VideoCaptureError::
        kDesktopCaptureDeviceWebrtcDesktopCapturerHasFailed:
      *output = media::VideoCaptureError::
          kDesktopCaptureDeviceWebrtcDesktopCapturerHasFailed;
      return true;
    case media::mojom::VideoCaptureError::
        kFileVideoCaptureDeviceCouldNotOpenVideoFile:
      *output = media::VideoCaptureError::
          kFileVideoCaptureDeviceCouldNotOpenVideoFile;
      return true;
    case media::mojom::VideoCaptureError::
        kDeviceCaptureLinuxFailedToCreateVideoCaptureDelegate:
      *output = media::VideoCaptureError::
          kDeviceCaptureLinuxFailedToCreateVideoCaptureDelegate;
      return true;
    case media::mojom::VideoCaptureError::
        kErrorFakeDeviceIntentionallyEmittingErrorEvent:
      *output = media::VideoCaptureError::
          kErrorFakeDeviceIntentionallyEmittingErrorEvent;
      return true;
    case media::mojom::VideoCaptureError::kDeviceClientTooManyFramesDroppedY16:
      *output = media::VideoCaptureError::kDeviceClientTooManyFramesDroppedY16;
      return true;
    case media::mojom::VideoCaptureError::
        kDeviceMediaToMojoAdapterEncounteredUnsupportedBufferType:
      *output = media::VideoCaptureError::
          kDeviceMediaToMojoAdapterEncounteredUnsupportedBufferType;
      return true;
    case media::mojom::VideoCaptureError::
        kVideoCaptureManagerProcessDeviceStartQueueDeviceInfoNotFound:
      *output = media::VideoCaptureError::
          kVideoCaptureManagerProcessDeviceStartQueueDeviceInfoNotFound;
      return true;
    case media::mojom::VideoCaptureError::
        kInProcessDeviceLauncherFailedToCreateDeviceInstance:
      *output = media::VideoCaptureError::
          kInProcessDeviceLauncherFailedToCreateDeviceInstance;
      return true;
    case media::mojom::VideoCaptureError::
        kServiceDeviceLauncherLostConnectionToDeviceFactoryDuringDeviceStart:
      *output = media::VideoCaptureError::
          kServiceDeviceLauncherLostConnectionToDeviceFactoryDuringDeviceStart;
      return true;
    case media::mojom::VideoCaptureError::
        kServiceDeviceLauncherServiceRespondedWithDeviceNotFound:
      *output = media::VideoCaptureError::
          kServiceDeviceLauncherServiceRespondedWithDeviceNotFound;
      return true;
    case media::mojom::VideoCaptureError::
        kServiceDeviceLauncherConnectionLostWhileWaitingForCallback:
      *output = media::VideoCaptureError::
          kServiceDeviceLauncherConnectionLostWhileWaitingForCallback;
      return true;
    case media::mojom::VideoCaptureError::kIntentionalErrorRaisedByUnitTest:
      *output = media::VideoCaptureError::kIntentionalErrorRaisedByUnitTest;
      return true;
    case media::mojom::VideoCaptureError::kCrosHalV3FailedToStartDeviceThread:
      *output = media::VideoCaptureError::kCrosHalV3FailedToStartDeviceThread;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateMojoConnectionError:
      *output =
          media::VideoCaptureError::kCrosHalV3DeviceDelegateMojoConnectionError;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToGetCameraInfo:
      *output = media::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToGetCameraInfo;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateMissingSensorOrientationInfo:
      *output = media::VideoCaptureError::
          kCrosHalV3DeviceDelegateMissingSensorOrientationInfo;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToOpenCameraDevice:
      *output = media::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToOpenCameraDevice;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToInitializeCameraDevice:
      *output = media::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToInitializeCameraDevice;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToConfigureStreams:
      *output = media::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToConfigureStreams;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateWrongNumberOfStreamsConfigured:
      *output = media::VideoCaptureError::
          kCrosHalV3DeviceDelegateWrongNumberOfStreamsConfigured;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3DeviceDelegateFailedToGetDefaultRequestSettings:
      *output = media::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToGetDefaultRequestSettings;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerHalRequestedTooManyBuffers:
      *output = media::VideoCaptureError::
          kCrosHalV3BufferManagerHalRequestedTooManyBuffers;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToCreateGpuMemoryBuffer:
      *output = media::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToCreateGpuMemoryBuffer;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToMapGpuMemoryBuffer:
      *output = media::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToMapGpuMemoryBuffer;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerUnsupportedVideoPixelFormat:
      *output = media::VideoCaptureError::
          kCrosHalV3BufferManagerUnsupportedVideoPixelFormat;
      return true;
    case media::mojom::VideoCaptureError::kCrosHalV3BufferManagerFailedToDupFd:
      *output = media::VideoCaptureError::kCrosHalV3BufferManagerFailedToDupFd;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToWrapGpuMemoryHandle:
      *output = media::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToWrapGpuMemoryHandle;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToRegisterBuffer:
      *output = media::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToRegisterBuffer;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerProcessCaptureRequestFailed:
      *output = media::VideoCaptureError::
          kCrosHalV3BufferManagerProcessCaptureRequestFailed;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerInvalidPendingResultId:
      *output = media::VideoCaptureError::
          kCrosHalV3BufferManagerInvalidPendingResultId;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerReceivedDuplicatedPartialMetadata:
      *output = media::VideoCaptureError::
          kCrosHalV3BufferManagerReceivedDuplicatedPartialMetadata;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerIncorrectNumberOfOutputBuffersReceived:
      *output = media::VideoCaptureError::
          kCrosHalV3BufferManagerIncorrectNumberOfOutputBuffersReceived;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerInvalidTypeOfOutputBuffersReceived:
      *output = media::VideoCaptureError::
          kCrosHalV3BufferManagerInvalidTypeOfOutputBuffersReceived;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerReceivedMultipleResultBuffersForFrame:
      *output = media::VideoCaptureError::
          kCrosHalV3BufferManagerReceivedMultipleResultBuffersForFrame;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerUnknownStreamInCamera3NotifyMsg:
      *output = media::VideoCaptureError::
          kCrosHalV3BufferManagerUnknownStreamInCamera3NotifyMsg;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerReceivedInvalidShutterTime:
      *output = media::VideoCaptureError::
          kCrosHalV3BufferManagerReceivedInvalidShutterTime;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerFatalDeviceError:
      *output =
          media::VideoCaptureError::kCrosHalV3BufferManagerFatalDeviceError;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerReceivedFrameIsOutOfOrder:
      *output = media::VideoCaptureError::
          kCrosHalV3BufferManagerReceivedFrameIsOutOfOrder;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerFailedToUnwrapReleaseFenceFd:
      *output = media::VideoCaptureError::
          kCrosHalV3BufferManagerFailedToUnwrapReleaseFenceFd;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerSyncWaitOnReleaseFenceTimedOut:
      *output = media::VideoCaptureError::
          kCrosHalV3BufferManagerSyncWaitOnReleaseFenceTimedOut;
      return true;
    case media::mojom::VideoCaptureError::
        kCrosHalV3BufferManagerInvalidJpegBlob:
      *output =
          media::VideoCaptureError::kCrosHalV3BufferManagerInvalidJpegBlob;
      return true;
    case media::mojom::VideoCaptureError::kAndroidFailedToAllocate:
      *output = media::VideoCaptureError::kAndroidFailedToAllocate;
      return true;
    case media::mojom::VideoCaptureError::kAndroidFailedToStartCapture:
      *output = media::VideoCaptureError::kAndroidFailedToStartCapture;
      return true;
    case media::mojom::VideoCaptureError::kAndroidFailedToStopCapture:
      *output = media::VideoCaptureError::kAndroidFailedToStopCapture;
      return true;
    case media::mojom::VideoCaptureError::
        kAndroidApi1CameraErrorCallbackReceived:
      *output =
          media::VideoCaptureError::kAndroidApi1CameraErrorCallbackReceived;
      return true;
    case media::mojom::VideoCaptureError::kAndroidApi2CameraDeviceErrorReceived:
      *output = media::VideoCaptureError::kAndroidApi2CameraDeviceErrorReceived;
      return true;
    case media::mojom::VideoCaptureError::
        kAndroidApi2CaptureSessionConfigureFailed:
      *output =
          media::VideoCaptureError::kAndroidApi2CaptureSessionConfigureFailed;
      return true;
    case media::mojom::VideoCaptureError::
        kAndroidApi2ImageReaderUnexpectedImageFormat:
      *output = media::VideoCaptureError::
          kAndroidApi2ImageReaderUnexpectedImageFormat;
      return true;
    case media::mojom::VideoCaptureError::
        kAndroidApi2ImageReaderSizeDidNotMatchImageSize:
      *output = media::VideoCaptureError::
          kAndroidApi2ImageReaderSizeDidNotMatchImageSize;
      return true;
    case media::mojom::VideoCaptureError::kAndroidApi2ErrorRestartingPreview:
      *output = media::VideoCaptureError::kAndroidApi2ErrorRestartingPreview;
      return true;
    case media::mojom::VideoCaptureError::
        kAndroidScreenCaptureUnsupportedFormat:
      *output =
          media::VideoCaptureError::kAndroidScreenCaptureUnsupportedFormat;
      return true;
    case media::mojom::VideoCaptureError::
        kAndroidScreenCaptureFailedToStartCaptureMachine:
      *output = media::VideoCaptureError::
          kAndroidScreenCaptureFailedToStartCaptureMachine;
      return true;
    case media::mojom::VideoCaptureError::
        kAndroidScreenCaptureTheUserDeniedScreenCapture:
      *output = media::VideoCaptureError::
          kAndroidScreenCaptureTheUserDeniedScreenCapture;
      return true;
    case media::mojom::VideoCaptureError::
        kAndroidScreenCaptureFailedToStartScreenCapture:
      *output = media::VideoCaptureError::
          kAndroidScreenCaptureFailedToStartScreenCapture;
      return true;
    case media::mojom::VideoCaptureError::
        kWinDirectShowCantGetCaptureFormatSettings:
      *output =
          media::VideoCaptureError::kWinDirectShowCantGetCaptureFormatSettings;
      return true;
    case media::mojom::VideoCaptureError::
        kWinDirectShowFailedToGetNumberOfCapabilities:
      *output = media::VideoCaptureError::
          kWinDirectShowFailedToGetNumberOfCapabilities;
      return true;
    case media::mojom::VideoCaptureError::
        kWinDirectShowFailedToGetCaptureDeviceCapabilities:
      *output = media::VideoCaptureError::
          kWinDirectShowFailedToGetCaptureDeviceCapabilities;
      return true;
    case media::mojom::VideoCaptureError::
        kWinDirectShowFailedToSetCaptureDeviceOutputFormat:
      *output = media::VideoCaptureError::
          kWinDirectShowFailedToSetCaptureDeviceOutputFormat;
      return true;
    case media::mojom::VideoCaptureError::
        kWinDirectShowFailedToConnectTheCaptureGraph:
      *output = media::VideoCaptureError::
          kWinDirectShowFailedToConnectTheCaptureGraph;
      return true;
    case media::mojom::VideoCaptureError::
        kWinDirectShowFailedToPauseTheCaptureDevice:
      *output =
          media::VideoCaptureError::kWinDirectShowFailedToPauseTheCaptureDevice;
      return true;
    case media::mojom::VideoCaptureError::
        kWinDirectShowFailedToStartTheCaptureDevice:
      *output =
          media::VideoCaptureError::kWinDirectShowFailedToStartTheCaptureDevice;
      return true;
    case media::mojom::VideoCaptureError::
        kWinDirectShowFailedToStopTheCaptureGraph:
      *output =
          media::VideoCaptureError::kWinDirectShowFailedToStopTheCaptureGraph;
      return true;
    case media::mojom::VideoCaptureError::kWinMediaFoundationEngineIsNull:
      *output = media::VideoCaptureError::kWinMediaFoundationEngineIsNull;
      return true;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationEngineGetSourceFailed:
      *output =
          media::VideoCaptureError::kWinMediaFoundationEngineGetSourceFailed;
      return true;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationFillPhotoCapabilitiesFailed:
      *output = media::VideoCaptureError::
          kWinMediaFoundationFillPhotoCapabilitiesFailed;
      return true;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationFillVideoCapabilitiesFailed:
      *output = media::VideoCaptureError::
          kWinMediaFoundationFillVideoCapabilitiesFailed;
      return true;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationNoVideoCapabilityFound:
      *output =
          media::VideoCaptureError::kWinMediaFoundationNoVideoCapabilityFound;
      return true;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationGetAvailableDeviceMediaTypeFailed:
      *output = media::VideoCaptureError::
          kWinMediaFoundationGetAvailableDeviceMediaTypeFailed;
      return true;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationSetCurrentDeviceMediaTypeFailed:
      *output = media::VideoCaptureError::
          kWinMediaFoundationSetCurrentDeviceMediaTypeFailed;
      return true;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationEngineGetSinkFailed:
      *output =
          media::VideoCaptureError::kWinMediaFoundationEngineGetSinkFailed;
      return true;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationSinkQueryCapturePreviewInterfaceFailed:
      *output = media::VideoCaptureError::
          kWinMediaFoundationSinkQueryCapturePreviewInterfaceFailed;
      return true;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationSinkRemoveAllStreamsFailed:
      *output = media::VideoCaptureError::
          kWinMediaFoundationSinkRemoveAllStreamsFailed;
      return true;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationCreateSinkVideoMediaTypeFailed:
      *output = media::VideoCaptureError::
          kWinMediaFoundationCreateSinkVideoMediaTypeFailed;
      return true;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationConvertToVideoSinkMediaTypeFailed:
      *output = media::VideoCaptureError::
          kWinMediaFoundationConvertToVideoSinkMediaTypeFailed;
      return true;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationSinkAddStreamFailed:
      *output =
          media::VideoCaptureError::kWinMediaFoundationSinkAddStreamFailed;
      return true;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationSinkSetSampleCallbackFailed:
      *output = media::VideoCaptureError::
          kWinMediaFoundationSinkSetSampleCallbackFailed;
      return true;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationEngineStartPreviewFailed:
      *output =
          media::VideoCaptureError::kWinMediaFoundationEngineStartPreviewFailed;
      return true;
    case media::mojom::VideoCaptureError::
        kWinMediaFoundationGetMediaEventStatusFailed:
      *output = media::VideoCaptureError::
          kWinMediaFoundationGetMediaEventStatusFailed;
      return true;
    case media::mojom::VideoCaptureError::kMacSetCaptureDeviceFailed:
      *output = media::VideoCaptureError::kMacSetCaptureDeviceFailed;
      return true;
    case media::mojom::VideoCaptureError::kMacCouldNotStartCaptureDevice:
      *output = media::VideoCaptureError::kMacCouldNotStartCaptureDevice;
      return true;
    case media::mojom::VideoCaptureError::
        kMacReceivedFrameWithUnexpectedResolution:
      *output =
          media::VideoCaptureError::kMacReceivedFrameWithUnexpectedResolution;
      return true;
    case media::mojom::VideoCaptureError::kMacUpdateCaptureResolutionFailed:
      *output = media::VideoCaptureError::kMacUpdateCaptureResolutionFailed;
      return true;
    case media::mojom::VideoCaptureError::
        kMacDeckLinkDeviceIdNotFoundInTheSystem:
      *output =
          media::VideoCaptureError::kMacDeckLinkDeviceIdNotFoundInTheSystem;
      return true;
    case media::mojom::VideoCaptureError::
        kMacDeckLinkErrorQueryingInputInterface:
      *output =
          media::VideoCaptureError::kMacDeckLinkErrorQueryingInputInterface;
      return true;
    case media::mojom::VideoCaptureError::
        kMacDeckLinkErrorCreatingDisplayModeIterator:
      *output = media::VideoCaptureError::
          kMacDeckLinkErrorCreatingDisplayModeIterator;
      return true;
    case media::mojom::VideoCaptureError::kMacDeckLinkCouldNotFindADisplayMode:
      *output = media::VideoCaptureError::kMacDeckLinkCouldNotFindADisplayMode;
      return true;
    case media::mojom::VideoCaptureError::
        kMacDeckLinkCouldNotSelectTheVideoFormatWeLike:
      *output = media::VideoCaptureError::
          kMacDeckLinkCouldNotSelectTheVideoFormatWeLike;
      return true;
    case media::mojom::VideoCaptureError::kMacDeckLinkCouldNotStartCapturing:
      *output = media::VideoCaptureError::kMacDeckLinkCouldNotStartCapturing;
      return true;
    case media::mojom::VideoCaptureError::kMacDeckLinkUnsupportedPixelFormat:
      *output = media::VideoCaptureError::kMacDeckLinkUnsupportedPixelFormat;
      return true;
    case media::mojom::VideoCaptureError::
        kMacAvFoundationReceivedAVCaptureSessionRuntimeErrorNotification:
      *output = media::VideoCaptureError::
          kMacAvFoundationReceivedAVCaptureSessionRuntimeErrorNotification;
      return true;
    case media::mojom::VideoCaptureError::kAndroidApi2ErrorConfiguringCamera:
      *output = media::VideoCaptureError::kAndroidApi2ErrorConfiguringCamera;
      return true;
    case media::mojom::VideoCaptureError::kCrosHalV3DeviceDelegateFailedToFlush:
      *output = media::VideoCaptureError::kCrosHalV3DeviceDelegateFailedToFlush;
      return true;
  }
  NOTREACHED();
  return false;
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
        kResolutionAdapterTimestampTooCloseToPrevious:
      return media::mojom::VideoCaptureFrameDropReason::
          kResolutionAdapterTimestampTooCloseToPrevious;
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
  }
  NOTREACHED();
  return media::mojom::VideoCaptureFrameDropReason::kNone;
}

// static
bool EnumTraits<media::mojom::VideoCaptureFrameDropReason,
                media::VideoCaptureFrameDropReason>::
    FromMojom(media::mojom::VideoCaptureFrameDropReason input,
              media::VideoCaptureFrameDropReason* output) {
  switch (input) {
    case media::mojom::VideoCaptureFrameDropReason::kNone:
      *output = media::VideoCaptureFrameDropReason::kNone;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kDeviceClientFrameHasInvalidFormat:
      *output = media::VideoCaptureFrameDropReason::
          kDeviceClientFrameHasInvalidFormat;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kDeviceClientLibyuvConvertToI420Failed:
      *output = media::VideoCaptureFrameDropReason::
          kDeviceClientLibyuvConvertToI420Failed;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::kV4L2BufferErrorFlagWasSet:
      *output = media::VideoCaptureFrameDropReason::kV4L2BufferErrorFlagWasSet;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kV4L2InvalidNumberOfBytesInBuffer:
      *output =
          media::VideoCaptureFrameDropReason::kV4L2InvalidNumberOfBytesInBuffer;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::kAndroidThrottling:
      *output = media::VideoCaptureFrameDropReason::kAndroidThrottling;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kAndroidGetByteArrayElementsFailed:
      *output = media::VideoCaptureFrameDropReason::
          kAndroidGetByteArrayElementsFailed;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kAndroidApi1UnexpectedDataLength:
      *output =
          media::VideoCaptureFrameDropReason::kAndroidApi1UnexpectedDataLength;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kAndroidApi2AcquiredImageIsNull:
      *output =
          media::VideoCaptureFrameDropReason::kAndroidApi2AcquiredImageIsNull;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kWinDirectShowUnexpectedSampleLength:
      *output = media::VideoCaptureFrameDropReason::
          kWinDirectShowUnexpectedSampleLength;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kWinDirectShowFailedToGetMemoryPointerFromMediaSample:
      *output = media::VideoCaptureFrameDropReason::
          kWinDirectShowFailedToGetMemoryPointerFromMediaSample;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kWinMediaFoundationReceivedSampleIsNull:
      *output = media::VideoCaptureFrameDropReason::
          kWinMediaFoundationReceivedSampleIsNull;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kWinMediaFoundationLockingBufferDelieveredNullptr:
      *output = media::VideoCaptureFrameDropReason::
          kWinMediaFoundationLockingBufferDelieveredNullptr;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kWinMediaFoundationGetBufferByIndexReturnedNull:
      *output = media::VideoCaptureFrameDropReason::
          kWinMediaFoundationGetBufferByIndexReturnedNull;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kBufferPoolMaxBufferCountExceeded:
      *output =
          media::VideoCaptureFrameDropReason::kBufferPoolMaxBufferCountExceeded;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kBufferPoolBufferAllocationFailed:
      *output =
          media::VideoCaptureFrameDropReason::kBufferPoolBufferAllocationFailed;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kVideoCaptureImplNotInStartedState:
      *output = media::VideoCaptureFrameDropReason::
          kVideoCaptureImplNotInStartedState;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kVideoCaptureImplFailedToWrapDataAsMediaVideoFrame:
      *output = media::VideoCaptureFrameDropReason::
          kVideoCaptureImplFailedToWrapDataAsMediaVideoFrame;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kVideoTrackAdapterHasNoResolutionAdapters:
      *output = media::VideoCaptureFrameDropReason::
          kVideoTrackAdapterHasNoResolutionAdapters;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kResolutionAdapterFrameIsNotValid:
      *output =
          media::VideoCaptureFrameDropReason::kResolutionAdapterFrameIsNotValid;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kResolutionAdapterWrappingFrameForCroppingFailed:
      *output = media::VideoCaptureFrameDropReason::
          kResolutionAdapterWrappingFrameForCroppingFailed;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kResolutionAdapterTimestampTooCloseToPrevious:
      *output = media::VideoCaptureFrameDropReason::
          kResolutionAdapterTimestampTooCloseToPrevious;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kResolutionAdapterFrameRateIsHigherThanRequested:
      *output = media::VideoCaptureFrameDropReason::
          kResolutionAdapterFrameRateIsHigherThanRequested;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kResolutionAdapterHasNoCallbacks:
      *output =
          media::VideoCaptureFrameDropReason::kResolutionAdapterHasNoCallbacks;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kVideoTrackFrameDelivererNotEnabledReplacingWithBlackFrame:
      *output = media::VideoCaptureFrameDropReason::
          kVideoTrackFrameDelivererNotEnabledReplacingWithBlackFrame;
      return true;
    case media::mojom::VideoCaptureFrameDropReason::
        kRendererSinkFrameDelivererIsNotStarted:
      *output = media::VideoCaptureFrameDropReason::
          kRendererSinkFrameDelivererIsNotStarted;
      return true;
  }
  NOTREACHED();
  return false;
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
      return media::mojom::VideoFacingMode::NONE;
  }
  NOTREACHED();
  return media::mojom::VideoFacingMode::NONE;
}

// static
bool EnumTraits<media::mojom::VideoFacingMode, media::VideoFacingMode>::
    FromMojom(media::mojom::VideoFacingMode input,
              media::VideoFacingMode* output) {
  switch (input) {
    case media::mojom::VideoFacingMode::NONE:
      *output = media::VideoFacingMode::MEDIA_VIDEO_FACING_NONE;
      return true;
    case media::mojom::VideoFacingMode::USER:
      *output = media::VideoFacingMode::MEDIA_VIDEO_FACING_USER;
      return true;
    case media::mojom::VideoFacingMode::ENVIRONMENT:
      *output = media::VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT;
      return true;
  }
  NOTREACHED();
  return false;
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
    case media::VideoCaptureApi::VIRTUAL_DEVICE:
      return media::mojom::VideoCaptureApi::VIRTUAL_DEVICE;
    case media::VideoCaptureApi::UNKNOWN:
      return media::mojom::VideoCaptureApi::UNKNOWN;
  }
  NOTREACHED();
  return media::mojom::VideoCaptureApi::UNKNOWN;
}

// static
bool EnumTraits<media::mojom::VideoCaptureApi, media::VideoCaptureApi>::
    FromMojom(media::mojom::VideoCaptureApi input,
              media::VideoCaptureApi* output) {
  switch (input) {
    case media::mojom::VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE:
      *output = media::VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE;
      return true;
    case media::mojom::VideoCaptureApi::WIN_MEDIA_FOUNDATION:
      *output = media::VideoCaptureApi::WIN_MEDIA_FOUNDATION;
      return true;
    case media::mojom::VideoCaptureApi::WIN_MEDIA_FOUNDATION_SENSOR:
      *output = media::VideoCaptureApi::WIN_MEDIA_FOUNDATION_SENSOR;
      return true;
    case media::mojom::VideoCaptureApi::WIN_DIRECT_SHOW:
      *output = media::VideoCaptureApi::WIN_DIRECT_SHOW;
      return true;
    case media::mojom::VideoCaptureApi::MACOSX_AVFOUNDATION:
      *output = media::VideoCaptureApi::MACOSX_AVFOUNDATION;
      return true;
    case media::mojom::VideoCaptureApi::MACOSX_DECKLINK:
      *output = media::VideoCaptureApi::MACOSX_DECKLINK;
      return true;
    case media::mojom::VideoCaptureApi::ANDROID_API1:
      *output = media::VideoCaptureApi::ANDROID_API1;
      return true;
    case media::mojom::VideoCaptureApi::ANDROID_API2_LEGACY:
      *output = media::VideoCaptureApi::ANDROID_API2_LEGACY;
      return true;
    case media::mojom::VideoCaptureApi::ANDROID_API2_FULL:
      *output = media::VideoCaptureApi::ANDROID_API2_FULL;
      return true;
    case media::mojom::VideoCaptureApi::ANDROID_API2_LIMITED:
      *output = media::VideoCaptureApi::ANDROID_API2_LIMITED;
      return true;
    case media::mojom::VideoCaptureApi::VIRTUAL_DEVICE:
      *output = media::VideoCaptureApi::VIRTUAL_DEVICE;
      return true;
    case media::mojom::VideoCaptureApi::UNKNOWN:
      *output = media::VideoCaptureApi::UNKNOWN;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
media::mojom::VideoCaptureTransportType EnumTraits<
    media::mojom::VideoCaptureTransportType,
    media::VideoCaptureTransportType>::ToMojom(media::VideoCaptureTransportType
                                                   input) {
  switch (input) {
    case media::VideoCaptureTransportType::MACOSX_USB_OR_BUILT_IN:
      return media::mojom::VideoCaptureTransportType::MACOSX_USB_OR_BUILT_IN;
    case media::VideoCaptureTransportType::OTHER_TRANSPORT:
      return media::mojom::VideoCaptureTransportType::OTHER_TRANSPORT;
  }
  NOTREACHED();
  return media::mojom::VideoCaptureTransportType::OTHER_TRANSPORT;
}

// static
bool EnumTraits<media::mojom::VideoCaptureTransportType,
                media::VideoCaptureTransportType>::
    FromMojom(media::mojom::VideoCaptureTransportType input,
              media::VideoCaptureTransportType* output) {
  switch (input) {
    case media::mojom::VideoCaptureTransportType::MACOSX_USB_OR_BUILT_IN:
      *output = media::VideoCaptureTransportType::MACOSX_USB_OR_BUILT_IN;
      return true;
    case media::mojom::VideoCaptureTransportType::OTHER_TRANSPORT:
      *output = media::VideoCaptureTransportType::OTHER_TRANSPORT;
      return true;
  }
  NOTREACHED();
  return false;
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
  out->enable_face_detection = data.enable_face_detection();
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
  if (!data.ReadCaptureApi(&(output->capture_api)))
    return false;
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

}  // namespace mojo
