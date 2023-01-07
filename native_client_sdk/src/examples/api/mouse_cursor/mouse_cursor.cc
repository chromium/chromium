// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdio.h>

#include "ppapi/c/ppb_mouse_cursor.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/mouse_cursor.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/var.h"
#include "ppapi/utility/completion_callback_factory.h"

#ifdef WIN32
#undef PostMessage
// Allow 'this' in initializer list
#pragma warning(disable : 4355)
#endif

namespace {

uint32_t MakeColor(float r, float g, float b, float a) {
  // Since we're using premultiplied alpha
  // (PP_IMAGEDATAFORMAT_BGRA_PREMUL), we have to multiply each
  // color component by the alpha value.
  uint8_t a8 = static_cast<uint8_t>(255 * a);
  uint8_t r8 = static_cast<uint8_t>(255 * r * a);
  uint8_t g8 = static_cast<uint8_t>(255 * g * a);
  uint8_t b8 = static_cast<uint8_t>(255 * b * a);
  return (a8 << 24) | (r8 << 16) | (g8 << 8) | b8;
}

}

class MouseCursorInstance : public pp::Instance {
 public:
  explicit MouseCursorInstance(PP_Instance instance)
      : pp::Instance(instance) {}

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    MakeCustomCursor();
    return true;
  }

 private:
  virtual void HandleMessage(const pp::Var& var_message) {
    if (!var_message.is_number()) {
      fprintf(stderr, "Unexpected message.\n");
      return;
    }

    PP_MouseCursor_Type cursor =
        static_cast<PP_MouseCursor_Type>(var_message.AsInt());
    if (cursor == PP_MOUSECURSOR_TYPE_CUSTOM) {
      pp::Point hot_spot(16, 16);
      pp::MouseCursor::SetCursor(this, cursor, custom_cursor_, hot_spot);
    } else {
      pp::MouseCursor::SetCursor(this, cursor);
    }
  }

  void MakeCustomCursor() {
    pp::Size size(32, 32);
    custom_cursor_ =
        pp::ImageData(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL, size, true);
    DrawCircle(16, 16, 9, 14, 0.8f, 0.8f, 0);
    DrawCircle(11, 12, 2, 3, 0, 0, 0);
    DrawCircle(21, 12, 2, 3, 0, 0, 0);
    DrawHorizontalLine(12, 20, 21, 0.5f, 0, 0, 1.0f);
  }

  void DrawCircle(int cx, int cy, float alpha_radius, float radius,
                  float r, float g, float b) {
    pp::Size size = custom_cursor_.size();
    uint32_t* data = static_cast<uint32_t*>(custom_cursor_.data());
    // It's less efficient to loop over the entire image this way, but the
    // image is small, and this is simpler.
    for (int y = 0; y < size.width(); ++y) {
      for (int x = 0; x < size.width(); ++x) {
        int dx = (x - cx);
        int dy = (y - cy);
        float dist = sqrtf(dx * dx + dy * dy);

        if (dist < radius) {
          float a;
          if (dist > alpha_radius) {
            a = 1.f - (dist - alpha_radius) / (radius - alpha_radius);
          } else {
            a = 1.f;
          }

          data[y * size.width() + x] = MakeColor(r, g, b, a);
        }
      }
    }
  }

  void DrawHorizontalLine(int x1, int x2, int y,
                          float r, float g, float b, float a) {
    pp::Size size = custom_cursor_.size();
    uint32_t* data = static_cast<uint32_t*>(custom_cursor_.data());
    for (int x = x1; x <= x2; ++x) {
      data[y * size.width() + x] = MakeColor(r, g, b, a);
    }
  }

  pp::ImageData custom_cursor_;
};

class MouseCursorModule : public pp::Module {
 public:
  MouseCursorModule() : pp::Module() {}
  virtual ~MouseCursorModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MouseCursorInstance(instance);
  }
};

namespace pp {
Module* CreateModule() { return new MouseCursorModule(); }
}  // namespace pp
