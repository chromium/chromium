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

using Source = webrtc::DesktopCapturer::Source;
using PickerCallback = base::OnceCallback<void(Source)>;
using PickerCancelCallback = base::OnceClosure;
using PickerErrorCallback = base::OnceClosure;

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
- (void)updateWithPickerCallback:(PickerCallback)pickerCallback
                  cancelCallback:(PickerCancelCallback)cancelCallback
                   errorCallback:(PickerErrorCallback)errorCallback
                  assignSourceId:(int)assignedSourceId
                            type:(content::DesktopMediaID::Type)type;
@property(strong, readonly)
    NSMutableDictionary<NSNumber*, SCContentFilter*>* contentFilters;
@property(strong) NSMapTable<SCStream*, NSNumber*>* streamToIdMapping;
@end

@implementation PickerObserver {
  PickerCallback _pickerCallback;
  PickerCancelCallback _cancelCallback;
  PickerErrorCallback _errorCallback;
  int _assignedSourceId;
  content::DesktopMediaID::Type _type;
  std::unordered_set<int> _receivedFirstResponse;
  SEQUENCE_CHECKER(_sequenceChecker);
}

@synthesize contentFilters;
@synthesize streamToIdMapping;

- (instancetype)init {
  if ((self = [super init])) {
    contentFilters = [[NSMutableDictionary alloc] init];
    streamToIdMapping = [NSMapTable strongToStrongObjectsMapTable];
  }
  return self;
}

- (void)updateWithPickerCallback:(PickerCallback)pickerCallback
                  cancelCallback:(PickerCancelCallback)cancelCallback
                   errorCallback:(PickerErrorCallback)errorCallback
                  assignSourceId:(int)assignedSourceId
                            type:(content::DesktopMediaID::Type)type {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _pickerCallback = std::move(pickerCallback);
  _cancelCallback = std::move(cancelCallback);
  _errorCallback = std::move(errorCallback);
  _assignedSourceId = assignedSourceId;
  _type = type;
}

- (void)contentSharingPicker:(SCContentSharingPicker*)picker
         didUpdateWithFilter:(SCContentFilter*)filter
                   forStream:(SCStream*)stream {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (stream) {
    if (NSNumber* sourceId = [streamToIdMapping objectForKey:stream]) {
      contentFilters[sourceId] = filter;
      VLOG(1) << "NSCPM::contentSharingPicker:didUpdateWithFilter: "
                 "stream found in streamToIdMapping for sourceId "
              << [sourceId intValue];
    } else {
      VLOG(1) << "NSCPM::contentSharingPicker:didUpdateWithFilter: "
                 "stream not found in streamToIdMapping";
    }
    return;
  }
  if (!_pickerCallback) {
    VLOG(1) << "NSCPM::contentSharingPicker:didUpdateWithFilter: "
               "_pickerCallback is null for source_id = "
            << _assignedSourceId;
    return;
  }
  VLOG(1) << "NSCPM::contentSharingPicker:didUpdateWithFilter: source_id = "
          << _assignedSourceId;
  if (!_receivedFirstResponse.contains(_assignedSourceId)) {
    _receivedFirstResponse.insert(_assignedSourceId);
    LogUpdateToUma(_type);
  }
  contentFilters[@(_assignedSourceId)] = filter;

  Source source;
  source.id = _assignedSourceId;
  std::move(_pickerCallback).Run(source);
}

// TODO(https://crbug.com/409475502): Handle `didCancelForStream` when it can be
// called multiple times for a capture session when we will add support for
// changing selected content.
- (void)contentSharingPicker:(SCContentSharingPicker*)picker
          didCancelForStream:(SCStream*)stream {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  VLOG(1) << "NSCPM:contentSharingPicker:didCancelForStream: source_id = "
          << _assignedSourceId;
  if (!_receivedFirstResponse.contains(_assignedSourceId)) {
    _receivedFirstResponse.insert(_assignedSourceId);
    LogCancelToUma(_type);
  }
  if (_cancelCallback) {
    std::move(_cancelCallback).Run();
  }
}

