// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <cmath>

#include "ppapi/c/ppb_console.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/mouse_lock.h"
#include "ppapi/cpp/fullscreen.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/var.h"
#include "ppapi/utility/completion_callback_factory.h"

class MyInstance : public pp::Instance, public pp::MouseLock {
 public:
  explicit MyInstance(PP_Instance instance)
      : pp::Instance(instance),
        pp::MouseLock(this),
        width_(0),
        height_(0),
        mouse_locked_(false),
        pending_paint_(false),
        waiting_for_flush_completion_(false),
        callback_factory_(this),
        console_(NULL),
        fullscreen_(this) {
  }
  virtual ~MyInstance() {}

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    console_ = reinterpret_cast<const PPB_Console*>(
        pp::Module::Get()->GetBrowserInterface(PPB_CONSOLE_INTERFACE));
    if (!console_)
      return false;

    RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE |
                       PP_INPUTEVENT_CLASS_KEYBOARD);
    return true;
  }

  virtual bool HandleInputEvent(const pp::InputEvent& event) {
    switch (event.GetType()) {
      case PP_INPUTEVENT_TYPE_MOUSEDOWN: {
        pp::MouseInputEvent mouse_event(event);
        if (mouse_event.GetButton() == PP_INPUTEVENT_MOUSEBUTTON_LEFT &&
            !mouse_locked_) {
          LockMouse(callback_factory_.NewCallback(&MyInstance::DidLockMouse));
        }
        return true;
      }
      case PP_INPUTEVENT_TYPE_MOUSEMOVE: {
        pp::MouseInputEvent mouse_event(event);
        mouse_movement_ = mouse_event.GetMovement();
        static unsigned int i = 0;
        Log(PP_LOGLEVEL_LOG, "[%d] movementX: %d; movementY: %d\n", i++,
            mouse_movement_.x(), mouse_movement_.y());
        Paint();
        return true;
      }
      case PP_INPUTEVENT_TYPE_KEYDOWN: {
        pp::KeyboardInputEvent key_event(event);
        if (key_event.GetKeyCode() == 13) {
          // Lock the mouse when the Enter key is pressed.
          if (mouse_locked_)
            UnlockMouse();
          else
            LockMouse(callback_factory_.NewCallback(&MyInstance::DidLockMouse));
          return true;
        } else if (key_event.GetKeyCode() == 70) {
          // Enter Flash fullscreen mode when the 'f' key is pressed.
          if (!fullscreen_.IsFullscreen())
            fullscreen_.SetFullscreen(true);
          return true;
        }
        return false;
      }
      default:
        return false;
    }
  }

  virtual void DidChangeView(const pp::Rect& position, const pp::Rect& clip) {
    if (position.size().width() == width_ &&
        position.size().height() == height_)
      return;  // We don't care about the position, only the size.

    width_ = position.size().width();
    height_ = position.size().height();

    device_context_ = pp::Graphics2D(this, pp::Size(width_, height_), false);
    if (!BindGraphics(device_context_))
      return;

    Paint();
  }

  virtual void MouseLockLost() {
    if (mouse_locked_) {
      mouse_locked_ = false;
      Paint();
    } else {
      PP_NOTREACHED();
    }
  }

 private:
  void DidLockMouse(int32_t result) {
    mouse_locked_ = result == PP_OK;
    mouse_movement_.set_x(0);
    mouse_movement_.set_y(0);
    Paint();
  }

  void DidFlush(int32_t result) {
    waiting_for_flush_completion_ = false;
    if (pending_paint_) {
      pending_paint_ = false;
      Paint();
    }
  }

  void Paint() {
    if (waiting_for_flush_completion_) {
      pending_paint_ = true;
      return;
    }

    pp::ImageData image = PaintImage(width_, height_);
    if (!image.is_null()) {
      device_context_.ReplaceContents(&image);
      waiting_for_flush_completion_ = true;
      device_context_.Flush(
          callback_factory_.NewCallback(&MyInstance::DidFlush));
    }
  }

  pp::ImageData PaintImage(int width, int height) {
    pp::ImageData image(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                        pp::Size(width, height), false);
    if (image.is_null())
      return image;

    const static int kCenteralSpotRadius = 5;
    const static uint32_t kBackgroundColor = 0xfff0f0f0;
    const static uint32_t kLockedForegroundColor = 0xfff08080;
    const static uint32_t kUnlockedForegroundColor = 0xff80f080;

    int center_x = width / 2;
    int center_y = height / 2;
    pp::Point vertex(mouse_movement_.x() + center_x,
                     mouse_movement_.y() + center_y);
    pp::Point anchor_1;
    pp::Point anchor_2;
    enum {
      LEFT = 0,
      RIGHT = 1,
      UP = 2,
      DOWN = 3
    } direction = LEFT;
    bool draw_needle = GetDistance(mouse_movement_.x(), mouse_movement_.y(),
                                   0, 0) > kCenteralSpotRadius;
    if (draw_needle) {
      if (abs(mouse_movement_.x()) >= abs(mouse_movement_.y())) {
         anchor_1.set_x(center_x);
         anchor_1.set_y(center_y - kCenteralSpotRadius);
         anchor_2.set_x(center_x);
         anchor_2.set_y(center_y + kCenteralSpotRadius);
         direction = (mouse_movement_.x() < 0) ? LEFT : RIGHT;
         if (direction == LEFT)
           anchor_1.swap(anchor_2);
      } else {
         anchor_1.set_x(center_x + kCenteralSpotRadius);
         anchor_1.set_y(center_y);
         anchor_2.set_x(center_x - kCenteralSpotRadius);
         anchor_2.set_y(center_y);
         direction = (mouse_movement_.y() < 0) ? UP : DOWN;
         if (direction == UP)
           anchor_1.swap(anchor_2);
      }
    }
    uint32_t foreground_color = mouse_locked_ ? kLockedForegroundColor :
                                                kUnlockedForegroundColor;
    for (int y = 0; y < image.size().height(); ++y) {
      for (int x = 0; x < image.size().width(); ++x) {
        if (GetDistance(x, y, center_x, center_y) < kCenteralSpotRadius) {
          *image.GetAddr32(pp::Point(x, y)) = foreground_color;
          continue;
        }
        if (draw_needle) {
          bool within_bound_1 =
              ((y - anchor_1.y()) * (vertex.x() - anchor_1.x())) >
              ((vertex.y() - anchor_1.y()) * (x - anchor_1.x()));
          bool within_bound_2 =
              ((y - anchor_2.y()) * (vertex.x() - anchor_2.x())) <
              ((vertex.y() - anchor_2.y()) * (x - anchor_2.x()));
          bool within_bound_3 =
              (direction == UP && y < center_y) ||
              (direction == DOWN && y > center_y) ||
              (direction == LEFT && x < center_x) ||
              (direction == RIGHT && x > center_x);

          if (within_bound_1 && within_bound_2 && within_bound_3) {
            *image.GetAddr32(pp::Point(x, y)) = foreground_color;
            continue;
          }
        }
        *image.GetAddr32(pp::Point(x, y)) = kBackgroundColor;
      }
    }

    return image;
  }

  double GetDistance(int point_1_x, int point_1_y,
                     int point_2_x, int point_2_y) {
    return sqrt(pow(static_cast<double>(point_1_x - point_2_x), 2) +
                pow(static_cast<double>(point_1_y - point_2_y), 2));
  }

  void Log(PP_LogLevel level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buf[512];
    vsnprintf(buf, sizeof(buf) - 1, format, args);
    buf[sizeof(buf) - 1] = '\0';
    va_end(args);

    pp::Var value(buf);
    console_->Log(pp_instance(), level, value.pp_var());
  }

  int width_;
  int height_;

  bool mouse_locked_;
  pp::Point mouse_movement_;

  bool pending_paint_;
  bool waiting_for_flush_completion_;

  pp::CompletionCallbackFactory<MyInstance> callback_factory_;

  const PPB_Console* console_;

  pp::Fullscreen fullscreen_;

  pp::Graphics2D device_context_;
};

// This object is the global object representing this plugin library as long
// as it is loaded.
class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() {}
  virtual ~MyModule() {}

  // Override CreateInstance to create your customized Instance object.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp

