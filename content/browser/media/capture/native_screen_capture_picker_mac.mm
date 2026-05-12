// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/native_screen_capture_picker_mac.h"

#import <AppKit/AppKit.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <unordered_map>
#include <utility>

#include "base/check.h"
#include "base/features.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/timer/timer.h"
#include "content/browser/media/capture/desktop_capture_util_mac.h"
#include "content/browser/media/capture/native_screen_capture_picker.h"
#include "content/browser/media/capture/screen_capture_kit_device_mac.h"
#include "content/public/browser/desktop_media_id.h"
#include "media/capture/video/video_capture_device.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
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

namespace content {

// When enabled, this allows you to change the maximum number of streams you can
// share with the native picker to kMaxContentShareCountValue.
BASE_FEATURE(kMaxContentShareCount, base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<int> kMaxContentShareCountValue = {
    &kMaxContentShareCount, "max_content_share_count", 50};

namespace {
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
}  // namespace

void API_AVAILABLE(macos(14.0))
    NativeScreenCapturePickerMac::SetGetWindowOwnerPidForTesting(  // IN-TEST
        GetWindowOwnerPidCallback callback) {
  GetTestingCallback() = std::move(callback);
}

NativeScreenCapturePickerMac::CaptureSession::CaptureSession() = default;
NativeScreenCapturePickerMac::CaptureSession::~CaptureSession() = default;

NativeScreenCapturePickerMac::NativeScreenCapturePickerMac()
    : device_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

NativeScreenCapturePickerMac::~NativeScreenCapturePickerMac() = default;

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
  if (cancel_callback_) {
    std::move(cancel_callback_).Run();
  }
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
  if (error_callback_) {
    std::move(error_callback_).Run();
  }
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
    // Don't deactivate the picker if there are any active capture sessions.
    if (active_source_ids_.empty()) {
      SCContentSharingPicker* picker = [SCContentSharingPicker sharedPicker];
      picker.active = false;
    }
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
      base::BindPostTask(
          device_task_runner_,
          base::BindOnce(
              &NativeScreenCapturePickerMac::CleanupContentFilter,
              // Passing `this` is safe since `sessions_` is owned by `this`.
              base::Unretained(this), id)));
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

std::unique_ptr<NativeScreenCapturePicker>
CreateNativeScreenCapturePickerMac() {
  if (@available(macOS 14.0, *)) {
    return std::make_unique<NativeScreenCapturePickerMac>();
  }
  return nullptr;
}

}  // namespace content