- (void)contentSharingPickerStartDidFailWithError:(NSError*)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  VLOG(1) << "NSCPM::contentSharingPickerStartDidFailWithError: source_id = "
          << _assignedSourceId << ", code = " << [error code]
          << ", domain = " << [error domain]
          << ", description = " << [error localizedDescription];
  if (!_receivedFirstResponse.contains(_assignedSourceId)) {
    _receivedFirstResponse.insert(_assignedSourceId);
    LogErrorToUma(_type);
  }
  if (_errorCallback) {
    std::move(_errorCallback).Run();
  }
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
  NativeScreenCapturePickerMac();
  ~NativeScreenCapturePickerMac() override;

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
  void ScheduleCleanup(DesktopMediaID::Id id);
  void CleanupContentFilter(DesktopMediaID::Id id);

  // Callback called by `ScreenCaptureKitDeviceMac` on creating a new stream.
  void UpdateStreamMap(DesktopMediaID::Id id, SCStream* stream);

  // `active_source_ids_` keeps a track of the number of active capture
  // sessions.
  NSMutableSet<NSNumber*>* __strong active_source_ids_;

  // There is only one picker observer which is assigned new callbacks and a new
  // source id with a new getDisplayMedia request. Since while making a
  // selection of the capture surface the rest of the UI becomes
  // non-interactive, it is ensured that the callbacks are called for the right
  // selection.
  PickerObserver* __strong picker_observer_;
  std::unordered_map<DesktopMediaID::Id, base::OneShotTimer>
      cached_content_filters_cleanup_timers_;
  DesktopMediaID::Id next_id_ = 0;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<NativeScreenCapturePickerMac> weak_ptr_factory_{this};
};

NativeScreenCapturePickerMac::NativeScreenCapturePickerMac()
    : active_source_ids_([[NSMutableSet alloc] init]) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

NativeScreenCapturePickerMac::~NativeScreenCapturePickerMac() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NativeScreenCapturePickerMac::Open(
    DesktopMediaID::Type type,
    base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
    base::OnceCallback<void(Source)> picker_callback,
    base::OnceClosure cancel_callback,
    base::OnceClosure error_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(type == DesktopMediaID::Type::TYPE_SCREEN ||
        type == DesktopMediaID::Type::TYPE_WINDOW);
  if (@available(macOS 14.0, *)) {
    NSNumber* source_id = @(next_id_);
    SCContentSharingPicker* picker = [SCContentSharingPicker sharedPicker];
    if (!picker_observer_) {
      picker_observer_ = [[PickerObserver alloc] init];
      [picker addObserver:picker_observer_];
    }
    [picker_observer_ updateWithPickerCallback:std::move(picker_callback)
                                cancelCallback:std::move(cancel_callback)
                                 errorCallback:std::move(error_callback)
                                assignSourceId:next_id_
                                          type:type];
    std::move(created_callback).Run(next_id_);
    ++next_id_;
    picker.active = true;
    SCContentSharingPickerConfiguration* config = [picker defaultConfiguration];
    // TODO(https://crbug.com/360781940): Add support for changing selected
    // content. The problem to solve is how this should interact with stream
    // restart.
    config.allowsChangingSelectedContent = false;
    NSNumber* max_stream_count = @(kMaxContentShareCountValue.Get());
    if (type == DesktopMediaID::Type::TYPE_SCREEN) {
      config.allowedPickerModes = SCContentSharingPickerModeSingleDisplay;
      picker.defaultConfiguration = config;
      picker.maximumStreamCount = max_stream_count;
      [picker presentPickerUsingContentStyle:SCShareableContentStyleDisplay];
      VLOG(1) << "NSCPM: Show screen-sharing picker for source_id = "
              << source_id.longValue;
      LogToUma(SCContentSharingPickerOperation::kPresentScreen_Start);
    } else {
      config.allowedPickerModes = SCContentSharingPickerModeSingleWindow;
      picker.defaultConfiguration = config;
      picker.maximumStreamCount = max_stream_count;
      [picker presentPickerUsingContentStyle:SCShareableContentStyleWindow];
      VLOG(1) << "NSCPM: Show window-sharing picker for source_id = "
              << source_id.longValue;
      LogToUma(SCContentSharingPickerOperation::kPresentWindow_Start);
    }
  } else {
    NOTREACHED();
  }
}

