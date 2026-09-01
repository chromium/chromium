// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/native_screen_capture_picker_mac.h"

#import <AppKit/AppKit.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <atomic>
#include <unordered_map>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/features.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/timer/timer.h"
#include "content/browser/media/capture/desktop_capture_util_mac.h"
#include "content/browser/media/capture/native_screen_capture_picker.h"
#include "content/browser/media/capture/screen_capture_kit_device_mac.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "media/capture/video/video_capture_device.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/webrtc/modules/desktop_capture/mac/window_list_utils.h"

// Enables the allowsChangingSelectedContent property on the native macOS
// picker (SCContentSharingPicker). This allows users to select a new window or
// screen to share without restarting the stream and enables the capture to
// follow an app into its fullscreen presentation mode.
// TODO(crbug.com/409475502): Remove this feature once it has been rolled out to
// stable for a few milestones.
BASE_FEATURE(kAllowChangingSelectedContent, base::FEATURE_ENABLED_BY_DEFAULT);

using Source = webrtc::DesktopCapturer::Source;
using PickerErrorCallback = base::RepeatingCallback<void(NSError*)>;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SCContentSharingPickerOperation {
  kPresentScreen_Start = 0,
  kPresentScreen_Update = 1,
  kPresentScreen_Cancel = 2,
  kPresentScreen_Error = 3,
  kPresentWindow_Start = 4,
  kPresentWindow_Update = 5,
  kPresentWindow_Cancel = 6,
  kPresentWindow_Error = 7,
  kMaxValue = kPresentWindow_Error
};

void API_AVAILABLE(macos(14.0))
    LogToUma(SCContentSharingPickerOperation operation) {
  base::UmaHistogramEnumeration(
      "Media.ScreenCaptureKit.SCContentSharingPicker2", operation);
}

void API_AVAILABLE(macos(14.0))
    LogUpdateToUma(content::DesktopMediaID::Type type) {
  LogToUma(type == content::DesktopMediaID::Type::TYPE_SCREEN
               ? SCContentSharingPickerOperation::kPresentScreen_Update
               : SCContentSharingPickerOperation::kPresentWindow_Update);
}

void API_AVAILABLE(macos(14.0))
    LogCancelToUma(content::DesktopMediaID::Type type) {
  LogToUma(type == content::DesktopMediaID::Type::TYPE_SCREEN
               ? SCContentSharingPickerOperation::kPresentScreen_Cancel
               : SCContentSharingPickerOperation::kPresentWindow_Cancel);
}

void API_AVAILABLE(macos(14.0))
    LogErrorToUma(content::DesktopMediaID::Type type) {
  LogToUma(type == content::DesktopMediaID::Type::TYPE_SCREEN
               ? SCContentSharingPickerOperation::kPresentScreen_Error
               : SCContentSharingPickerOperation::kPresentWindow_Error);
}

API_AVAILABLE(macos(14.0))
@interface PickerObserver : NSObject <SCContentSharingPickerObserver>
- (instancetype)
    initWithPickerCallback:
        (base::RepeatingCallback<void(SCContentFilter*, SCStream*)>)
            pickerCallback
            cancelCallback:
                (base::RepeatingCallback<void(SCStream*)>)cancelCallback
             errorCallback:
                 (base::RepeatingCallback<void(NSError*)>)errorCallback;
@end

@implementation PickerObserver {
  base::RepeatingCallback<void(SCContentFilter*, SCStream*)> _pickerCallback;
  base::RepeatingCallback<void(SCStream*)> _cancelCallback;
  PickerErrorCallback _errorCallback;
}

- (instancetype)initWithPickerCallback:
                    (base::RepeatingCallback<void(SCContentFilter*, SCStream*)>)
                        pickerCallback
                        cancelCallback:
                            (base::RepeatingCallback<void(SCStream*)>)
                                cancelCallback
                         errorCallback:(PickerErrorCallback)errorCallback {
  if ((self = [super init])) {
    _pickerCallback = std::move(pickerCallback);
    _cancelCallback = std::move(cancelCallback);
    _errorCallback = std::move(errorCallback);
  }
  return self;
}

