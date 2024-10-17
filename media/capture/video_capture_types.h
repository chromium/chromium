// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CAPTURE_TYPES_H_
#define MEDIA_CAPTURE_VIDEO_CAPTURE_TYPES_H_

#include <stddef.h>

#include <vector>

#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "media/base/video_types.h"
#include "media/capture/capture_export.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// TODO(wjia): this type should be defined in a common place and
// shared with device manager.
using VideoCaptureSessionId = base::UnguessableToken;

// Policies for capture devices that have source content that varies in size.
// It is up to the implementation how the captured content will be transformed
// (e.g., scaling and/or letterboxing) in order to produce video frames that
// strictly adheree to one of these policies.
enum class ResolutionChangePolicy {
  // Capture device outputs a fixed resolution all the time. The resolution of
  // the first frame is the resolution for all frames.
  FIXED_RESOLUTION,

  // Capture device is allowed to output frames of varying resolutions. The
  // width and height will not exceed the maximum dimensions specified. The
  // aspect ratio of the frames will match the aspect ratio of the maximum
  // dimensions as closely as possible.
  FIXED_ASPECT_RATIO,

  // Capture device is allowed to output frames of varying resolutions not
  // exceeding the maximum dimensions specified.
  ANY_WITHIN_LIMIT,

  // Must always be equal to largest entry in the enum.
  LAST = ANY_WITHIN_LIMIT,
};

// Potential values of the googPowerLineFrequency optional constraint passed to
// getUserMedia. Note that the numeric values are currently significant, and are
// used to map enum values to corresponding frequency values.
enum class PowerLineFrequency {
  kDefault = 0,
  k50Hz = 50,
  k60Hz = 60,
};

