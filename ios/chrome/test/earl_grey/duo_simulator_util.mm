// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/duo_simulator_util.h"

#if TARGET_OS_SIMULATOR
#import <dlfcn.h>
#import <mach/mach.h>
#endif

#import <algorithm>
#import <optional>

#import "base/threading/platform_thread.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/web/common/uikit_ui_util.h"

// How iPhone Duo Simulator Posture & Orientation Control Works:
//
// Standard `XCUIDevice.sharedDevice.orientation` calls do not drive the
// foldable posture/orientation state machine on the iPhone Duo (`iPhone19,4`)
// simulator. Instead, `Simulator.app` (`DeviceHub.app`) controls the simulated
// hinge and device orientation by sending binary-serialized property list
// dictionaries (`provider = com.apple.Virtualization.VirtualMachines`,
// `source = hinge-slider-control` or `orientation-picker-control`) inside
// vendor-defined IOHID events (`PrimaryUsagePage = 0xFF61`,
// `PrimaryUsage = 0x005B`). Inside the simulator runtime, `locationd`
// subscribes to those HID events via `IOHIDEventSystemClient` and forwards the
// resulting device posture and orientation transitions to `SpringBoard`.
//
// Troubleshooting Future Simulator Releases:
//
// If a future Xcode or iOS Simulator release changes this protocol, inspect
// `DeviceHub.app`'s device control extensions (`strings` / `otool`) for updated
// HID usage pages or payload dictionary keys, and stream `locationd` and
// `SpringBoard` logs while interacting with the hinge slider or orientation
// buttons in `Simulator.app`:
//   xcrun simctl spawn booted log stream --level debug \
//       --predicate 'process IN {"locationd", "SpringBoard"}'