- (void)contentSharingPicker:(SCContentSharingPicker*)picker
         didUpdateWithFilter:(SCContentFilter*)filter
                   forStream:(SCStream*)stream {
  _pickerCallback.Run(filter, stream);
}

- (void)contentSharingPicker:(SCContentSharingPicker*)picker
          didCancelForStream:(SCStream*)stream {
  _cancelCallback.Run(stream);
}

- (void)contentSharingPickerStartDidFailWithError:(NSError*)error {
  _errorCallback.Run(error);
}
@end

namespace {

// Frame queue depth for one-shot screenshot capture. Setting to 1 minimizes
// frame buffer allocation and latency in ScreenCaptureKit.
constexpr NSInteger kScreenshotQueueDepth = 1;

// Maximum duration to wait for a frame from ScreenCaptureKit before timing out.
constexpr base::TimeDelta kScreenshotTimeout = base::Seconds(5);

// Delay allowing the system picker window to complete its fade-out animation
// before capturing the screen.
constexpr base::TimeDelta kPickerFadeOutDelay = base::Milliseconds(250);

// Default resolution used as fallback when filter content rect is empty.
constexpr int kDefaultFallbackWidth = 1920;
constexpr int kDefaultFallbackHeight = 1080;

}  // namespace

// Helper object that captures a single video frame from an SCStream and stops
// capture immediately. SCScreenshotManager unconditionally enforces
// system-level TCC screen recording permissions in macOS System Settings
// (returning error -3801 when not granted), whereas SCStream is officially
// authorized by Apple to capture without global permissions when using an
// SCContentFilter originating from the SCContentSharingPicker.
API_AVAILABLE(macos(14.0))
@interface EphemeralFrameGrabber : NSObject <SCStreamOutput, SCStreamDelegate>
- (instancetype)initWithCallback:
    (base::OnceCallback<void(const SkBitmap&)>)callback;
- (void)startWithFilter:(SCContentFilter*)filter
          configuration:(SCStreamConfiguration*)config;
- (void)finishWithBitmap:(const SkBitmap&)bitmap;
@end

API_AVAILABLE(macos(14.0))
@implementation EphemeralFrameGrabber {
  base::OnceCallback<void(const SkBitmap&)> _callback;
  SCStream* __strong _stream;
  std::unique_ptr<base::OneShotTimer> _timeoutTimer;
  scoped_refptr<base::SingleThreadTaskRunner> _taskRunner;
  EphemeralFrameGrabber* __strong _selfRetain;
}

- (instancetype)initWithCallback:
    (base::OnceCallback<void(const SkBitmap&)>)callback {
  if ((self = [super init])) {
    _callback = std::move(callback);
    _taskRunner = base::SingleThreadTaskRunner::GetCurrentDefault();
    _timeoutTimer = std::make_unique<base::OneShotTimer>();
  }
  return self;
}

- (void)startWithFilter:(SCContentFilter*)filter
          configuration:(SCStreamConfiguration*)config {
  DCHECK(_taskRunner->RunsTasksInCurrentSequence());
  NSError* error = nil;
  _stream = [[SCStream alloc] initWithFilter:filter
                               configuration:config
                                    delegate:self];
  if (!_stream) {
    [self finishWithBitmap:SkBitmap()];
    return;
  }

  if (![_stream addStreamOutput:self
                           type:SCStreamOutputTypeScreen
             sampleHandlerQueue:dispatch_get_main_queue()
                          error:&error] ||
      error) {
    [self finishWithBitmap:SkBitmap()];
    return;
  }

  _selfRetain = self;

  _timeoutTimer->Start(FROM_HERE, kScreenshotTimeout,
                       base::BindOnce(
                           [](EphemeralFrameGrabber* grabber) {
                             [grabber finishWithBitmap:SkBitmap()];
                           },
                           base::Unretained(self)));

  __block EphemeralFrameGrabber* strongSelf = self;
  [_stream startCaptureWithCompletionHandler:^(NSError* _Nullable startError) {
    if (startError) {
      [strongSelf finishWithBitmap:SkBitmap()];
    }
  }];
}

