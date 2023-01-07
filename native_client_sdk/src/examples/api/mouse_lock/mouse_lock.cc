// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <cmath>
#include <cstdlib>

#include <algorithm>

#include "mouse_lock.h"

#ifdef WIN32
#undef min
#undef max
#undef PostMessage
#endif

// Indicate the direction of the mouse location relative to the center of the
// view.  These values are used to determine which 2D quadrant the needle lies
// in.
typedef enum {
  kLeft = 0,
  kRight = 1,
  kUp = 2,
  kDown = 3
} MouseDirection;

namespace {
const int kCentralSpotRadius = 5;
const uint32_t kReturnKeyCode = 13;
const uint32_t kBackgroundColor = 0xff606060;
const uint32_t kForegroundColor = 0xfff08080;
}  // namespace

MouseLockInstance::~MouseLockInstance() {
  free(background_scanline_);
  background_scanline_ = NULL;
}

bool MouseLockInstance::Init(uint32_t argc,
                             const char* argn[],
                             const char* argv[]) {
  RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE | PP_INPUTEVENT_CLASS_KEYBOARD);
  return true;
}

bool MouseLockInstance::HandleInputEvent(const pp::InputEvent& event) {
  switch (event.GetType()) {
    case PP_INPUTEVENT_TYPE_MOUSEDOWN: {
      if (mouse_locked_) {
        UnlockMouse();
      } else {
        LockMouse(
            callback_factory_.NewCallback(&MouseLockInstance::DidLockMouse));
      }
      return true;
    }

    case PP_INPUTEVENT_TYPE_MOUSEMOVE: {
      pp::MouseInputEvent mouse_event(event);
      mouse_movement_ = mouse_event.GetMovement();
      Paint();
      return true;
    }

    case PP_INPUTEVENT_TYPE_KEYDOWN: {
      pp::KeyboardInputEvent key_event(event);

      // Switch in and out of fullscreen when 'Enter' is hit
      if (key_event.GetKeyCode() == kReturnKeyCode) {
        // Ignore switch if in transition
        if (!is_context_bound_)
          return true;

        if (fullscreen_.IsFullscreen()) {
          if (!fullscreen_.SetFullscreen(false)) {
            Log("Could not leave fullscreen mode\n");
          } else {
            is_context_bound_ = false;
          }
        } else {
          if (!fullscreen_.SetFullscreen(true)) {
            Log("Could not enter fullscreen mode\n");
          } else {
            is_context_bound_ = false;
          }
        }
      }
      return true;
    }

    case PP_INPUTEVENT_TYPE_MOUSEUP:
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
    case PP_INPUTEVENT_TYPE_WHEEL:
    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYUP:
    case PP_INPUTEVENT_TYPE_CHAR:
    case PP_INPUTEVENT_TYPE_CONTEXTMENU:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_START:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_END:
    case PP_INPUTEVENT_TYPE_IME_TEXT:
    case PP_INPUTEVENT_TYPE_UNDEFINED:
    case PP_INPUTEVENT_TYPE_TOUCHSTART:
    case PP_INPUTEVENT_TYPE_TOUCHMOVE:
    case PP_INPUTEVENT_TYPE_TOUCHEND:
    case PP_INPUTEVENT_TYPE_TOUCHCANCEL:
    default:
      return false;
  }
}

