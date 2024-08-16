// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/native_screen_capture_picker_mac.h"

#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include "content/browser/media/capture/screen_capture_kit_device_mac.h"
#include "content/public/browser/desktop_media_id.h"
#include "media/capture/video/video_capture_device.h"

using Source = webrtc::DesktopCapturer::Source;
using PickerCallback = base::OnceCallback<void(Source)>;
using PickerCancelCallback = base::OnceCallback<void()>;
using PickerErrorCallback = base::OnceCallback<void()>;

API_AVAILABLE(macos(14.0))
@interface PickerObserver : NSObject <SCContentSharingPickerObserver>
- (instancetype)initWithPickerCallback:(PickerCallback)pickerCallback
                        cancelCallback:(PickerCancelCallback)cancelCallback
                         errorCallback:(PickerErrorCallback)errorCallback
                        assignSourceId:(int)assignedSourceId;
@property(strong, readonly) SCContentFilter* contentFilter;
@end

@implementation PickerObserver {
  PickerCallback _pickerCallback;
  PickerCancelCallback _cancelCallback;
  PickerErrorCallback _errorCallback;
  int _assignedSourceId;
}

@synthesize contentFilter;

- (instancetype)initWithPickerCallback:(PickerCallback)pickerCallback
                        cancelCallback:(PickerCancelCallback)cancelCallback
                         errorCallback:(PickerErrorCallback)errorCallback
                        assignSourceId:(int)assignedSourceId {
  if (self = [super init]) {
    _pickerCallback = std::move(pickerCallback);
    _cancelCallback = std::move(cancelCallback);
    _errorCallback = std::move(errorCallback);
    _assignedSourceId = assignedSourceId;
  }
  return self;
}

- (void)contentSharingPicker:(SCContentSharingPicker*)picker
         didUpdateWithFilter:(SCContentFilter*)filter
                   forStream:(SCStream*)stream {
  contentFilter = filter;

  Source source;
  source.id = _assignedSourceId;
  if (_pickerCallback) {
    std::move(_pickerCallback).Run(source);
  }
}

- (void)contentSharingPicker:(SCContentSharingPicker*)picker
          didCancelForStream:(SCStream*)stream {
  if (_cancelCallback) {
    std::move(_cancelCallback).Run();
  }
}

- (void)contentSharingPickerStartDidFailWithError:(NSError*)error {
  if (_errorCallback) {
    std::move(_errorCallback).Run();
  }
}
@end

namespace content {

class API_AVAILABLE(macos(14.0)) NativeScreenCapturePickerMac
    : public NativeScreenCapturePicker {
 public:
  NativeScreenCapturePickerMac();
  ~NativeScreenCapturePickerMac() override;

  void Open(DesktopMediaID::Type type,
            base::OnceCallback<void(Source)> picker_callback,
            base::OnceCallback<void()> cancel_callback,
            base::OnceCallback<void()> error_callback) override;
  void Close(DesktopMediaID device_id) override;
  std::unique_ptr<media::VideoCaptureDevice> CreateDevice(
      const DesktopMediaID& source) override;

  base::WeakPtr<NativeScreenCapturePicker> GetWeakPtr() override;

 private:
  NSMutableDictionary<NSNumber*, PickerObserver*>* __strong picker_observers_;
  DesktopMediaID::Id next_id_ = 0;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<NativeScreenCapturePickerMac> weak_ptr_factory_{this};
};

NativeScreenCapturePickerMac::NativeScreenCapturePickerMac()
    : picker_observers_(
          [[NSMutableDictionary<NSNumber*, PickerObserver*> alloc] init]) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

NativeScreenCapturePickerMac::~NativeScreenCapturePickerMac() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NativeScreenCapturePickerMac::Open(
    DesktopMediaID::Type type,
    base::OnceCallback<void(Source)> picker_callback,
    base::OnceCallback<void()> cancel_callback,
    base::OnceCallback<void()> error_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(type == DesktopMediaID::Type::TYPE_SCREEN ||
        type == DesktopMediaID::Type::TYPE_WINDOW);
  if (@available(macOS 14.0, *)) {
    NSNumber* source_id = @(next_id_);
    auto picker_observer = [[PickerObserver alloc]
        initWithPickerCallback:(std::move(picker_callback))
                cancelCallback:(std::move(cancel_callback))errorCallback
                              :(std::move(error_callback))assignSourceId
                              :next_id_];
    picker_observers_[source_id] = picker_observer;
    ++next_id_;
    SCContentSharingPicker* picker = [SCContentSharingPicker sharedPicker];
    [picker addObserver:picker_observer];
    picker.active = true;
    SCContentSharingPickerConfiguration* config = [picker defaultConfiguration];
    // Limits the maximum number of screen/window capture to 5.
    NSNumber* max_stream_count = @5;
    if (type == DesktopMediaID::Type::TYPE_SCREEN) {
      config.allowedPickerModes = SCContentSharingPickerModeSingleDisplay;
      picker.defaultConfiguration = config;
      picker.maximumStreamCount = max_stream_count;
      [picker presentPickerUsingContentStyle:SCShareableContentStyleDisplay];
    } else {
      config.allowedPickerModes = SCContentSharingPickerModeSingleWindow;
      picker.defaultConfiguration = config;
      picker.maximumStreamCount = max_stream_count;
      [picker presentPickerUsingContentStyle:SCShareableContentStyleWindow];
    }
  } else {
    NOTREACHED();
  }
}

void NativeScreenCapturePickerMac::Close(DesktopMediaID device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (@available(macOS 14.0, *)) {
    NSNumber* source_id = @(device_id.id);
    PickerObserver* picker_observer = picker_observers_[source_id];
    if (!picker_observer) {
      return;
    }
    [picker_observers_ removeObjectForKey:source_id];
    SCContentSharingPicker* picker = [SCContentSharingPicker sharedPicker];
    [picker removeObserver:picker_observer];
    // Don't deactivate the picker if there are any active picker observers.
    if ([picker_observers_ count] > 0) {
      return;
    }
    picker.active = false;
  } else {
    NOTREACHED();
  }
}

std::unique_ptr<media::VideoCaptureDevice>
NativeScreenCapturePickerMac::CreateDevice(const DesktopMediaID& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NSNumber* source_id = @(source.id);
  PickerObserver* picker_observer = [picker_observers_ objectForKey:source_id];
  SCContentFilter* filter =
      picker_observer ? [picker_observer contentFilter] : nullptr;
  std::unique_ptr<media::VideoCaptureDevice> device =
      CreateScreenCaptureKitDeviceMac(source, filter);
  return device;
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