- (void)stream:(SCStream*)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type {
  DCHECK(_taskRunner->RunsTasksInCurrentSequence());
  if (type != SCStreamOutputTypeScreen || !_callback) {
    return;
  }

  CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
  if (!pixelBuffer) {
    [self finishWithBitmap:SkBitmap()];
    return;
  }

  if (CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly) !=
      kCVReturnSuccess) {
    [self finishWithBitmap:SkBitmap()];
    return;
  }

  const size_t width = CVPixelBufferGetWidth(pixelBuffer);
  const size_t height = CVPixelBufferGetHeight(pixelBuffer);
  const size_t bytes_per_row = CVPixelBufferGetBytesPerRow(pixelBuffer);
  const void* src = CVPixelBufferGetBaseAddress(pixelBuffer);

  SkBitmap bitmap;
  if (src && width > 0 && height > 0) {
    SkImageInfo info = SkImageInfo::Make(width, height, kBGRA_8888_SkColorType,
                                         kPremul_SkAlphaType);
    if (bitmap.tryAllocPixels(info)) {
      SkPixmap src_pixmap(info, src, bytes_per_row);
      bitmap.writePixels(src_pixmap);
    }
  }
  CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);

  [self finishWithBitmap:bitmap];
}

- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error {
  [self finishWithBitmap:SkBitmap()];
}

- (void)finishWithBitmap:(const SkBitmap&)bitmap {
  if (!_taskRunner->RunsTasksInCurrentSequence()) {
    _taskRunner->PostTask(
        FROM_HERE,
        base::BindOnce([](EphemeralFrameGrabber* grabber,
                          SkBitmap bmp) { [grabber finishWithBitmap:bmp]; },
                       base::Unretained(self), bitmap));
    return;
  }

  if (!_callback) {
    return;
  }

  _timeoutTimer->Stop();
  auto callback = std::move(_callback);
  SCStream* stream = _stream;
  _stream = nil;

  if (stream) {
    [stream stopCaptureWithCompletionHandler:^(NSError* _Nullable error){
    }];
  }

  std::move(callback).Run(bitmap);
  _selfRetain = nil;
}
@end

namespace content {

// When enabled, this allows you to change the maximum number of streams you can
// share with the native picker to kMaxContentShareCountValue.
BASE_FEATURE(kMaxContentShareCount, base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<int> kMaxContentShareCountValue = {
    &kMaxContentShareCount, "max_content_share_count", 50};

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SCContentSharingPickerSessionEvent {
  kPickerOpened = 0,
  kWindowListUpdated = 1,
  kPrimaryAppRemoved = 2,
  kApplicationAudioRequested = 3,
  kMaxValue = kApplicationAudioRequested
};

void LogSessionEvent(SCContentSharingPickerSessionEvent event) {
  base::UmaHistogramEnumeration(
      "Media.ScreenCaptureKit.SCContentSharingPicker.SessionEvent", event);
}

API_AVAILABLE(macos(14.0))
NativeScreenCapturePickerMac::GetWindowOwnerPidCallback& GetTestingCallback() {
  static base::NoDestructor<
      NativeScreenCapturePickerMac::GetWindowOwnerPidCallback>
      callback;
  return *callback;
}

API_AVAILABLE(macos(14.0))
pid_t GetWindowOwnerPid(DesktopMediaID::Id id) {
  if (auto& testing_callback = GetTestingCallback()) {
    return testing_callback.Run(id);
  }
  return webrtc::GetWindowOwnerPid(id);
}

// Returns the backing scale factor of the NSScreen that contains the majority
// of the given `rect` (specified in ScreenCaptureKit's logical coordinates).
CGFloat GetBackingScaleFactorForRect(CGRect rect) {
  NSRect flipped_rect;
  flipped_rect.origin.x = rect.origin.x;
  flipped_rect.size = rect.size;

  // Convert ScreenCaptureKit coordinates (y=0 at the top-left of primary
  // screen) to AppKit coordinates (y=0 at the bottom-left of primary screen).
  NSArray<NSScreen*>* screens = [NSScreen screens];
  if (screens.count == 0) {
    return 1.0;
  }

  CGFloat primary_height = screens[0].frame.size.height;
  flipped_rect.origin.y = primary_height - (rect.origin.y + rect.size.height);

  // We find the screen with the largest overlapping intersection area with our
  // rect. We do not use a simple contains check because the window might span
  // multiple monitors or be partially offscreen.
  NSScreen* best_screen = screens[0];
  CGFloat max_area = 0;
  for (NSScreen* screen in screens) {
    const NSRect intersection = NSIntersectionRect(screen.frame, flipped_rect);
    const CGFloat area = intersection.size.width * intersection.size.height;
    if (area > max_area) {
      max_area = area;
      best_screen = screen;
    }
  }
  return best_screen.backingScaleFactor;
}

API_AVAILABLE(macos(14.0))
std::atomic<NativeScreenCapturePickerMac*> g_instance{nullptr};
}  // namespace

// static
NativeScreenCapturePickerMac* NativeScreenCapturePickerMac::GetInstance() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return g_instance.load();
}

