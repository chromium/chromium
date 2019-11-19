// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/touch_injector_win.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "remoting/proto/event.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/win/screen_capture_utils.h"

namespace remoting {

using protocol::TouchEvent;
using protocol::TouchEventPoint;

namespace {

typedef BOOL(NTAPI* InitializeTouchInjectionFunction)(UINT32, DWORD);
typedef BOOL(NTAPI* InjectTouchInputFunction)(UINT32,
                                              const POINTER_TOUCH_INFO*);
const uint32_t kMaxSimultaneousTouchCount = 10;

// This is used to reinject all points that have not changed as "move"ed points,
// even if they have not actually moved.
// This is required for multi-touch to work, e.g. pinching and zooming gestures
// (handled by apps) won't work without reinjecting the points, even though the
// user moved only one finger and held the other finger in place.
void AppendMapValuesToVector(
    std::map<uint32_t, POINTER_TOUCH_INFO>* touches_in_contact,
    std::vector<POINTER_TOUCH_INFO>* output_vector) {
  for (auto& id_and_pointer_touch_info : *touches_in_contact) {
    POINTER_TOUCH_INFO& pointer_touch_info = id_and_pointer_touch_info.second;
    output_vector->push_back(pointer_touch_info);
  }
}

void ConvertToPointerTouchInfoImpl(
    const TouchEventPoint& touch_point,
    POINTER_TOUCH_INFO* pointer_touch_info) {
  pointer_touch_info->touchMask =
      TOUCH_MASK_CONTACTAREA | TOUCH_MASK_ORIENTATION;
  pointer_touch_info->touchFlags = TOUCH_FLAG_NONE;

  // Although radius_{x,y} can be undefined (i.e. has_radius_{x,y} == false),
  // the default value (0.0) will set the area correctly.
  // MSDN mentions that if the digitizer does not detect the size of the touch
  // point, rcContact should be set to 0 by 0 rectangle centered at the
  // coordinate.
  pointer_touch_info->rcContact.left =
      touch_point.x() - touch_point.radius_x();
  pointer_touch_info->rcContact.top = touch_point.y() - touch_point.radius_y();
  pointer_touch_info->rcContact.right =
      touch_point.x() + touch_point.radius_x();
  pointer_touch_info->rcContact.bottom =
      touch_point.y() + touch_point.radius_y();

  pointer_touch_info->orientation = touch_point.angle();

  if (touch_point.has_pressure()) {
    pointer_touch_info->touchMask |= TOUCH_MASK_PRESSURE;
    const float kMinimumPressure = 0.0;
    const float kMaximumPressure = 1.0;
    const float clamped_touch_point_pressure =
        std::max(kMinimumPressure,
                 std::min(kMaximumPressure, touch_point.pressure()));

    const int kWindowsMaxTouchPressure = 1024;  // Defined in MSDN.
    const int pressure =
        clamped_touch_point_pressure * kWindowsMaxTouchPressure;
    pointer_touch_info->pressure = pressure;
  }

  pointer_touch_info->pointerInfo.pointerType = PT_TOUCH;
  pointer_touch_info->pointerInfo.pointerId = touch_point.id();
  pointer_touch_info->pointerInfo.ptPixelLocation.x = touch_point.x();
  pointer_touch_info->pointerInfo.ptPixelLocation.y = touch_point.y();
}

// The caller should set memset(0) the struct and set
// pointer_touch_info->pointerInfo.pointerFlags.
void ConvertToPointerTouchInfo(
    const TouchEventPoint& touch_point,
    POINTER_TOUCH_INFO* pointer_touch_info) {
  // TODO(zijiehe): Use GetFullscreenTopLeft() once
  // https://chromium-review.googlesource.com/c/581951/ is submitted.
  webrtc::DesktopVector top_left = webrtc::GetScreenRect(
      webrtc::kFullDesktopScreenId, std::wstring()).top_left();
  if (top_left.is_zero()) {
    ConvertToPointerTouchInfoImpl(touch_point, pointer_touch_info);
    return;
  }

  TouchEventPoint point(touch_point);
  point.set_x(point.x() + top_left.x());
  point.set_y(point.y() + top_left.y());

  ConvertToPointerTouchInfoImpl(point, pointer_touch_info);
}

}  // namespace

TouchInjectorWinDelegate::~TouchInjectorWinDelegate() {}

// static.
std::unique_ptr<TouchInjectorWinDelegate> TouchInjectorWinDelegate::Create() {
  base::ScopedNativeLibrary library(base::FilePath(L"User32.dll"));
  if (!library.is_valid()) {
    PLOG(INFO) << "Failed to get library module for touch injection functions.";
    return std::unique_ptr<TouchInjectorWinDelegate>();
  }

  InitializeTouchInjectionFunction init_func =
      reinterpret_cast<InitializeTouchInjectionFunction>(
          library.GetFunctionPointer("InitializeTouchInjection"));
  if (!init_func) {
    PLOG(INFO) << "Failed to get InitializeTouchInjection function handle.";
    return std::unique_ptr<TouchInjectorWinDelegate>();
  }

  InjectTouchInputFunction inject_touch_func =
      reinterpret_cast<InjectTouchInputFunction>(
          library.GetFunctionPointer("InjectTouchInput"));
  if (!inject_touch_func) {
    PLOG(INFO) << "Failed to get InjectTouchInput.";
    return std::unique_ptr<TouchInjectorWinDelegate>();
  }

  return std::unique_ptr<TouchInjectorWinDelegate>(new TouchInjectorWinDelegate(
      library.release(), init_func, inject_touch_func));
}

TouchInjectorWinDelegate::TouchInjectorWinDelegate(
    base::NativeLibrary library,
    InitializeTouchInjectionFunction initialize_touch_injection_func,
    InjectTouchInputFunction inject_touch_input_func)
    : library_module_(library),
      initialize_touch_injection_func_(initialize_touch_injection_func),
      inject_touch_input_func_(inject_touch_input_func) {}

BOOL TouchInjectorWinDelegate::InitializeTouchInjection(UINT32 max_count,
                                                        DWORD dw_mode) {
  return initialize_touch_injection_func_(max_count, dw_mode);
}

DWORD TouchInjectorWinDelegate::InjectTouchInput(
    UINT32 count,
    const POINTER_TOUCH_INFO* contacts) {
  return inject_touch_input_func_(count, contacts);
}

TouchInjectorWin::TouchInjectorWin() = default;

TouchInjectorWin::~TouchInjectorWin() = default;

// Note that TouchInjectorWinDelegate::Create() is not called in this method
// so that a mock delegate can be injected in tests and set expectations on the
// mock and return value of this method.
bool TouchInjectorWin::Init() {
  if (!delegate_)
    delegate_ = TouchInjectorWinDelegate::Create();

  // If initializing the delegate failed above, then the platform likely doesn't
  // support touch (or the libraries failed to load for some reason).
  if (!delegate_)
    return false;

  if (!delegate_->InitializeTouchInjection(
          kMaxSimultaneousTouchCount, TOUCH_FEEDBACK_DEFAULT)) {
    // delagate_ is reset here so that the function that need the delegate
    // can check if it is null.
    delegate_.reset();
    PLOG(INFO) << "Failed to initialize touch injection.";
    return false;
  }

  return true;
}

void TouchInjectorWin::Deinitialize() {
  touches_in_contact_.clear();
  // Same reason as TouchInjectorWin::Init(). For injecting mock delegates for
  // tests, a new delegate is created here.
  delegate_ = TouchInjectorWinDelegate::Create();
}

void TouchInjectorWin::InjectTouchEvent(const TouchEvent& event) {
  if (!delegate_) {
    VLOG(3) << "Touch injection functions are not initialized.";
    return;
  }

  switch (event.event_type()) {
    case TouchEvent::TOUCH_POINT_START:
      AddNewTouchPoints(event);
      break;
    case TouchEvent::TOUCH_POINT_MOVE:
      MoveTouchPoints(event);
      break;
    case TouchEvent::TOUCH_POINT_END:
      EndTouchPoints(event);
      break;
    case TouchEvent::TOUCH_POINT_CANCEL:
      CancelTouchPoints(event);
      break;
    default:
      NOTREACHED();
      return;
  }
}

void TouchInjectorWin::SetInjectorDelegateForTest(
    std::unique_ptr<TouchInjectorWinDelegate> functions) {
  delegate_ = std::move(functions);
}

void TouchInjectorWin::AddNewTouchPoints(const TouchEvent& event) {
  DCHECK_EQ(event.event_type(), TouchEvent::TOUCH_POINT_START);

  std::vector<POINTER_TOUCH_INFO> touches;
  // Must inject already touching points as move events.
  AppendMapValuesToVector(&touches_in_contact_, &touches);

  for (const TouchEventPoint& touch_point : event.touch_points()) {
    POINTER_TOUCH_INFO pointer_touch_info;
    memset(&pointer_touch_info, 0, sizeof(pointer_touch_info));
    pointer_touch_info.pointerInfo.pointerFlags =
        POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT | POINTER_FLAG_DOWN;
    ConvertToPointerTouchInfo(touch_point, &pointer_touch_info);
    touches.push_back(pointer_touch_info);

    // All points in the map should be a move point.
    pointer_touch_info.pointerInfo.pointerFlags =
        POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT | POINTER_FLAG_UPDATE;
    touches_in_contact_[touch_point.id()] = pointer_touch_info;
  }

  if (delegate_->InjectTouchInput(touches.size(), touches.data()) == 0) {
    PLOG(ERROR) << "Failed to inject a touch start event.";
  }
}

void TouchInjectorWin::MoveTouchPoints(const TouchEvent& event) {
  DCHECK_EQ(event.event_type(), TouchEvent::TOUCH_POINT_MOVE);

  for (const TouchEventPoint& touch_point : event.touch_points()) {
    POINTER_TOUCH_INFO* pointer_touch_info =
        &touches_in_contact_[touch_point.id()];
    memset(pointer_touch_info, 0, sizeof(*pointer_touch_info));
    pointer_touch_info->pointerInfo.pointerFlags =
        POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT | POINTER_FLAG_UPDATE;
    ConvertToPointerTouchInfo(touch_point, pointer_touch_info);
  }

  std::vector<POINTER_TOUCH_INFO> touches;
  // Must inject already touching points as move events.
  AppendMapValuesToVector(&touches_in_contact_, &touches);
  if (delegate_->InjectTouchInput(touches.size(), touches.data()) == 0) {
    PLOG(ERROR) << "Failed to inject a touch move event.";
  }
}

void TouchInjectorWin::EndTouchPoints(const TouchEvent& event) {
  DCHECK_EQ(event.event_type(), TouchEvent::TOUCH_POINT_END);

  std::vector<POINTER_TOUCH_INFO> touches;
  for (const TouchEventPoint& touch_point : event.touch_points()) {
    POINTER_TOUCH_INFO pointer_touch_info =
        touches_in_contact_[touch_point.id()];
    pointer_touch_info.pointerInfo.pointerFlags = POINTER_FLAG_UP;

    touches_in_contact_.erase(touch_point.id());
    touches.push_back(pointer_touch_info);
  }

  AppendMapValuesToVector(&touches_in_contact_, &touches);
  if (delegate_->InjectTouchInput(touches.size(), touches.data()) == 0) {
    PLOG(ERROR) << "Failed to inject a touch end event.";
  }
}

void TouchInjectorWin::CancelTouchPoints(const TouchEvent& event) {
  DCHECK_EQ(event.event_type(), TouchEvent::TOUCH_POINT_CANCEL);

  std::vector<POINTER_TOUCH_INFO> touches;
  for (const TouchEventPoint& touch_point : event.touch_points()) {
    POINTER_TOUCH_INFO pointer_touch_info =
        touches_in_contact_[touch_point.id()];
    pointer_touch_info.pointerInfo.pointerFlags =
        POINTER_FLAG_UP | POINTER_FLAG_CANCELED;

    touches_in_contact_.erase(touch_point.id());
    touches.push_back(pointer_touch_info);
  }

  AppendMapValuesToVector(&touches_in_contact_, &touches);
  if (delegate_->InjectTouchInput(touches.size(), touches.data()) == 0) {
    PLOG(ERROR) << "Failed to inject a touch cancel event.";
  }
}

}  // namespace remoting