void MouseLockInstance::DidChangeView(const pp::View& view) {
  // DidChangeView can get called for many reasons, so we only want to
  // rebuild the device context if we really need to.

  if ((size_ == view.GetRect().size()) &&
      (was_fullscreen_ == view.IsFullscreen()) && is_context_bound_) {
    Log("DidChangeView SKIP %d,%d FULL=%s CTX Bound=%s",
        view.GetRect().width(),
        view.GetRect().height(),
        view.IsFullscreen() ? "true" : "false",
        is_context_bound_ ? "true" : "false");
    return;
  }

  Log("DidChangeView DO %d,%d FULL=%s CTX Bound=%s",
      view.GetRect().width(),
      view.GetRect().height(),
      view.IsFullscreen() ? "true" : "false",
      is_context_bound_ ? "true" : "false");

  size_ = view.GetRect().size();
  device_context_ = pp::Graphics2D(this, size_, false);
  waiting_for_flush_completion_ = false;

  is_context_bound_ = BindGraphics(device_context_);
  if (!is_context_bound_) {
    Log("Could not bind to 2D context\n.");
    return;
  } else {
    Log("Bound to 2D context size %d,%d.\n", size_.width(), size_.height());
  }

  // Create a scanline for fill.
  delete[] background_scanline_;
  background_scanline_ = new uint32_t[size_.width()];
  uint32_t* bg_pixel = background_scanline_;
  for (int x = 0; x < size_.width(); ++x) {
    *bg_pixel++ = kBackgroundColor;
  }

  // Remember if we are fullscreen or not
  was_fullscreen_ = view.IsFullscreen();

  // Paint this context
  Paint();
}

void MouseLockInstance::MouseLockLost() {
  if (mouse_locked_) {
    Log("Mouselock unlocked.\n");
    mouse_locked_ = false;
    Paint();
  }
}

void MouseLockInstance::DidLockMouse(int32_t result) {
  mouse_locked_ = result == PP_OK;
  if (result != PP_OK) {
    Log("Mouselock failed with failed with error number %d.\n", result);
  }
  mouse_movement_.set_x(0);
  mouse_movement_.set_y(0);
  Paint();
}

void MouseLockInstance::DidFlush(int32_t result) {
  if (result != 0)
    Log("Flushed failed with error number %d.\n", result);
  waiting_for_flush_completion_ = false;
}

void MouseLockInstance::Paint() {
  // If we are already waiting to paint...
  if (waiting_for_flush_completion_) {
    return;
  }

  pp::ImageData image = PaintImage(size_);
  if (image.is_null()) {
    Log("Could not create image data\n");
    return;
  }

  device_context_.ReplaceContents(&image);
  waiting_for_flush_completion_ = true;
  device_context_.Flush(
      callback_factory_.NewCallback(&MouseLockInstance::DidFlush));
}

pp::ImageData MouseLockInstance::PaintImage(const pp::Size& size) {
  pp::ImageData image(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL, size, false);
  if (image.is_null() || image.data() == NULL) {
    Log("Skipping image.\n");
    return image;
  }

  ClearToBackground(&image);

  DrawCenterSpot(&image, kForegroundColor);
  DrawNeedle(&image, kForegroundColor);
  return image;
}

void MouseLockInstance::ClearToBackground(pp::ImageData* image) {
  if (image == NULL) {
    Log("ClearToBackground with NULL image.");
    return;
  }
  if (background_scanline_ == NULL) {
    Log("ClearToBackground with no scanline.");
    return;
  }
  int image_height = image->size().height();
  int image_width = image->size().width();

  for (int y = 0; y < image_height; ++y) {
    uint32_t* scanline = image->GetAddr32(pp::Point(0, y));
    memcpy(scanline,
           background_scanline_,
           image_width * sizeof(*background_scanline_));
  }
}

void MouseLockInstance::DrawCenterSpot(pp::ImageData* image,
                                       uint32_t spot_color) {
  if (image == NULL) {
    Log("DrawCenterSpot with NULL image");
    return;
  }
  // Draw the center spot.  The ROI is bounded by the size of the spot, plus
  // one pixel.
  int center_x = image->size().width() / 2;
  int center_y = image->size().height() / 2;
  int region_of_interest_radius = kCentralSpotRadius + 1;

  pp::Point left_top(std::max(0, center_x - region_of_interest_radius),
                     std::max(0, center_y - region_of_interest_radius));
  pp::Point right_bottom(
      std::min(image->size().width(), center_x + region_of_interest_radius),
      std::min(image->size().height(), center_y + region_of_interest_radius));
  for (int y = left_top.y(); y < right_bottom.y(); ++y) {
    for (int x = left_top.x(); x < right_bottom.x(); ++x) {
      if (GetDistance(x, y, center_x, center_y) < kCentralSpotRadius) {
        *image->GetAddr32(pp::Point(x, y)) = spot_color;
      }
    }
  }
}

