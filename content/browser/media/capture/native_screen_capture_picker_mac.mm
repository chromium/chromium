// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/native_screen_capture_picker_mac.h"

#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <unordered_map>
#include <utility>

#include "base/features.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/timer/timer.h"
#include "content/browser/media/capture/native_screen_capture_picker.h"
#include "content/browser/media/capture/screen_capture_kit_device_mac.h"
#include "content/public/browser/desktop_media_id.h"
#include "media/capture/video/video_capture_device.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

// Enables the allowsChangingSelectedContent property on the native macOS
// picker (SCContentSharingPicker). This allows users to select a new window or
// screen to share without restarting the stream and enables the capture to
// follow an app into its fullscreen presentation mode.
// TODO(crbug.com/409475502): Remove this feature once it has been rolled out to
// stable for a few milestones.
BASE_FEATURE(kAllowChangingSelectedContent, base::FEATURE_ENABLED_BY_DEFAULT);

using Source = webrtc::DesktopCapturer::Source;

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
  base::RepeatingCallback<void(NSError*)> _errorCallback;
}

- (instancetype)
    initWithPickerCallback:
        (base::RepeatingCallback<void(SCContentFilter*, SCStream*)>)
            pickerCallback
            cancelCallback:
                (base::RepeatingCallback<void(SCStream*)>)cancelCallback
             errorCallback:
                 (base::RepeatingCallback<void(NSError*)>)errorCallback {
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

class API_AVAILABLE(macos(14.0)) NativeScreenCapturePickerMac
    : public NativeScreenCapturePicker {
 public:
  using PickerUpdateCallback =
      base::RepeatingCallback<void(SCContentFilter*, SCStream*)>;
  using PickerCancelCallback = base::RepeatingCallback<void(SCStream*)>;
  using PickerErrorCallback = base::RepeatingCallback<void(NSError*)>;

  NativeScreenCapturePickerMac();
  ~NativeScreenCapturePickerMac() override = default;

  void Open(DesktopMediaID::Type type,
            base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
            base::OnceCallback<void(Source)> picker_callback,
            base::OnceClosure cancel_callback,
            base::OnceClosure error_callback) override;
  void Close(DesktopMediaID device_id) override;
  std::unique_ptr<media::VideoCaptureDevice> CreateDevice(
      const DesktopMediaID& source) override;

  base::WeakPtr<NativeScreenCapturePicker> GetWeakPtr() override;

 private:
  // Callbacks called by PickerObserver when it receives an event from the OS.
  void OnPickerObserverUpdated(SCContentFilter* filter, SCStream* stream);
  void OnPickerObserverCancelled(SCStream* stream);
  void OnPickerObserverEncounteredError(NSError* error);

  void ScheduleCleanup(DesktopMediaID::Id id);
  void CleanupContentFilter(DesktopMediaID::Id id);

  // Callback called by `ScreenCaptureKitDeviceMac` on creating a new stream.
  void UpdateStreamMap(DesktopMediaID::Id id, SCStream* stream);

  // There is only one picker observer which has callbacks initialized only
  // once.
  PickerObserver* __strong picker_observer_;
  base::OnceCallback<void(Source)> picker_callback_;
  base::OnceClosure cancel_callback_;
  base::OnceClosure error_callback_;
  absl::flat_hash_set<int> received_first_response_;

  // On every new getDisplayMedia request, we increment
  // `active_picker_source_id_` and assign `active_picker_type_` the type of the
  // request. Since while making a selection of the capture surface the rest of
  // the UI becomes non-interactive, it is ensured that the callbacks are called
  // for the right selection.
  DesktopMediaID::Id active_picker_source_id_ = 0;
  DesktopMediaID::Type active_picker_type_;

  std::unordered_map<DesktopMediaID::Id, base::OneShotTimer>
      cached_content_filters_cleanup_timers_;

  // `active_source_ids_` keeps a track of the number of active capture
  // sessions.
  absl::flat_hash_set<int> active_source_ids_;
  absl::flat_hash_map<DesktopMediaID::Id, SCContentFilter*> content_filters_;
  absl::flat_hash_map<SCStream*, DesktopMediaID::Id> stream_to_id_map_;

  const scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;
  base::WeakPtrFactory<NativeScreenCapturePickerMac> weak_ptr_factory_{this};
};

NativeScreenCapturePickerMac::NativeScreenCapturePickerMac()
    : device_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

void NativeScreenCapturePickerMac::Open(
    DesktopMediaID::Type type,
    base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
    base::OnceCallback<void(Source)> picker_callback,
    base::OnceClosure cancel_callback,
    base::OnceClosure error_callback) {
  DCHECK(device_task_runner_->RunsTasksInCurrentSequence());
  CHECK(type == DesktopMediaID::Type::TYPE_SCREEN ||
        type == DesktopMediaID::Type::TYPE_WINDOW);
  if (@available(macOS 14.0, *)) {
    active_picker_source_id_++;
    active_picker_type_ = type;
    picker_callback_ = std::move(picker_callback);
    cancel_callback_ = std::move(cancel_callback);
    error_callback_ = std::move(error_callback);

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

void NativeScreenCapturePickerMac::OnPickerObserverUpdated(
    SCContentFilter* filter,
    SCStream* stream) {
  DCHECK(device_task_runner_->RunsTasksInCurrentSequence());

  if (stream) {
    auto it = stream_to_id_map_.find(stream);
    if (it != stream_to_id_map_.end()) {
      int source_id = it->second;
      content_filters_[source_id] = filter;
      VLOG(1) << "NSCPM::OnPickerObserverUpdated: "
                 "stream found in stream_to_id_map_ for source id "
              << source_id;
    } else {
      VLOG(1) << "NSCPM::OnPickerObserverUpdated: "
                 "stream not found in stream_to_id_map_";
    }
    return;
  }
  if (!picker_callback_) {
    VLOG(1) << "NSCPM::OnPickerObserverUpdated: "
               "picker_callback_ is null for source id = "
            << active_picker_source_id_;
    return;
  }
  VLOG(1) << "NSCPM::OnPickerObserverUpdated: for source id = "
          << active_picker_source_id_;
  if (!received_first_response_.contains(active_picker_source_id_)) {
    received_first_response_.insert(active_picker_source_id_);
    LogUpdateToUma(active_picker_type_);
  }
  content_filters_[active_picker_source_id_] = filter;

  Source source;
  source.id = active_picker_source_id_;
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
  if (!received_first_response_.contains(active_picker_source_id_)) {
    received_first_response_.insert(active_picker_source_id_);
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
  if (!received_first_response_.contains(active_picker_source_id_)) {
    received_first_response_.insert(active_picker_source_id_);
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
    if (active_source_ids_.size() == 0) {
      SCContentSharingPicker* picker = [SCContentSharingPicker sharedPicker];
      picker.active = false;
    }
    VLOG(1) << "NSCPM::Close: for source id = " << device_id.id;
  } else {
    NOTREACHED();
  }
}

std::unique_ptr<media::VideoCaptureDevice>
NativeScreenCapturePickerMac::CreateDevice(const DesktopMediaID& source) {
  DCHECK(device_task_runner_->RunsTasksInCurrentSequence());

  cached_content_filters_cleanup_timers_.erase(source.id);
  active_source_ids_.insert(source.id);
  SCContentSharingPicker* picker = [SCContentSharingPicker sharedPicker];
  picker.active = true;
  VLOG(1) << "NSCPM::CreateDevice: source.id = " << source.id
          << ", contentFilters.count = " << content_filters_.size();
  return CreateScreenCaptureKitDeviceMac(
      source, content_filters_[source.id],
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
  cached_content_filters_cleanup_timers_[id].Start(
      FROM_HERE, base::Seconds(60),
      base::BindPostTask(
          device_task_runner_,
          base::BindOnce(
              &NativeScreenCapturePickerMac::CleanupContentFilter,
              // Passing `this` is safe since
              // `cached_content_filters_cleanup_timers_` is owned by `this`.
              base::Unretained(this), id)));
}

void NativeScreenCapturePickerMac::CleanupContentFilter(DesktopMediaID::Id id) {
  DCHECK(device_task_runner_->RunsTasksInCurrentSequence());
  content_filters_.erase(id);
  absl::erase_if(stream_to_id_map_, [&](const auto& stream_to_id_pair) {
    return stream_to_id_pair.second == id;
  });

  cached_content_filters_cleanup_timers_.erase(id);

  VLOG(1) << "NSCPM::CleanupContentFilter: source id = " << id
          << ", contentFilters.count = " << content_filters_.size()
          << ", stream_to_id_map_.count = " << stream_to_id_map_.size();
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
