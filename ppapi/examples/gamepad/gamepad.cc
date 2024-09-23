// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <algorithm>
#include <cmath>

#include "ppapi/c/ppb_gamepad.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/view.h"
#include "ppapi/utility/completion_callback_factory.h"

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
  explicit MyInstance(PP_Instance instance)
      : pp::Instance(instance),
        width_(0),
        height_(0),
        callback_factory_(this),
        gamepad_(NULL) {
  }
  virtual ~MyInstance() {}

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    gamepad_ = reinterpret_cast<const PPB_Gamepad*>(
        pp::Module::Get()->GetBrowserInterface(PPB_GAMEPAD_INTERFACE));
    if (!gamepad_)
      return false;
    return true;
  }

  virtual void DidChangeView(const pp::View& view) {
    pp::Rect rect = view.GetRect();
    if (rect.size().width() == width_ &&
        rect.size().height() == height_)
      return;  // We don't care about the position, only the size.

    width_ = rect.size().width();
    height_ = rect.size().height();

    device_context_ = pp::Graphics2D(this, pp::Size(width_, height_), false);
    if (!BindGraphics(device_context_))
      return;

    Paint();
  }

  void OnFlush(int32_t) {
    // This plugin continuously paints because it continously samples the
    // gamepad and paints its updated state.
    Paint();
  }

 private:
  void Paint() {
    pp::ImageData image = PaintImage(device_context_.size());
    if (!image.is_null()) {
      device_context_.ReplaceContents(&image);
      device_context_.Flush(
          callback_factory_.NewCallback(&MyInstance::OnFlush));
    }
  }

  pp::ImageData PaintImage(const pp::Size& size) {
    pp::ImageData image(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL, size, true);
    if (image.is_null())
      return image;

    PP_GamepadsSampleData gamepad_data;
    gamepad_->Sample(pp_instance(), &gamepad_data);

    if (gamepad_data.length > 0 && gamepad_data.items[0].connected) {
      int width2 = size.width() / 2;
      int height2 = size.height() / 2;
      // Draw 2 axes
      for (size_t i = 0; i < gamepad_data.items[0].axes_length; i += 2) {
        int x = static_cast<int>(
            gamepad_data.items[0].axes[i + 0] * width2 + width2);
        int y = static_cast<int>(
            gamepad_data.items[0].axes[i + 1] * height2 + height2);
        uint32_t box_bgra = 0x80000000;  // Alpha 50%.
        FillRect(&image, x - 3, y - 3, 7, 7, box_bgra);
      }

      for (size_t i = 0; i < gamepad_data.items[0].buttons_length; ++i) {
        float button_val = gamepad_data.items[0].buttons[i];
        uint32_t colour = static_cast<uint32_t>((button_val * 192) + 63) << 24;
        int x = static_cast<int>(i) * 8 + 10;
        int y = 10;
        FillRect(&image, x - 3, y - 3, 7, 7, colour);
      }
    }
    return image;
  }

  int width_;
  int height_;

  pp::CompletionCallbackFactory<MyInstance> callback_factory_;

  const PPB_Gamepad* gamepad_;

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