void MouseLockInstance::DrawNeedle(pp::ImageData* image,
                                   uint32_t needle_color) {
  if (image == NULL) {
    Log("DrawNeedle with NULL image");
    return;
  }
  if (GetDistance(mouse_movement_.x(), mouse_movement_.y(), 0, 0) <=
      kCentralSpotRadius) {
    return;
  }

  int abs_mouse_x = std::abs(mouse_movement_.x());
  int abs_mouse_y = std::abs(mouse_movement_.y());
  int center_x = image->size().width() / 2;
  int center_y = image->size().height() / 2;
  pp::Point vertex(mouse_movement_.x() + center_x,
                   mouse_movement_.y() + center_y);
  pp::Point anchor_1;
  pp::Point anchor_2;
  MouseDirection direction = kLeft;

  if (abs_mouse_x >= abs_mouse_y) {
    anchor_1.set_x(center_x);
    anchor_1.set_y(center_y - kCentralSpotRadius);
    anchor_2.set_x(center_x);
    anchor_2.set_y(center_y + kCentralSpotRadius);
    direction = (mouse_movement_.x() < 0) ? kLeft : kRight;
    if (direction == kLeft)
      anchor_1.swap(anchor_2);
  } else {
    anchor_1.set_x(center_x + kCentralSpotRadius);
    anchor_1.set_y(center_y);
    anchor_2.set_x(center_x - kCentralSpotRadius);
    anchor_2.set_y(center_y);
    direction = (mouse_movement_.y() < 0) ? kUp : kDown;
    if (direction == kUp)
      anchor_1.swap(anchor_2);
  }

  pp::Point left_top(std::max(0, center_x - abs_mouse_x),
                     std::max(0, center_y - abs_mouse_y));
  pp::Point right_bottom(
      std::min(image->size().width(), center_x + abs_mouse_x),
      std::min(image->size().height(), center_y + abs_mouse_y));
  for (int y = left_top.y(); y < right_bottom.y(); ++y) {
    for (int x = left_top.x(); x < right_bottom.x(); ++x) {
      bool within_bound_1 = ((y - anchor_1.y()) * (vertex.x() - anchor_1.x())) >
                            ((vertex.y() - anchor_1.y()) * (x - anchor_1.x()));
      bool within_bound_2 = ((y - anchor_2.y()) * (vertex.x() - anchor_2.x())) <
                            ((vertex.y() - anchor_2.y()) * (x - anchor_2.x()));
      bool within_bound_3 = (direction == kUp && y < center_y) ||
                            (direction == kDown && y > center_y) ||
                            (direction == kLeft && x < center_x) ||
                            (direction == kRight && x > center_x);

      if (within_bound_1 && within_bound_2 && within_bound_3) {
        *image->GetAddr32(pp::Point(x, y)) = needle_color;
      }
    }
  }
}

void MouseLockInstance::Log(const char* format, ...) {
  static PPB_Console* console =
      (PPB_Console*)pp::Module::Get()->GetBrowserInterface(
          PPB_CONSOLE_INTERFACE);

  if (NULL == console)
    return;
  va_list args;
  va_start(args, format);
  char buf[512];
  vsnprintf(buf, sizeof(buf) - 1, format, args);
  buf[sizeof(buf) - 1] = '\0';
  va_end(args);

  pp::Var value(buf);
  console->Log(pp_instance(), PP_LOGLEVEL_ERROR, value.pp_var());
}

// This object is the global object representing this plugin library as long
// as it is loaded.
class MouseLockModule : public pp::Module {
 public:
  MouseLockModule() : pp::Module() {}
  virtual ~MouseLockModule() {}

  // Override CreateInstance to create your customized Instance object.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MouseLockInstance(instance);
  }
};

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() { return new MouseLockModule(); }

}  // namespace pp