namespace {

NSString* const kDuoSimulatorModelIdentifier = @"iPhone19,4";

#if TARGET_OS_SIMULATOR
// Vendor-defined HID usage monitored by locationd for simulator posture events.
constexpr uint32_t kVendorDefinedUsagePage = 0xFF61;
constexpr uint32_t kVendorDefinedUsage = 0x005B;
constexpr int32_t kVirtualEventSystemClientType = 4;
constexpr CFOptionFlags kIOCFSerializeToBinary = 1;

constexpr double kClosedAngleMaxDegrees = 5.0;
constexpr CGFloat kCoverDisplayMaxShortDimension = 500.0;
constexpr CGFloat kInnerDisplayMinShortDimension = 600.0;

constexpr base::TimeDelta kVirtualServiceRegistrationDelay =
    base::Milliseconds(50);

NSString* const kPayloadProviderKey = @"provider";
NSString* const kPayloadProviderValue =
    @"com.apple.Virtualization.VirtualMachines";
NSString* const kPayloadSourceKey = @"source";
NSString* const kPayloadHingeSliderSource = @"hinge-slider-control";
NSString* const kPayloadOrientationPickerSource = @"orientation-picker-control";
NSString* const kPayloadTypeKey = @"type";
NSString* const kPayloadRangeType = @"range";
NSString* const kPayloadEnumType = @"enum";
NSString* const kPayloadValueKey = @"value";

NSString* const kOrientationPortraitValue = @"portrait";
NSString* const kOrientationPortraitUpsideDownValue = @"pud";
NSString* const kOrientationLandscapeLeftValue = @"landscape-left";
NSString* const kOrientationLandscapeRightValue = @"landscape-right";

void VirtualServiceNotificationCallback(void* target,
                                        void* refcon,
                                        void* service,
                                        uint32_t type,
                                        CFDictionaryRef info) {}

Boolean VirtualServiceSetPropertyCallback(void* target,
                                          void* refcon,
                                          void* service,
                                          CFStringRef key,
                                          CFTypeRef value) {
  return false;
}

CFTypeRef VirtualServiceCopyPropertyCallback(void* target,
                                             void* refcon,
                                             void* service,
                                             CFStringRef key) {
  NSDictionary* properties = (__bridge NSDictionary*)target;
  id value = properties[(__bridge NSString*)key];
  return value ? CFBridgingRetain(value) : nullptr;
}

CFTypeRef VirtualServiceCopyEventCallback(void* target,
                                          void* refcon,
                                          void* service,
                                          uint32_t type,
                                          CFTypeRef matching,
                                          uint32_t options) {
  return nullptr;
}

int32_t VirtualServiceSetOutputEventCallback(void* target,
                                             void* refcon,
                                             void* service,
                                             CFTypeRef event) {
  return 0;
}

// Callback table for IOHIDVirtualServiceClientCreateWithCallbacks.
struct IOHIDVirtualServiceClientCallbacks {
  decltype(&VirtualServiceNotificationCallback) notification;
  decltype(&VirtualServiceSetPropertyCallback) set_property;
  decltype(&VirtualServiceCopyPropertyCallback) copy_property;
  decltype(&VirtualServiceCopyEventCallback) copy_event;
  decltype(&VirtualServiceSetOutputEventCallback) set_output_event;
};

bool DispatchSimulatorVendorDefinedHIDPayload(NSDictionary* payload) {
  using IOCFSerializeFn = CFDataRef (*)(CFTypeRef, CFOptionFlags);
  using IOHIDEventCreateVendorDefinedEventFn =
      CFTypeRef (*)(CFAllocatorRef, uint64_t, uint32_t, uint32_t, uint32_t,
                    const uint8_t*, CFIndex, uint32_t);
  using IOHIDEventSystemClientCreateWithTypeFn =
      CFTypeRef (*)(CFAllocatorRef, int32_t, CFDictionaryRef);
  using IOHIDEventSystemClientSetDispatchQueueFn =
      void (*)(CFTypeRef, dispatch_queue_t);
  using IOHIDEventSystemClientActivateFn = void (*)(CFTypeRef);
  using IOHIDVirtualServiceClientCreateWithCallbacksFn =
      CFTypeRef (*)(CFTypeRef, CFDictionaryRef, const void*, void*, void*);
  using IOHIDVirtualServiceClientDispatchEventFn =
      Boolean (*)(CFTypeRef, CFTypeRef);

  static dispatch_once_t once_token;
  static dispatch_queue_t s_queue = nullptr;
  // Intentionally leaked for the lifetime of the test host.
  static CFTypeRef s_system_client = nullptr;
  static CFTypeRef s_virtual_service = nullptr;
  static IOCFSerializeFn s_serialize = nullptr;
  static IOHIDEventCreateVendorDefinedEventFn s_create_event = nullptr;
  static IOHIDVirtualServiceClientDispatchEventFn s_service_dispatch = nullptr;

  // 1. Lazily resolve private IOKit HID symbols and register a persistent
  // virtual HID service advertising UsagePage 0xFF61 / Usage 0x005B so
  // `locationd` subscribes to our events.
  dispatch_once(&once_token, ^{
    void* iokit =
        dlopen("/System/Library/Frameworks/IOKit.framework/IOKit", RTLD_LAZY);
    if (!iokit) {
      return;
    }
    s_serialize =
        reinterpret_cast<IOCFSerializeFn>(dlsym(iokit, "IOCFSerialize"));
    s_create_event = reinterpret_cast<IOHIDEventCreateVendorDefinedEventFn>(
        dlsym(iokit, "IOHIDEventCreateVendorDefinedEvent"));
    auto create_client =
        reinterpret_cast<IOHIDEventSystemClientCreateWithTypeFn>(
            dlsym(iokit, "IOHIDEventSystemClientCreateWithType"));
    auto set_queue = reinterpret_cast<IOHIDEventSystemClientSetDispatchQueueFn>(
        dlsym(iokit, "IOHIDEventSystemClientSetDispatchQueue"));
    auto activate_client = reinterpret_cast<IOHIDEventSystemClientActivateFn>(
        dlsym(iokit, "IOHIDEventSystemClientActivate"));
    auto create_virtual_service =
        reinterpret_cast<IOHIDVirtualServiceClientCreateWithCallbacksFn>(
            dlsym(iokit, "IOHIDVirtualServiceClientCreateWithCallbacks"));
    s_service_dispatch =
        reinterpret_cast<IOHIDVirtualServiceClientDispatchEventFn>(
            dlsym(iokit, "IOHIDVirtualServiceClientDispatchEvent"));

    if (!create_client || !set_queue || !activate_client ||
        !create_virtual_service || !s_service_dispatch) {
      return;
    }

    s_queue = dispatch_queue_create("org.chromium.ios.duo_hid_service",
                                    DISPATCH_QUEUE_SERIAL);
    NSDictionary* properties = @{
      @"PrimaryUsagePage" : @(kVendorDefinedUsagePage),
      @"PrimaryUsage" : @(kVendorDefinedUsage),
      @"DeviceUsagePairs" : @[
        @{
          @"DeviceUsagePage" : @(kVendorDefinedUsagePage),
          @"DeviceUsage" : @(kVendorDefinedUsage),
        },
      ],
    };
    CFTypeRef retained_properties = CFBridgingRetain(properties);

    static const IOHIDVirtualServiceClientCallbacks kCallbacks = {
        &VirtualServiceNotificationCallback,
        &VirtualServiceSetPropertyCallback,
        &VirtualServiceCopyPropertyCallback,
        &VirtualServiceCopyEventCallback,
        &VirtualServiceSetOutputEventCallback,
    };

    // Create and activate the virtual HID service on its dedicated serial
    // queue.
    dispatch_sync(s_queue, ^{
      s_system_client = create_client(kCFAllocatorDefault,
                                      kVirtualEventSystemClientType, nullptr);
      if (s_system_client) {
        set_queue(s_system_client, s_queue);
        activate_client(s_system_client);
        s_virtual_service = create_virtual_service(
            s_system_client, static_cast<CFDictionaryRef>(retained_properties),
            &kCallbacks, const_cast<void*>(retained_properties), nullptr);
      }
    });
    // Wait briefly for locationd to match the new virtual service.
    base::PlatformThread::Sleep(kVirtualServiceRegistrationDelay);
  });

  if (!s_serialize || !s_create_event || !s_virtual_service ||
      !s_service_dispatch || !s_queue) {
    return false;
  }

  // 2. Serialize the control payload dictionary into binary IOCF format.
  CFDataRef serialized =
      s_serialize((__bridge CFTypeRef)payload, kIOCFSerializeToBinary);
  if (!serialized) {
    return false;
  }

  // 3. Wrap the binary payload in a vendor-defined IOHIDEvent and dispatch it
  // through the virtual HID service.
  __block bool dispatched = false;
  dispatch_sync(s_queue, ^{
    CFTypeRef event = s_create_event(
        kCFAllocatorDefault, mach_absolute_time(), kVendorDefinedUsagePage,
        kVendorDefinedUsage, 0, CFDataGetBytePtr(serialized),
        CFDataGetLength(serialized), 0);
    if (event) {
      dispatched = s_service_dispatch(s_virtual_service, event);
      CFRelease(event);
    }
  });
  CFRelease(serialized);
  return dispatched;
}

#if defined(__IPHONE_27_1) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_27_1
constexpr double kOpenAngleMinDegrees = 175.0;

std::optional<UIHingeStatus> g_latest_hinge_status API_AVAILABLE(ios(27.1));

UIHingeStatus ExpectedHingeStatusForAngle(double angle_in_degrees)
    API_AVAILABLE(ios(27.1)) {
  if (angle_in_degrees <= kClosedAngleMaxDegrees) {
    return UIHingeStatusClosed;
  }
  if (angle_in_degrees < kOpenAngleMinDegrees) {
    return UIHingeStatusPartiallyOpen;
  }
  return UIHingeStatusFullyOpen;
}
#endif

// Installs `UIHingeInteraction` on `window` if not already present.
void EnsureHingeInteractionInstalled(UIWindow* window) {
  if (!window) {
    return;
  }
#if defined(__IPHONE_27_1) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_27_1
  if (@available(iOS 27.1, *)) {
    for (id<UIInteraction> existing in window.interactions) {
      if ([existing isKindOfClass:[UIHingeInteraction class]]) {
        return;
      }
    }
    UIHingeInteraction* interaction = [[UIHingeInteraction alloc]
        initWithUpdateHandler:^(UIHingeInteraction*,
                                UIHingeInteractionUpdate* update) {
          UIHinge* hinge = update.hinge;
          g_latest_hinge_status =
              (hinge && hinge.status != UIHingeStatusUnknown)
                  ? hinge.status
                  : UIHingeStatusClosed;
        }];
    [window addInteraction:interaction];
  }
#endif
}

bool IsWindowTransitionSettled(UIWindow* window) {
  if (!window) {
    return false;
  }
  if (window.rootViewController.transitionCoordinator != nil) {
    return false;
  }
  if (window.layer.animationKeys.count > 0) {
    return false;
  }
  return true;
}

bool IsWindowInExpectedHingeStatus(UIWindow* window, double angle_in_degrees) {
  if (!IsWindowTransitionSettled(window)) {
    return false;
  }
  CGFloat min_dimension =
      std::min(window.bounds.size.width, window.bounds.size.height);
  bool expect_closed = (angle_in_degrees <= kClosedAngleMaxDegrees);
  bool display_matches = expect_closed
                             ? (min_dimension < kCoverDisplayMaxShortDimension)
                             : (min_dimension > kInnerDisplayMinShortDimension);
  if (!display_matches) {
    return false;
  }
  // On iOS 27.1+, use `UIHingeInteraction` to distinguish Book from Open.
#if defined(__IPHONE_27_1) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_27_1
  if (@available(iOS 27.1, *)) {
    UIHingeStatus expected_status =
        ExpectedHingeStatusForAngle(angle_in_degrees);
    if (expected_status != UIHingeStatusClosed) {
      return g_latest_hinge_status == expected_status;
    }
  }
#endif
  return true;
}

#endif  // TARGET_OS_SIMULATOR

}  // namespace