enum class VideoCaptureBufferType {
  kSharedMemory,
  kMailboxHolder,
  kGpuMemoryBuffer
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class VideoCaptureError {
  kNone = 0,
  kVideoCaptureControllerInvalidOrUnsupportedVideoCaptureParametersRequested =
      1,
  kVideoCaptureControllerIsAlreadyInErrorState = 2,
  kVideoCaptureManagerDeviceConnectionLost = 3,
  kFrameSinkVideoCaptureDeviceAlreadyEndedOnFatalError = 4,
  kFrameSinkVideoCaptureDeviceEncounteredFatalError = 5,
  kV4L2FailedToOpenV4L2DeviceDriverFile = 6,
  kV4L2ThisIsNotAV4L2VideoCaptureDevice = 7,
  kV4L2FailedToFindASupportedCameraFormat = 8,
  kV4L2FailedToSetVideoCaptureFormat = 9,
  kV4L2UnsupportedPixelFormat = 10,
  kV4L2FailedToSetCameraFramerate = 11,
  kV4L2ErrorRequestingMmapBuffers = 12,
  kV4L2AllocateBufferFailed = 13,
  kV4L2VidiocStreamonFailed = 14,
  kV4L2VidiocStreamoffFailed = 15,
  kV4L2FailedToVidiocReqbufsWithCount0 = 16,
  kV4L2PollFailed = 17,
  kV4L2MultipleContinuousTimeoutsWhileReadPolling = 18,
  kV4L2FailedToDequeueCaptureBuffer = 19,
  kV4L2FailedToEnqueueCaptureBuffer = 20,
  kSingleClientVideoCaptureHostLostConnectionToDevice = 21,
  kSingleClientVideoCaptureDeviceLaunchAborted = 22,
  kDesktopCaptureDeviceWebrtcDesktopCapturerHasFailed = 23,
  kFileVideoCaptureDeviceCouldNotOpenVideoFile = 24,
  kDeviceCaptureLinuxFailedToCreateVideoCaptureDelegate = 25,
  kErrorFakeDeviceIntentionallyEmittingErrorEvent = 26,
  kDeviceClientTooManyFramesDroppedY16 = 28,
  kDeviceMediaToMojoAdapterEncounteredUnsupportedBufferType = 29,
  kVideoCaptureManagerProcessDeviceStartQueueDeviceInfoNotFound = 30,
  kInProcessDeviceLauncherFailedToCreateDeviceInstance = 31,
  kServiceDeviceLauncherLostConnectionToDeviceFactoryDuringDeviceStart = 32,
  kServiceDeviceLauncherServiceRespondedWithDeviceNotFound = 33,
  kServiceDeviceLauncherConnectionLostWhileWaitingForCallback = 34,
  kIntentionalErrorRaisedByUnitTest = 35,
  kCrosHalV3FailedToStartDeviceThread = 36,
  kCrosHalV3DeviceDelegateMojoConnectionError = 37,
  kCrosHalV3DeviceDelegateFailedToGetCameraInfo = 38,
  kCrosHalV3DeviceDelegateMissingSensorOrientationInfo = 39,
  kCrosHalV3DeviceDelegateFailedToOpenCameraDevice = 40,
  kCrosHalV3DeviceDelegateFailedToInitializeCameraDevice = 41,
  kCrosHalV3DeviceDelegateFailedToConfigureStreams = 42,
  kCrosHalV3DeviceDelegateWrongNumberOfStreamsConfigured = 43,
  kCrosHalV3DeviceDelegateFailedToGetDefaultRequestSettings = 44,
  kCrosHalV3BufferManagerHalRequestedTooManyBuffers = 45,
  kCrosHalV3BufferManagerFailedToCreateGpuMemoryBuffer = 46,
  kCrosHalV3BufferManagerFailedToMapGpuMemoryBuffer = 47,
  kCrosHalV3BufferManagerUnsupportedVideoPixelFormat = 48,
  kCrosHalV3BufferManagerFailedToDupFd = 49,
  kCrosHalV3BufferManagerFailedToWrapGpuMemoryHandle = 50,
  kCrosHalV3BufferManagerFailedToRegisterBuffer = 51,
  kCrosHalV3BufferManagerProcessCaptureRequestFailed = 52,
  kCrosHalV3BufferManagerInvalidPendingResultId = 53,
  kCrosHalV3BufferManagerReceivedDuplicatedPartialMetadata = 54,
  kCrosHalV3BufferManagerIncorrectNumberOfOutputBuffersReceived = 55,
  kCrosHalV3BufferManagerInvalidTypeOfOutputBuffersReceived = 56,
  kCrosHalV3BufferManagerReceivedMultipleResultBuffersForFrame = 57,
  kCrosHalV3BufferManagerUnknownStreamInCamera3NotifyMsg = 58,
  kCrosHalV3BufferManagerReceivedInvalidShutterTime = 59,
  kCrosHalV3BufferManagerFatalDeviceError = 60,
  kCrosHalV3BufferManagerReceivedFrameIsOutOfOrder = 61,
  kCrosHalV3BufferManagerFailedToUnwrapReleaseFenceFd = 62,
  kCrosHalV3BufferManagerSyncWaitOnReleaseFenceTimedOut = 63,
  kCrosHalV3BufferManagerInvalidJpegBlob = 64,
  kAndroidFailedToAllocate = 65,
  kAndroidFailedToStartCapture = 66,
  kAndroidFailedToStopCapture = 67,
  kAndroidApi1CameraErrorCallbackReceived = 68,
  kAndroidApi2CameraDeviceErrorReceived = 69,
  kAndroidApi2CaptureSessionConfigureFailed = 70,
  kAndroidApi2ImageReaderUnexpectedImageFormat = 71,
  kAndroidApi2ImageReaderSizeDidNotMatchImageSize = 72,
  kAndroidApi2ErrorRestartingPreview = 73,
  kAndroidScreenCaptureUnsupportedFormat = 74,
  kAndroidScreenCaptureFailedToStartCaptureMachine = 75,
  kAndroidScreenCaptureTheUserDeniedScreenCapture = 76,
  kAndroidScreenCaptureFailedToStartScreenCapture = 77,
  kWinDirectShowCantGetCaptureFormatSettings = 78,
  kWinDirectShowFailedToGetNumberOfCapabilities = 79,
  kWinDirectShowFailedToGetCaptureDeviceCapabilities = 80,
  kWinDirectShowFailedToSetCaptureDeviceOutputFormat = 81,
  kWinDirectShowFailedToConnectTheCaptureGraph = 82,
  kWinDirectShowFailedToPauseTheCaptureDevice = 83,
  kWinDirectShowFailedToStartTheCaptureDevice = 84,
  kWinDirectShowFailedToStopTheCaptureGraph = 85,
  kWinMediaFoundationEngineIsNull = 86,
  kWinMediaFoundationEngineGetSourceFailed = 87,
  kWinMediaFoundationFillPhotoCapabilitiesFailed = 88,
  kWinMediaFoundationFillVideoCapabilitiesFailed = 89,
  kWinMediaFoundationNoVideoCapabilityFound = 90,
  kWinMediaFoundationGetAvailableDeviceMediaTypeFailed = 91,
  kWinMediaFoundationSetCurrentDeviceMediaTypeFailed = 92,
  kWinMediaFoundationEngineGetSinkFailed = 93,
  kWinMediaFoundationSinkQueryCapturePreviewInterfaceFailed = 94,
  kWinMediaFoundationSinkRemoveAllStreamsFailed = 95,
  kWinMediaFoundationCreateSinkVideoMediaTypeFailed = 96,
  kWinMediaFoundationConvertToVideoSinkMediaTypeFailed = 97,
  kWinMediaFoundationSinkAddStreamFailed = 98,
  kWinMediaFoundationSinkSetSampleCallbackFailed = 99,
  kWinMediaFoundationEngineStartPreviewFailed = 100,
  kWinMediaFoundationGetMediaEventStatusFailed = 101,
  kMacSetCaptureDeviceFailed = 102,
  kMacCouldNotStartCaptureDevice = 103,
  kMacReceivedFrameWithUnexpectedResolution = 104,
  kMacUpdateCaptureResolutionFailed = 105,
  kMacDeckLinkDeviceIdNotFoundInTheSystem = 106,
  kMacDeckLinkErrorQueryingInputInterface = 107,
  kMacDeckLinkErrorCreatingDisplayModeIterator = 108,
  kMacDeckLinkCouldNotFindADisplayMode = 109,
  kMacDeckLinkCouldNotSelectTheVideoFormatWeLike = 110,
  kMacDeckLinkCouldNotStartCapturing = 111,
  kMacDeckLinkUnsupportedPixelFormat = 112,
  kMacAvFoundationReceivedAVCaptureSessionRuntimeErrorNotification = 113,
  kAndroidApi2ErrorConfiguringCamera = 114,
  kCrosHalV3DeviceDelegateFailedToFlush = 115,
  kFuchsiaCameraDeviceDisconnected = 116,
  kFuchsiaCameraStreamDisconnected = 117,
  kFuchsiaSysmemDidNotSetImageFormat = 118,
  kFuchsiaSysmemInvalidBufferIndex = 119,
  kFuchsiaSysmemInvalidBufferSize = 120,
  kFuchsiaUnsupportedPixelFormat = 121,
  kFuchsiaFailedToMapSysmemBuffer = 122,
  kCrosHalV3DeviceContextDuplicatedClient = 123,
  kDesktopCaptureDeviceMacFailedStreamCreate = 124,
  kDesktopCaptureDeviceMacFailedStreamStart = 125,
  kCrosHalV3BufferManagerFailedToReserveBuffers = 126,
  kWinMediaFoundationSystemPermissionDenied = 127,
  kVideoCaptureImplTimedOutOnStart = 128,
  kLacrosVideoCaptureDeviceProxyAlreadyEndedOnFatalError = 129,
  kLacrosVideoCaptureDeviceProxyEncounteredFatalError = 130,
  kScreenCaptureKitFailedGetShareableContent = 131,
  kScreenCaptureKitFailedAddStreamOutput = 132,
  kScreenCaptureKitFailedStartCapture = 133,
  kScreenCaptureKitFailedStopCapture = 134,
  kScreenCaptureKitStreamError = 135,
  kScreenCaptureKitFailedToFindSCDisplay = 136,
  kVideoCaptureControllerUnsupportedPixelFormat = 137,
  kVideoCaptureControllerInvalid = 138,
  kVideoCaptureDeviceFactoryChromeOSCreateDeviceFailed = 139,
  kVideoCaptureDeviceAlreadyReleased = 140,
  kVideoCaptureSystemDeviceIdNotFound = 141,
  kVideoCaptureDeviceFactoryWinUnknownError = 142,
  kWinMediaFoundationDeviceInitializationFailed = 143,
  kWinMediaFoundationSourceCreationFailed = 144,
  kWinDirectShowDeviceFilterCreationFailed = 145,
  kWinDirectShowDeviceInitializationFailed = 146,
  kVideoCaptureDeviceFactorySecondCreateDenied = 147,
  kScreenCaptureKitResetStreamError = 148,
  kWinMediaFoundationCameraBusy = 149,
  kWebRtcStartCaptureFailed = 150,
  kMaxValue = 150
};

// WARNING: Do not change the values assigned to the entries. They are used for
// UMA logging.
enum class VideoCaptureFrameDropReason {
  kNone = 0,
  kDeviceClientFrameHasInvalidFormat = 1,
  kDeviceClientLibyuvConvertToI420Failed = 3,
  kV4L2BufferErrorFlagWasSet = 4,
  kV4L2InvalidNumberOfBytesInBuffer = 5,
  kAndroidThrottling = 6,
  kAndroidGetByteArrayElementsFailed = 7,
  kAndroidApi1UnexpectedDataLength = 8,
  kAndroidApi2AcquiredImageIsNull = 9,
  kWinDirectShowUnexpectedSampleLength = 10,
  kWinDirectShowFailedToGetMemoryPointerFromMediaSample = 11,
  kWinMediaFoundationReceivedSampleIsNull = 12,
  kWinMediaFoundationLockingBufferDelieveredNullptr = 13,
  kWinMediaFoundationGetBufferByIndexReturnedNull = 14,
  kBufferPoolMaxBufferCountExceeded = 15,
  kBufferPoolBufferAllocationFailed = 16,
  kVideoCaptureImplNotInStartedState = 17,
  kVideoCaptureImplFailedToWrapDataAsMediaVideoFrame = 18,
  kVideoTrackAdapterHasNoResolutionAdapters = 19,
  kResolutionAdapterFrameIsNotValid = 20,
  kResolutionAdapterWrappingFrameForCroppingFailed = 21,
  // kResolutionAdapterTimestampTooCloseToPrevious = 22, // combined into 23.
  kResolutionAdapterFrameRateIsHigherThanRequested = 23,
  kResolutionAdapterHasNoCallbacks = 24,
  kVideoTrackFrameDelivererNotEnabledReplacingWithBlackFrame = 25,
  kRendererSinkFrameDelivererIsNotStarted = 26,
  kCropVersionNotCurrent_DEPRECATED = 27,
  kGpuMemoryBufferMapFailed = 28,
  kSubCaptureTargetVersionNotCurrent = 29,
  kPostProcessingFailed = 30,
  kMaxValue = kPostProcessingFailed
};

// Assert that the int:frequency mapping is correct.
static_assert(static_cast<int>(PowerLineFrequency::kDefault) == 0,
              "static_cast<int>(PowerLineFrequency::kDefault) must equal 0.");
static_assert(static_cast<int>(PowerLineFrequency::k50Hz) == 50,
              "static_cast<int>(PowerLineFrequency::k50Hz) must equal 50.");
static_assert(static_cast<int>(PowerLineFrequency::k60Hz) == 60,
              "static_cast<int>(PowerLineFrequency::k60Hz) must equal 60.");

// Some drivers use rational time per frame instead of float frame rate, this
// constant k is used to convert between both: A fps -> [k/k*A] seconds/frame.
const int kFrameRatePrecision = 10000;

// Video capture format specification.
// This class is used by the video capture device to specify the format of every
// frame captured and returned to a client. It is also used to specify a
// supported capture format by a device.
struct CAPTURE_EXPORT VideoCaptureFormat {
  VideoCaptureFormat();
  VideoCaptureFormat(const gfx::Size& frame_size,
                     float frame_rate,
                     VideoPixelFormat pixel_format);