void API_AVAILABLE(macos(14.0))
    NativeScreenCapturePickerMac::SetGetWindowOwnerPidForTesting(  // IN-TEST
        GetWindowOwnerPidCallback callback) {
  GetTestingCallback() = std::move(callback);
}

NativeScreenCapturePickerMac::CaptureSession::CaptureSession() = default;
NativeScreenCapturePickerMac::CaptureSession::~CaptureSession() = default;

NativeScreenCapturePickerMac::NativeScreenCapturePickerMac()
    : device_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(!g_instance.load())
      << "Only one instance of NativeScreenCapturePickerMac is allowed.";
  g_instance.store(this);
}

NativeScreenCapturePickerMac::~NativeScreenCapturePickerMac() {
  g_instance.store(nullptr);
}

void NativeScreenCapturePickerMac::Open(
    DesktopMediaID::Type type,
    base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
    base::OnceCallback<void(Source)> picker_callback,
    base::OnceClosure cancel_callback,
    base::OnceClosure error_callback,
    base::OnceCallback<void(DesktopMediaID::Id)> stop_audio_callback) {
  DCHECK(device_task_runner_->RunsTasksInCurrentSequence());
  CHECK(type == DesktopMediaID::Type::TYPE_SCREEN ||
        type == DesktopMediaID::Type::TYPE_WINDOW);
  if (@available(macOS 14.0, *)) {
    active_picker_source_id_++;
    active_picker_type_ = type;
    LogSessionEvent(SCContentSharingPickerSessionEvent::kPickerOpened);
    picker_callback_ = std::move(picker_callback);
    cancel_callback_ = std::move(cancel_callback);
    error_callback_ = std::move(error_callback);

    // Ensure the session entry exists and store the stop_audio_callback.
    auto& session = GetOrCreateCaptureSession(active_picker_source_id_);
    session.stop_audio_callback = std::move(stop_audio_callback);
    session.primary_audio_capture_id.reset();

    PickerUpdateCallback observer_update_callback = base::BindPostTask(
        device_task_runner_,
        base::BindRepeating(
            &NativeScreenCapturePickerMac::OnPickerObserverUpdated,
            weak_ptr_factory_.GetWeakPtr()));

    PickerCancelCallback observer_cancel_callback = base::BindPostTask(
        device_task_runner_,
        base::BindRepeating(
            &NativeScreenCapturePickerMac::OnPickerObserverCancelled,
            weak_ptr_factory_.GetWeakPtr()));

    PickerErrorCallback observer_error_callback = base::BindPostTask(
        device_task_runner_,
        base::BindRepeating(
            &NativeScreenCapturePickerMac::OnPickerObserverEncounteredError,
            weak_ptr_factory_.GetWeakPtr()));
    SCContentSharingPicker* picker = [SCContentSharingPicker sharedPicker];
    if (!picker_observer_) {
      picker_observer_ = [[PickerObserver alloc]
          initWithPickerCallback:std::move(observer_update_callback)
                  cancelCallback:std::move(observer_cancel_callback)
                   errorCallback:std::move(observer_error_callback)];
      [picker addObserver:picker_observer_];
    }

    std::move(created_callback).Run(active_picker_source_id_);
    picker.active = true;
    SCContentSharingPickerConfiguration* config = [picker defaultConfiguration];
    if (base::FeatureList::IsEnabled(kAllowChangingSelectedContent)) {
      config.allowsChangingSelectedContent = true;
    } else {
      config.allowsChangingSelectedContent = false;
    }
    NSNumber* max_stream_count = @(kMaxContentShareCountValue.Get());
    if (type == DesktopMediaID::Type::TYPE_SCREEN) {
      config.allowedPickerModes = SCContentSharingPickerModeSingleDisplay;
      picker.defaultConfiguration = config;
      picker.maximumStreamCount = max_stream_count;
      [picker presentPickerUsingContentStyle:SCShareableContentStyleDisplay];
      VLOG(1) << "NSCPM::Open: Show screen-sharing picker for source id = "
              << active_picker_source_id_;
      LogToUma(SCContentSharingPickerOperation::kPresentScreen_Start);
    } else {
      config.allowedPickerModes = SCContentSharingPickerModeSingleWindow;
      picker.defaultConfiguration = config;
      picker.maximumStreamCount = max_stream_count;
      [picker presentPickerUsingContentStyle:SCShareableContentStyleWindow];
      VLOG(1) << "NSCPM::Open: Show window-sharing picker for source id = "
              << active_picker_source_id_;
      LogToUma(SCContentSharingPickerOperation::kPresentWindow_Start);
    }
  } else {
    NOTREACHED();
  }
}