bool IsDuoSimulator() {
  return [NSProcessInfo.processInfo.environment[@"SIMULATOR_MODEL_IDENTIFIER"]
      isEqualToString:kDuoSimulatorModelIdentifier];
}

bool DispatchSimulatedDuoHingeAngle(double angle_in_degrees) {
#if TARGET_OS_SIMULATOR
  UIWindow* window = GetAnyKeyWindow();
  EnsureHingeInteractionInstalled(window);
#if defined(__IPHONE_27_1) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_27_1
  if (@available(iOS 27.1, *)) {
    if (!IsSimulatedDuoHingePostureSettled(angle_in_degrees)) {
      g_latest_hinge_status = std::nullopt;
    }
  }
#endif
  NSDictionary* payload = @{
    kPayloadProviderKey : kPayloadProviderValue,
    kPayloadSourceKey : kPayloadHingeSliderSource,
    kPayloadTypeKey : kPayloadRangeType,
    kPayloadValueKey : @(angle_in_degrees),
  };
  return DispatchSimulatorVendorDefinedHIDPayload(payload);
#else
  return false;
#endif
}

bool IsSimulatedDuoHingePostureSettled(double angle_in_degrees) {
#if TARGET_OS_SIMULATOR
  UIWindow* window = GetAnyKeyWindow();
  EnsureHingeInteractionInstalled(window);
  return IsWindowInExpectedHingeStatus(window, angle_in_degrees);
#else
  return false;
#endif
}