void NativeScreenCapturePickerMac::UpdateStreamMap(DesktopMediaID::Id id,
                                                   SCStream* stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (@available(macOS 14.0, *)) {
    if (!stream) {
      return;
    }

    [[picker_observer_ streamToIdMapping] setObject:@(id) forKey:stream];

    VLOG(1) << "NSCPM: UpdateStreamMap for source_id = " << id;
  } else {
    NOTREACHED();
  }
}

void NativeScreenCapturePickerMac::Close(DesktopMediaID device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (@available(macOS 14.0, *)) {
    ScheduleCleanup(device_id.id);
    [active_source_ids_ removeObject:@(device_id.id)];
    // Don't deactivate the picker if there are any active capture sessions.
    if ([active_source_ids_ count] == 0) {
      SCContentSharingPicker* picker = [SCContentSharingPicker sharedPicker];
      picker.active = false;
    }
    VLOG(1) << "NSCPM: Closing source_id = " << device_id.id;
  } else {
    NOTREACHED();
  }
}

std::unique_ptr<media::VideoCaptureDevice>
NativeScreenCapturePickerMac::CreateDevice(const DesktopMediaID& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cached_content_filters_cleanup_timers_.erase(source.id);
  NSNumber* source_id = @(source.id);
  [active_source_ids_ addObject:source_id];
  SCContentSharingPicker* picker = [SCContentSharingPicker sharedPicker];
  picker.active = true;
  VLOG(1) << "NSCPM: CreateDevice: source_id = " << source.id
          << ", contentFilters.count = " <<
      [[picker_observer_ contentFilters] count];
  return CreateScreenCaptureKitDeviceMac(
      source, [picker_observer_ contentFilters][source_id],
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&NativeScreenCapturePickerMac::UpdateStreamMap,
                         weak_ptr_factory_.GetWeakPtr())));
}

void NativeScreenCapturePickerMac::ScheduleCleanup(DesktopMediaID::Id id) {
  // We need to retain the content filter for some time in case the device is
  // restarted, e.g., when ApplyConstraints is called on a MediaStreamTrack.
  cached_content_filters_cleanup_timers_[id].Start(
      FROM_HERE, base::Seconds(60),
      base::BindOnce(
          &NativeScreenCapturePickerMac::CleanupContentFilter,
          // Passing `this` is safe since
          // `cached_content_filters_cleanup_timers_` is owned by `this`.
          base::Unretained(this), id));
}

void NativeScreenCapturePickerMac::CleanupContentFilter(DesktopMediaID::Id id) {
  NSNumber* source_id = @(id);
  [[picker_observer_ contentFilters] removeObjectForKey:source_id];

  NSEnumerator* streamEnumerator =
      [[picker_observer_ streamToIdMapping] keyEnumerator];
  NSMutableArray* streamsToBeRemoved = [NSMutableArray array];
  while (SCStream* stream = [streamEnumerator nextObject]) {
    // Streams to be removed need to be stored in a separate array and cannot be
    // directly removed in this loop because `NSMapTable` doesn't allow
    // enumeration and mutation at the same time.
    if ([[picker_observer_ streamToIdMapping] objectForKey:stream] ==
        source_id) {
      [streamsToBeRemoved addObject:stream];
    }
  }

  for (SCStream* stream in streamsToBeRemoved) {
    [[picker_observer_ streamToIdMapping] removeObjectForKey:stream];
  }

  cached_content_filters_cleanup_timers_.erase(id);

  VLOG(1) << "NSCPM: CleanupContentFilter: source_id = " << id
          << ", contentFilters.count = " <<
      [[picker_observer_ contentFilters] count]
          << ", streamToIdMapping.count = " <<
      [[picker_observer_ streamToIdMapping] count];
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