void NativeScreenCapturePickerMac::UpdateAudioStatusForSession(
    CaptureSession& session,
    DesktopMediaID::Id session_id,
    SCContentFilter* filter) {
  if (@available(macOS 15.2, *)) {
    // At the initial update, set `primary_audio_capture_id` to the
    // ApplicationAudioCaptureId of the application that owns the
    // first of the selected windows. Since the picker is run in
    // single-window mode, this list should typically only contain one
    // window.
    if (!session.primary_audio_capture_id && filter.includedWindows.count > 0) {
      SCWindow* first_window = filter.includedWindows.firstObject;
      session.primary_audio_capture_id = GetApplicationAudioCaptureIdForProcess(
          GetWindowOwnerPid(first_window.windowID));
      if (session.primary_audio_capture_id) {
        VLOG(1) << "NSCPM::UpdateAudioStatus: session " << session_id
                << " Set primary_audio_capture_id = "
                << session.primary_audio_capture_id->bundle_id;
      }
    }

    // If no window owned by the primary application remains in the
    // selection, the `stop_audio_callback` is called to signal that
    // audio capture should stop.
    if (session.primary_audio_capture_id && session.stop_audio_callback) {
      bool primary_app_present = false;
      for (SCWindow* window in filter.includedWindows) {
        if (GetApplicationAudioCaptureIdForProcess(GetWindowOwnerPid(
                window.windowID)) == session.primary_audio_capture_id) {
          primary_app_present = true;
          break;
        }
      }

      if (!primary_app_present) {
        VLOG(1) << "NSCPM::UpdateAudioStatus: session " << session_id
                << " Primary application no longer present. Triggering "
                   "stop_audio_callback.";
        if (!session.primary_app_removed_logged) {
          session.primary_app_removed_logged = true;
          LogSessionEvent(
              SCContentSharingPickerSessionEvent::kPrimaryAppRemoved);
        }
        std::move(session.stop_audio_callback).Run(session_id);
      }
    }
  }
}