bool DispatchSimulatedDuoOrientation(UIDeviceOrientation orientation) {
#if TARGET_OS_SIMULATOR
  NSString* value = nil;
  switch (orientation) {
    case UIDeviceOrientationPortrait:
      value = kOrientationPortraitValue;
      break;
    case UIDeviceOrientationPortraitUpsideDown:
      value = kOrientationPortraitUpsideDownValue;
      break;
    case UIDeviceOrientationLandscapeLeft:
      value = kOrientationLandscapeLeftValue;
      break;
    case UIDeviceOrientationLandscapeRight:
      value = kOrientationLandscapeRightValue;
      break;
    default:
      return false;
  }

  NSDictionary* payload = @{
    kPayloadProviderKey : kPayloadProviderValue,
    kPayloadSourceKey : kPayloadOrientationPickerSource,
    kPayloadTypeKey : kPayloadEnumType,
    kPayloadValueKey : value,
  };
  return DispatchSimulatorVendorDefinedHIDPayload(payload);
#else
  return false;
#endif
}

bool IsSimulatedDuoOrientationSettled(UIDeviceOrientation orientation) {
#if TARGET_OS_SIMULATOR
  UIWindow* window = GetAnyKeyWindow();
  if (!IsWindowTransitionSettled(window)) {
    return false;
  }
  bool device_is_portrait = UIDeviceOrientationIsPortrait(orientation);
  CGFloat min_dimension =
      std::min(window.bounds.size.width, window.bounds.size.height);
  bool is_inner_display = (min_dimension > kInnerDisplayMinShortDimension);

  // The cover display (466x678) is portrait when the device is held in
  // portrait. Unfolding the inner display (951x669) makes the screen wider
  // than tall in portrait, so its interface orientation is inverted relative to
  // the device orientation (portrait device -> landscape interface, and vice
  // versa).
  bool expect_portrait_interface =
      is_inner_display ? !device_is_portrait : device_is_portrait;
  UIInterfaceOrientation current = GetInterfaceOrientation();
  bool interface_matches = expect_portrait_interface
                               ? UIInterfaceOrientationIsPortrait(current)
                               : UIInterfaceOrientationIsLandscape(current);
  bool bounds_match =
      expect_portrait_interface
          ? (window.bounds.size.width < window.bounds.size.height)
          : (window.bounds.size.width > window.bounds.size.height);
  return interface_matches && bounds_match;
#else
  return false;
#endif
}
