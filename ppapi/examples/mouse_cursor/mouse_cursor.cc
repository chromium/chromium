// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <algorithm>

#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/mouse_cursor.h"
#include "ppapi/cpp/view.h"

void FillRect(pp::ImageData* image, int left, int top, int width, int height,
              uint32_t color) {
  for (int y = std::max(0, top);
       y < std::min(image->size().height() - 1, top + height);
       y++) {
    for (int x = std::max(0, left);
         x < std::min(image->size().width() - 1, left + width);
         x++)
      *image->GetAddr32(pp::Point(x, y)) = color;
  }
}

class MyInstance : public pp::Instance {
 public:
  MyInstance(PP_Instance instance)
      : pp::Instance(instance), width_(0), height_(0) {
    RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
  }

  virtual ~MyInstance() {
  }

  virtual void DidChangeView(const pp::View& view) {
    width_ = view.GetRect().width();
    height_ = view.GetRect().height();
  }

  virtual bool HandleInputEvent(const pp::InputEvent& event) {
    switch (event.GetType()) {
      case PP_INPUTEVENT_TYPE_MOUSEDOWN:
        return true;
      case PP_INPUTEVENT_TYPE_MOUSEMOVE:
        HandleMove(pp::MouseInputEvent(event));
        return true;
      case PP_INPUTEVENT_TYPE_KEYDOWN:
        return true;
      default:
        return false;
    }
  }

  void HandleMove(const pp::MouseInputEvent& event) {
    pp::Point point = event.GetPosition();
    int segments = 3;
    if (point.y() < height_ / segments) {
      // Top part gets custom cursor of wait.
      pp::MouseCursor::SetCursor(this, PP_MOUSECURSOR_TYPE_WAIT);
    } else if (point.y() < (height_ / segments) * 2) {
      // Next part gets custom image cursor.
      pp::ImageData cursor(this, pp::ImageData::GetNativeImageDataFormat(),
                           pp::Size(32, 32), true);
      // Note that in real life you will need to handle the case where the
      // native format is different.
      FillRect(&cursor, 0, 0, 32, 32, 0x80000080);
      pp::MouseCursor::SetCursor(this, PP_MOUSECURSOR_TYPE_CUSTOM, cursor,
                                 pp::Point(16, 16));
    } else {
      // Next part gets no cursor.
      pp::MouseCursor::SetCursor(this, PP_MOUSECURSOR_TYPE_NONE);
    }
  }


 private:
  int width_;
  int height_;
};

class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() {}
  virtual ~MyModule() {}

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