void NativeScreenCapturePickerMac::OnPickerObserverUpdated(
    SCContentFilter* filter,
    SCStream* stream) {
  DCHECK(device_task_runner_->RunsTasksInCurrentSequence());

  DesktopMediaID::Id session_id = 0;
  if (stream) {
    auto it = stream_to_id_map_.find(stream);
    if (it != stream_to_id_map_.end()) {
      session_id = it->second;
    }
  } else {
    session_id = active_picker_source_id_;
  }

  if (session_id == 0) {
    VLOG(1) << "NSCPM::OnPickerObserverUpdated: session_id is 0";
    return;
  }

  auto& session = GetOrCreateCaptureSession(session_id);
  session.filter = filter;

  UpdateAudioStatusForSession(session, session_id, filter);

  // If `stream` is non-nil, this is an update to an already active capture
  // session (e.g., the user added a window or changed their selection via the
  // native macOS UI). ScreenCaptureKit automatically applies the new filter to
  // the active SCStream under the hood. There is no need to manually call
  // `[stream updateContentFilter...]`. We only update `session.filter` above so
  // the correct filter is preserved if the stream needs to be recreated later
  // (e.g., due to applyConstraints() changing the resolution).
  if (stream) {
    VLOG(1) << "NSCPM::OnPickerObserverUpdated: "
               "stream found in stream_to_id_map_ for source id "
            << session_id;
    if (!session.window_list_updated_logged) {
      session.window_list_updated_logged = true;
      LogSessionEvent(SCContentSharingPickerSessionEvent::kWindowListUpdated);
    }
    return;
  }

  if (!picker_callback_) {
    VLOG(1) << "NSCPM::OnPickerObserverUpdated: "
               "picker_callback_ is null for source id = "
            << session_id;
    return;
  }

  VLOG(1) << "NSCPM::OnPickerObserverUpdated: for source id = " << session_id;

  if (!session.received_first_response) {
    session.received_first_response = true;
    LogUpdateToUma(active_picker_type_);
  }

  cancel_callback_.Reset();
  error_callback_.Reset();

  Source source;
  source.id = session_id;
  std::move(picker_callback_).Run(source);
}

void NativeScreenCapturePickerMac::OnPickerObserverCancelled(SCStream* stream) {
  DCHECK(device_task_runner_->RunsTasksInCurrentSequence());
  if (stream) {
    auto it = stream_to_id_map_.find(stream);
    if (it != stream_to_id_map_.end()) {
      VLOG(1) << "NSCPM::OnPickerObserverCancelled: source id = " << it->second;
      // TODO(https://crbug.com/409475502): Decide if we want to add logging
      // here or do something else.
    } else {
      VLOG(1) << "NSCPM::OnPickerObserverCancelled: "
                 "stream not found in stream_to_id_map_";
    }
    return;
  }

  VLOG(1) << "NSCPM::OnPickerObserverCancelled: sourcce id = "
          << active_picker_source_id_;
  auto& session = GetOrCreateCaptureSession(active_picker_source_id_);
  if (!session.received_first_response) {
    session.received_first_response = true;
    LogCancelToUma(active_picker_type_);
  }
  picker_callback_.Reset();
  error_callback_.Reset();
  if (cancel_callback_) {
    std::move(cancel_callback_).Run();
  }
  MaybeDeactivatePicker();
}

void NativeScreenCapturePickerMac::OnPickerObserverEncounteredError(
    NSError* error) {
  DCHECK(device_task_runner_->RunsTasksInCurrentSequence());

  VLOG(1) << "NSCPM::OnPickerObserverEncounteredError: source id = "
          << active_picker_source_id_ << ", code = " << [error code]
          << ", domain = " << [error domain]
          << ", description = " << [error localizedDescription];
  auto& session = GetOrCreateCaptureSession(active_picker_source_id_);
  if (!session.received_first_response) {
    session.received_first_response = true;
    LogErrorToUma(active_picker_type_);
  }
  picker_callback_.Reset();
  cancel_callback_.Reset();
  if (error_callback_) {
    std::move(error_callback_).Run();
  }
  MaybeDeactivatePicker();
}

void NativeScreenCapturePickerMac::UpdateStreamMap(DesktopMediaID::Id id,
                                                   SCStream* stream) {
  DCHECK(device_task_runner_->RunsTasksInCurrentSequence());
  if (@available(macOS 14.0, *)) {
    if (!stream) {
      return;
    }
    stream_to_id_map_[stream] = id;

    VLOG(1) << "NSCPM::UpdateStreamMap: for source id = " << id;
  } else {
    NOTREACHED();
  }
}

void NativeScreenCapturePickerMac::Close(DesktopMediaID device_id) {
  DCHECK(device_task_runner_->RunsTasksInCurrentSequence());
  if (@available(macOS 14.0, *)) {
    ScheduleCleanup(device_id.id);
    active_source_ids_.erase(device_id.id);
    MaybeDeactivatePicker();
    VLOG(1) << "NSCPM::Close: for source id = " << device_id.id;
  } else {
    NOTREACHED();
  }
}

