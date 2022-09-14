// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>

#include "ppapi/c/ppb_image_data.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/point.h"
#include "ppapi/utility/completion_callback_factory.h"

#ifdef WIN32
#undef PostMessage
// Allow 'this' in initializer list
#pragma warning(disable : 4355)
#endif

namespace {

static const int kMouseRadius = 20;

uint8_t RandUint8(uint8_t min, uint8_t max) {
  uint64_t r = rand();
  uint8_t result = static_cast<uint8_t>(r * (max - min + 1) / RAND_MAX) + min;
  return result;
}

uint32_t MakeColor(uint8_t r, uint8_t g, uint8_t b) {
  uint8_t a = 255;
  PP_ImageDataFormat format = pp::ImageData::GetNativeImageDataFormat();
  if (format == PP_IMAGEDATAFORMAT_BGRA_PREMUL) {
    return (a << 24) | (r << 16) | (g << 8) | b;
  } else {
    return (a << 24) | (b << 16) | (g << 8) | r;
  }
}

}  // namespace

class Graphics2DInstance : public pp::Instance {
 public:
  explicit Graphics2DInstance(PP_Instance instance)
      : pp::Instance(instance),
        callback_factory_(this),
        mouse_down_(false),
        buffer_(NULL),
        device_scale_(1.0f) {}