  static std::string ToString(const VideoCaptureFormat& format);

  // Compares the priority of the pixel formats. Returns true if |lhs| is the
  // preferred pixel format in comparison with |rhs|. Returns false otherwise.
  static bool ComparePixelFormatPreference(const VideoPixelFormat& lhs,
                                           const VideoPixelFormat& rhs);

  // Checks that all values are in the expected range. All limits are specified
  // in media::Limits.
  bool IsValid() const;

  bool operator==(const VideoCaptureFormat& other) const {
    return frame_size == other.frame_size && frame_rate == other.frame_rate &&
           pixel_format == other.pixel_format;
  }

  gfx::Size frame_size;
  float frame_rate;
  VideoPixelFormat pixel_format;
};

typedef std::vector<VideoCaptureFormat> VideoCaptureFormats;

// Parameters for starting video capture.
// This class is used by the client of a video capture device to specify the
// format of frames in which the client would like to have captured frames
// returned.
struct CAPTURE_EXPORT VideoCaptureParams {
  // Result struct for SuggestConstraints() method.
  struct SuggestedConstraints {
    gfx::Size min_frame_size;
    gfx::Size max_frame_size;
    bool fixed_aspect_ratio;

    std::string ToString() const;

    bool operator==(const SuggestedConstraints& other) const {
      return min_frame_size == other.min_frame_size &&
             max_frame_size == other.max_frame_size &&
             fixed_aspect_ratio == other.fixed_aspect_ratio;
    }
  };