void NativeScreenCapturePickerMac::GetApplicationAudioCaptureId(
    DesktopMediaID::Id session_id,
    GetApplicationAudioCaptureIdCallback callback) {
  DCHECK(device_task_runner_->RunsTasksInCurrentSequence());
  std::optional<desktop_capture::ApplicationAudioCaptureId>
      application_audio_capture_id;

  auto it = sessions_.find(session_id);
  if (it != sessions_.end()) {
    if (!it->second->application_audio_requested_logged) {
      it->second->application_audio_requested_logged = true;
      LogSessionEvent(
          SCContentSharingPickerSessionEvent::kApplicationAudioRequested);
    }
    application_audio_capture_id = it->second->primary_audio_capture_id;
  }

  std::move(callback).Run(application_audio_capture_id);
}

std::unique_ptr<media::VideoCaptureDevice>
NativeScreenCapturePickerMac::CreateDevice(const DesktopMediaID& source) {
  DCHECK(device_task_runner_->RunsTasksInCurrentSequence());

  auto& session = GetOrCreateCaptureSession(source.id);
  session.cleanup_timer.Stop();
  active_source_ids_.insert(source.id);
  SCContentSharingPicker* picker = [SCContentSharingPicker sharedPicker];
  picker.active = true;
  VLOG(1) << "NSCPM::CreateDevice: source.id = " << source.id
          << ", sessions_.count = " << sessions_.size();
  return CreateScreenCaptureKitDeviceMac(
      source, /*is_native_picker=*/true, session.filter,
      base::BindPostTask(
          device_task_runner_,
          base::BindOnce(&NativeScreenCapturePickerMac::UpdateStreamMap,
                         weak_ptr_factory_.GetWeakPtr())),
      /*pip_screen_capture_coordinator_proxy=*/nullptr);
}

void NativeScreenCapturePickerMac::ScheduleCleanup(DesktopMediaID::Id id) {
  DCHECK(device_task_runner_->RunsTasksInCurrentSequence());
  // We need to retain the content filter for some time in case the device is
  // restarted, e.g., when ApplyConstraints is called on a MediaStreamTrack.
  GetOrCreateCaptureSession(id).cleanup_timer.Start(
      FROM_HERE, base::Seconds(60),
      base::BindOnce(
          &NativeScreenCapturePickerMac::CleanupContentFilter,
          // Passing `this` is safe since `sessions_` is owned by `this`.
          base::Unretained(this), id));
}

void NativeScreenCapturePickerMac::CleanupContentFilter(DesktopMediaID::Id id) {
  DCHECK(device_task_runner_->RunsTasksInCurrentSequence());
  sessions_.erase(id);
  absl::erase_if(stream_to_id_map_, [&](const auto& stream_to_id_pair) {
    return stream_to_id_pair.second == id;
  });

  VLOG(1) << "NSCPM::CleanupContentFilter: source id = " << id
          << ", sessions_.count = " << sessions_.size()
          << ", stream_to_id_map_.count = " << stream_to_id_map_.size();
}

NativeScreenCapturePickerMac::CaptureSession&
NativeScreenCapturePickerMac::GetOrCreateCaptureSession(DesktopMediaID::Id id) {
  DCHECK(device_task_runner_->RunsTasksInCurrentSequence());
  std::unique_ptr<CaptureSession>& session = sessions_[id];
  if (!session) {
    session = std::make_unique<CaptureSession>();
  }
  return *session;
}

base::WeakPtr<NativeScreenCapturePicker>
NativeScreenCapturePickerMac::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void NativeScreenCapturePickerMac::CaptureScreenshot(
    DesktopMediaID::Id session_id,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  CHECK(device_task_runner_->RunsTasksInCurrentSequence());
  // The system picker requires a short delay to fully close and fade out,
  // otherwise the picker UI itself is captured in the screenshot.
  device_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&NativeScreenCapturePickerMac::CaptureScreenshotInternal,
                     weak_ptr_factory_.GetWeakPtr(), session_id,
                     std::move(callback)),
      kPickerFadeOutDelay);
}