  ~Graphics2DInstance() { delete[] buffer_; }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);

    unsigned int seed = 1;
    srand(seed);
    CreatePalette();
    return true;
  }

  virtual void DidChangeView(const pp::View& view) {
    device_scale_ = view.GetDeviceScale();
    pp::Size new_size = pp::Size(view.GetRect().width() * device_scale_,
                                 view.GetRect().height() * device_scale_);

    if (!CreateContext(new_size))
      return;

    // When flush_context_ is null, it means there is no Flush callback in
    // flight. This may have happened if the context was not created
    // successfully, or if this is the first call to DidChangeView (when the
    // module first starts). In either case, start the main loop.
    if (flush_context_.is_null())
      MainLoop(0);
  }

  virtual bool HandleInputEvent(const pp::InputEvent& event) {
    if (!buffer_)
      return true;

    if (event.GetType() == PP_INPUTEVENT_TYPE_MOUSEDOWN ||
        event.GetType() == PP_INPUTEVENT_TYPE_MOUSEMOVE) {
      pp::MouseInputEvent mouse_event(event);

      if (mouse_event.GetButton() == PP_INPUTEVENT_MOUSEBUTTON_NONE)
        return true;

      mouse_ = pp::Point(mouse_event.GetPosition().x() * device_scale_,
                         mouse_event.GetPosition().y() * device_scale_);
      mouse_down_ = true;
    }

    if (event.GetType() == PP_INPUTEVENT_TYPE_MOUSEUP)
      mouse_down_ = false;

    return true;
  }

 private:
  void CreatePalette() {
    for (int i = 0; i < 64; ++i) {
      // Black -> Red
      palette_[i] = MakeColor(i * 2, 0, 0);
      palette_[i + 64] = MakeColor(128 + i * 2, 0, 0);
      // Red -> Yellow
      palette_[i + 128] = MakeColor(255, i * 4, 0);
      // Yellow -> White
      palette_[i + 192] = MakeColor(255, 255, i * 4);
    }
  }

  bool CreateContext(const pp::Size& new_size) {
    const bool kIsAlwaysOpaque = true;
    context_ = pp::Graphics2D(this, new_size, kIsAlwaysOpaque);
    // Call SetScale before BindGraphics so the image is scaled correctly on
    // HiDPI displays.
    context_.SetScale(1.0f / device_scale_);
    if (!BindGraphics(context_)) {
      fprintf(stderr, "Unable to bind 2d context!\n");
      context_ = pp::Graphics2D();
      return false;
    }

    // Allocate a buffer of palette entries of the same size as the new context.
    buffer_ = new uint8_t[new_size.width() * new_size.height()];
    size_ = new_size;

    return true;
  }

  void Update() {
    // Old-school fire technique cribbed from
    // http://ionicsolutions.net/2011/12/30/demo-fire-effect/
    UpdateCoals();
    DrawMouse();
    UpdateFlames();
  }

  void UpdateCoals() {
    int width = size_.width();
    int height = size_.height();
    size_t span = 0;

    // Draw two rows of random values at the bottom.
    for (int y = height - 2; y < height; ++y) {
      size_t offset = y * width;
      for (int x = 0; x < width; ++x) {
        // On a random chance, draw some longer strips of brighter colors.
        if (span || RandUint8(1, 4) == 1) {
          if (!span)
            span = RandUint8(10, 20);
          buffer_[offset + x] = RandUint8(128, 255);
          span--;
        } else {
          buffer_[offset + x] = RandUint8(32, 96);
        }
      }
    }
  }

  void UpdateFlames() {
    int width = size_.width();
    int height = size_.height();
    for (int y = 1; y < height - 1; ++y) {
      size_t offset = y * width;
      for (int x = 1; x < width - 1; ++x) {
        int sum = 0;
        sum += buffer_[offset - width + x - 1];
        sum += buffer_[offset - width + x + 1];
        sum += buffer_[offset + x - 1];
        sum += buffer_[offset + x + 1];
        sum += buffer_[offset + width + x - 1];
        sum += buffer_[offset + width + x];
        sum += buffer_[offset + width + x + 1];
        buffer_[offset - width + x] = sum / 7;
      }
    }
  }

  void DrawMouse() {
    if (!mouse_down_)
      return;

    int width = size_.width();
    int height = size_.height();

    // Draw a circle at the mouse position.
    int radius = kMouseRadius * device_scale_;
    int cx = mouse_.x();
    int cy = mouse_.y();
    int minx = cx - radius <= 0 ? 1 : cx - radius;
    int maxx = cx + radius >= width ? width - 1 : cx + radius;
    int miny = cy - radius <= 0 ? 1 : cy - radius;
    int maxy = cy + radius >= height ? height - 1 : cy + radius;
    for (int y = miny; y < maxy; ++y) {
      for (int x = minx; x < maxx; ++x) {
        if ((x - cx) * (x - cx) + (y - cy) * (y - cy) < radius * radius)
          buffer_[y * width + x] = RandUint8(192, 255);
      }
    }
  }

  void Paint() {
    // See the comment above the call to ReplaceContents below.
    PP_ImageDataFormat format = pp::ImageData::GetNativeImageDataFormat();
    const bool kDontInitToZero = false;
    pp::ImageData image_data(this, format, size_, kDontInitToZero);

    uint32_t* data = static_cast<uint32_t*>(image_data.data());
    if (!data)
      return;

    uint32_t num_pixels = size_.width() * size_.height();
    size_t offset = 0;
    for (uint32_t i = 0; i < num_pixels; ++i) {
      data[offset] = palette_[buffer_[offset]];
      offset++;
    }

    // Using Graphics2D::ReplaceContents is the fastest way to update the
    // entire canvas every frame. According to the documentation:
    //
    //   Normally, calling PaintImageData() requires that the browser copy
    //   the pixels out of the image and into the graphics context's backing
    //   store. This function replaces the graphics context's backing store
    //   with the given image, avoiding the copy.
    //
    //   In the case of an animation, you will want to allocate a new image for
    //   the next frame. It is best if you wait until the flush callback has
    //   executed before allocating this bitmap. This gives the browser the
    //   option of caching the previous backing store and handing it back to
    //   you (assuming the sizes match). In the optimal case, this means no
    //   bitmaps are allocated during the animation, and the backing store and
    //   "front buffer" (which the module is painting into) are just being
    //   swapped back and forth.
    //
    context_.ReplaceContents(&image_data);
  }

  void MainLoop(int32_t) {
    if (context_.is_null()) {
      // The current Graphics2D context is null, so updating and rendering is
      // pointless. Set flush_context_ to null as well, so if we get another
      // DidChangeView call, the main loop is started again.
      flush_context_ = context_;
      return;
    }

    Update();
    Paint();
    // Store a reference to the context that is being flushed; this ensures
    // the callback is called, even if context_ changes before the flush
    // completes.
    flush_context_ = context_;
    context_.Flush(
        callback_factory_.NewCallback(&Graphics2DInstance::MainLoop));
  }

  pp::CompletionCallbackFactory<Graphics2DInstance> callback_factory_;
  pp::Graphics2D context_;
  pp::Graphics2D flush_context_;
  pp::Size size_;
  pp::Point mouse_;
  bool mouse_down_;
  uint8_t* buffer_;
  uint32_t palette_[256];
  float device_scale_;
};

class Graphics2DModule : public pp::Module {
 public:
  Graphics2DModule() : pp::Module() {}
  virtual ~Graphics2DModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new Graphics2DInstance(instance);
  }
};

namespace pp {
Module* CreateModule() { return new Graphics2DModule(); }
}  // namespace pp