  VideoCaptureParams();

  // Returns true if requested_format.IsValid() and all other values are within
  // their expected ranges.
  bool IsValid() const;

  // Computes and returns suggested capture constraints based on the requested
  // format and resolution change policy: minimum resolution, maximum
  // resolution, and whether a fixed aspect ratio is required.
  SuggestedConstraints SuggestConstraints() const;

  bool operator==(const VideoCaptureParams& other) const {
    return requested_format == other.requested_format &&
           resolution_change_policy == other.resolution_change_policy &&
           power_line_frequency == other.power_line_frequency &&
           is_high_dpi_enabled == other.is_high_dpi_enabled;
  }

  // Requests a resolution and format at which the capture will occur.
  VideoCaptureFormat requested_format;

  VideoCaptureBufferType buffer_type;

  // Policy for resolution change.
  ResolutionChangePolicy resolution_change_policy;

  // User-specified power line frequency.
  PowerLineFrequency power_line_frequency;

  // Flag indicating if face detection should be enabled. This is for
  // allowing the driver to apply appropriate settings for optimal
  // exposures around the face area. Currently only applicable on
  // Android platform with Camera2 driver support.
  bool enable_face_detection = false;

  // Flag indicating whether HiDPI mode should be enabled for tab capture
  // sessions.
  bool is_high_dpi_enabled = true;
};

CAPTURE_EXPORT std::ostream& operator<<(
    std::ostream& os,
    const VideoCaptureParams::SuggestedConstraints& constraints);

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CAPTURE_TYPES_H_
