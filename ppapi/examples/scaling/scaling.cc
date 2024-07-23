// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include <sstream>

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/var.h"
#include "ppapi/utility/completion_callback_factory.h"

// When compiling natively on Windows, PostMessage can be #define-d to
// something else.
#ifdef PostMessage
#undef PostMessage
#endif

// Example plugin to demonstrate usage of pp::View and pp::Graphics2D APIs for
// rendering 2D graphics at device resolution. See Paint() for more details.
class MyInstance : public pp::Instance {
 public:
  explicit MyInstance(PP_Instance instance)
      : pp::Instance(instance),
        width_(0),
        height_(0),
        pixel_width_(0),
        pixel_height_(0),
        device_scale_(1.0f),
        css_scale_(1.0f),
        using_device_pixels_(true) {
    RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE |
                       PP_INPUTEVENT_CLASS_KEYBOARD);
  }

  virtual void DidChangeView(const pp::View& view) {
    pp::Rect view_rect = view.GetRect();
    if (view_rect.width() == width_ &&
        view_rect.height() == height_ &&
        view.GetDeviceScale() == device_scale_ &&
        view.GetCSSScale() == css_scale_)
      return;  // We don't care about the position, only the size and scale.

    width_ = view_rect.width();
    height_ = view_rect.height();
    device_scale_ = view.GetDeviceScale();
    css_scale_ = view.GetCSSScale();

    pixel_width_ = static_cast<int>(width_ * device_scale_);
    pixel_height_ = static_cast<int>(height_ * device_scale_);

    SetupGraphics();
  }

  virtual bool HandleInputEvent(const pp::InputEvent& event) {
    switch (event.GetType()) {
      case PP_INPUTEVENT_TYPE_MOUSEDOWN:
        HandleMouseDown(event);
        return true;
      default:
        return false;
    }
  }

  virtual void HandleMessage(const pp::Var& message_data) {
    if (message_data.is_string()) {
      std::string str = message_data.AsString();
      if (str == "dip") {
        if (using_device_pixels_) {
          using_device_pixels_ = false;
          SetupGraphics();
        }
      } else if (str == "device") {
        if (!using_device_pixels_) {
          using_device_pixels_ = true;
          SetupGraphics();
        }
      } else if (str == "metrics") {
        std::stringstream stream;
        stream << "DIP (" << width_ << ", " << height_ << "), device pixels=("
               << pixel_width_ << ", " << pixel_height_ <<"), device_scale="
               << device_scale_ <<", css_scale=" << css_scale_;
        PostMessage(stream.str());
      }
    }
  }

 private:
  void HandleMouseDown(const pp::InputEvent& event) {
    pp::MouseInputEvent mouse_event(event);
    pp::Point position(mouse_event.GetPosition());
    pp::Point position_device(
        static_cast<int32_t>(position.x() * device_scale_),
        static_cast<int32_t>(position.y() * device_scale_));
    std::stringstream stream;
    stream << "Mousedown at DIP (" << position.x() << ", " << position.y()
           << "), device pixel (" << position_device.x() << ", "
           << position_device.y() << ")";
    if (css_scale_ > 0.0f) {
      pp::Point position_css(static_cast<int32_t>(position.x() / css_scale_),
                             static_cast<int32_t>(position.y() / css_scale_));
      stream << ", CSS pixel (" << position_css.x() << ", " << position_css.y()
             <<")";
    } else {
      stream <<", unknown CSS pixel. css_scale_=" << css_scale_;
    }
    PostMessage(stream.str());
  }

  void SetupGraphics() {
    if (using_device_pixels_) {
      // The plugin will treat 1 pixel in the device context as 1 device pixel.
      // This will set up a properly-sized pp::Graphics2D, and tell Pepper
      // to apply the correct scale so the resulting concatenated scale leaves
      // each pixel in the device context as one on the display device.
      device_context_ = pp::Graphics2D(this,
                                       pp::Size(pixel_width_, pixel_height_),
                                       true);
      if (device_scale_ > 0.0f) {
        // If SetScale is promoted to pp::Graphics2D, the dc_dev constructor
        // can be removed, and this will become the following line instead.
        // device_context_.SetScale(1.0f / device_scale_);
        device_context_.SetScale(1.0f / device_scale_);
      }
    } else {
      // The plugin will treat 1 pixel in the device context as one DIP.
      device_context_ = pp::Graphics2D(this, pp::Size(width_, height_), true);
    }
    BindGraphics(device_context_);
    Paint();
  }

  void Paint() {
    int width = using_device_pixels_ ? pixel_width_ : width_;
    int height = using_device_pixels_ ? pixel_height_ : height_;
    pp::ImageData image(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                        pp::Size(width, height), false);
    if (image.is_null())
      return;

    // Painting here will demonstrate a few techniques:
    // - painting a thin blue box and cross-hatch to show the finest resolution
    // available.
    // - painting a 25 DIP (logical pixel) green circle to show how objects of a
    //  fixed size in DIPs should be scaled if using a high-resolution
    // pp::Graphics2D.
    // - paiting a 50 CSS pixel red circle to show how objects of a fixed size
    // in  CSS pixels should be scaled if using a high-resolution
    // pp::Graphics2D, as well as how to use the GetCSSScale value properly.

    // Painting in "DIP resolution" mode (|using_device_pixels_| false) will
    // demonstrate how unscaled graphics would look, even on a high-DPI device.
    // Painting in "device resolution" mode (|using_device_pixels_| true) will
    // show how scaled graphics would look crisper on a high-DPI device, in
    // comparison to using unscaled graphics. Both modes should look identical
    // when displayed on a non-high-DPI device (window.devicePixelRatio == 1).
    // Toggling between "DIP resolution" mode and "device resolution" mode
    // should not change the sizes of the circles.

    // Changing the browser zoom level should cause the CSS circle to zoom, but
    // not the DIP-sized circle.

    // All painting here does not use any anti-aliasing.
    float circle_1_radius = 25;
    if (using_device_pixels_)
      circle_1_radius *= device_scale_;

    float circle_2_radius = 50 * css_scale_;
    if (using_device_pixels_)
      circle_2_radius *= device_scale_;

    for (int y = 0; y < height; ++y) {
      char* row = static_cast<char*>(image.data()) + (y * image.stride());
      uint32_t* pixel = reinterpret_cast<uint32_t*>(row);
      for (int x = 0; x < width; ++x) {
        int dx = (width / 2) - x;
        int dy = (height / 2) - y;
        float dist_squared = static_cast<float>((dx * dx) + (dy * dy));
        if (x == 0 || y == 0 || x == width - 1 || y == width - 1 || x == y ||
            width - x - 1 == y) {
          *pixel++ = 0xFF0000FF;
        } else if (dist_squared < circle_1_radius * circle_1_radius) {
          *pixel++ = 0xFF00FF00;
        } else if (dist_squared < circle_2_radius * circle_2_radius) {
          *pixel++ = 0xFFFF0000;
        } else {
          *pixel++ = 0xFF000000;
        }
      }
    }

    device_context_.ReplaceContents(&image);
    device_context_.Flush(pp::CompletionCallback(&OnFlush, this));
  }

  static void OnFlush(void* user_data, int32_t result) {}

  pp::Graphics2D device_context_;
  int width_;
  int height_;
  int pixel_width_;
  int pixel_height_;
  float device_scale_;
  float css_scale_;
  bool using_device_pixels_;
};

// This object is the global object representing this plugin library as long as
// it is loaded.
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