void NativeScreenCapturePickerMac::CaptureScreenshotInternal(
    DesktopMediaID::Id session_id,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  CHECK(device_task_runner_->RunsTasksInCurrentSequence());

  auto it = sessions_.find(session_id);
  // If the session was closed or does not contain a valid capture filter
  // (e.g., if the user cancelled the picker or the window became invalid),
  // we cannot take a screenshot. Return a null bitmap.
  if (it == sessions_.end() || !it->second->filter) {
    MaybeDeactivatePicker();
    std::move(callback).Run(SkBitmap());
    return;
  }

  SCContentFilter* filter = it->second->filter;
  SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];

  config.ignoreShadowsSingleWindow = YES;
  config.backgroundColor = CGColorGetConstantColor(kCGColorClear);
  config.scalesToFit = YES;
  // Request 32BGRA format to match SkBitmap's expected color type.
  config.pixelFormat = kCVPixelFormatType_32BGRA;
  config.showsCursor = NO;

  const CGRect rect = filter.contentRect;

  // ScreenCaptureKit's output dimensions (config.width/height) are specified in
  // physical pixels, while filter.contentRect is in logical points. We must
  // explicitly scale the dimensions by the backing scale factor to capture the
  // window at its native Retina resolution (otherwise it will be downscaled and
  // look blurry).
  //
  // If the content rect is empty (e.g., if the window is invalid or closed), we
  // fall back to a default size (1920x1080) to prevent passing 0 dimensions to
  // ScreenCaptureKit, which would cause a crash.
  if (!CGRectIsEmpty(rect)) {
    const CGFloat scale = GetBackingScaleFactorForRect(rect);
    config.width = rect.size.width * scale;
    config.height = rect.size.height * scale;
  } else {
    config.width = kDefaultFallbackWidth;
    config.height = kDefaultFallbackHeight;
  }

  config.queueDepth = kScreenshotQueueDepth;

  EphemeralFrameGrabber* grabber = [[EphemeralFrameGrabber alloc]
      initWithCallback:base::BindOnce(
                           &NativeScreenCapturePickerMac::OnScreenshotCaptured,
                           weak_ptr_factory_.GetWeakPtr(), session_id,
                           std::move(callback))];
  [grabber startWithFilter:filter configuration:config];
}

void NativeScreenCapturePickerMac::OnScreenshotCaptured(
    DesktopMediaID::Id session_id,
    base::OnceCallback<void(const SkBitmap&)> callback,
    const SkBitmap& bitmap) {
  CHECK(device_task_runner_->RunsTasksInCurrentSequence());
  CleanupContentFilter(session_id);
  MaybeDeactivatePicker();
  std::move(callback).Run(bitmap);
}

void NativeScreenCapturePickerMac::MaybeDeactivatePicker() {
  DCHECK(device_task_runner_->RunsTasksInCurrentSequence());
  // Don't deactivate the picker if there are any active capture sessions.
  if (active_source_ids_.empty()) {
    SCContentSharingPicker* picker = [SCContentSharingPicker sharedPicker];
    picker.active = false;
  }
}

void CaptureScreenshotFromMacNativePicker(
    DesktopMediaID::Id session_id,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  if (@available(macOS 14.0, *)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](DesktopMediaID::Id session_id,
               base::OnceCallback<void(const SkBitmap&)> callback) {
              if (auto* picker = NativeScreenCapturePickerMac::GetInstance()) {
                picker->CaptureScreenshot(session_id, std::move(callback));
              } else {
                std::move(callback).Run(SkBitmap());
              }
            },
            session_id,
            base::BindPostTask(
                base::SingleThreadTaskRunner::GetCurrentDefault(),
                std::move(callback))));
    return;
  }
  std::move(callback).Run(SkBitmap());
}

std::unique_ptr<NativeScreenCapturePicker>
CreateNativeScreenCapturePickerMac() {
  if (@available(macOS 14.0, *)) {
    return std::make_unique<NativeScreenCapturePickerMac>();
  }
  return nullptr;
}

}  // namespace content
