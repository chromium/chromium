// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <cassert>

#include "ppapi/c/ppb_gamepad.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/var.h"
#include "ppapi/utility/completion_callback_factory.h"

#ifdef WIN32
#undef min
#undef max

// Allow 'this' in initializer list
#pragma warning(disable : 4355)
#endif

class GamepadInstance : public pp::Instance {
 public:
  explicit GamepadInstance(PP_Instance instance);
  virtual ~GamepadInstance();

  // Update the graphics context to the new size, and regenerate |pixel_buffer_|
  // to fit the new size as well.
  virtual void DidChangeView(const pp::View& view);

  // Flushes its contents of |pixel_buffer_| to the 2D graphics context.
  void Paint();

  int width() const {
    return pixel_buffer_ ? pixel_buffer_->size().width() : 0;
  }
  int height() const {
    return pixel_buffer_ ? pixel_buffer_->size().height() : 0;
  }

  // Indicate whether a flush is pending.  This can only be called from the
  // main thread; it is not thread safe.
  bool flush_pending() const { return flush_pending_; }
  void set_flush_pending(bool flag) { flush_pending_ = flag; }

 private:
  // Create and initialize the 2D context used for drawing.
  void CreateContext(const pp::Size& size);
  // Destroy the 2D drawing context.
  void DestroyContext();
  // Push the pixels to the browser, then attempt to flush the 2D context.  If
  // there is a pending flush on the 2D context, then update the pixels only
  // and do not flush.
  void FlushPixelBuffer();

  void FlushCallback(int32_t result);

  bool IsContextValid() const { return graphics_2d_context_ != NULL; }

  pp::CompletionCallbackFactory<GamepadInstance> callback_factory_;
  pp::Graphics2D* graphics_2d_context_;
  pp::ImageData* pixel_buffer_;
  const PPB_Gamepad* gamepad_;
  bool flush_pending_;
};

GamepadInstance::GamepadInstance(PP_Instance instance)
    : pp::Instance(instance),
      callback_factory_(this),
      graphics_2d_context_(NULL),
      pixel_buffer_(NULL),
      flush_pending_(false) {
  pp::Module* module = pp::Module::Get();
  assert(module);
  gamepad_ = static_cast<const PPB_Gamepad*>(
      module->GetBrowserInterface(PPB_GAMEPAD_INTERFACE));
  assert(gamepad_);
}

GamepadInstance::~GamepadInstance() {
  DestroyContext();
  delete pixel_buffer_;
}

void GamepadInstance::DidChangeView(const pp::View& view) {
  pp::Rect position = view.GetRect();
  if (position.size().width() == width() &&
      position.size().height() == height())
    return;  // Size didn't change, no need to update anything.

  // Create a new device context with the new size.
  DestroyContext();
  CreateContext(position.size());
  // Delete the old pixel buffer and create a new one.
  delete pixel_buffer_;
  pixel_buffer_ = NULL;
  if (graphics_2d_context_ != NULL) {
    pixel_buffer_ = new pp::ImageData(this,
                                      PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                                      graphics_2d_context_->size(),
                                      false);
  }
  Paint();
}

void FillRect(pp::ImageData* image,
              int left,
              int top,
              int width,
              int height,
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

void GamepadInstance::Paint() {
  // Clear the background.
  FillRect(pixel_buffer_, 0, 0, width(), height(), 0xfff0f0f0);

  // Get current gamepad data.
  PP_GamepadsSampleData gamepad_data;
  gamepad_->Sample(pp_instance(), &gamepad_data);

  // Draw the current state for each connected gamepad.
  for (size_t p = 0; p < gamepad_data.length; ++p) {
    int width2 = width() / gamepad_data.length / 2;
    int height2 = height() / 2;
    int offset = width2 * 2 * p;
    PP_GamepadSampleData& pad = gamepad_data.items[p];

    if (!pad.connected)
      continue;

    // Draw axes.
    for (size_t i = 0; i < pad.axes_length; i += 2) {
      int x = static_cast<int>(pad.axes[i + 0] * width2 + width2) + offset;
      int y = static_cast<int>(pad.axes[i + 1] * height2 + height2);
      uint32_t box_bgra = 0x80000000;  // Alpha 50%.
      FillRect(pixel_buffer_, x - 3, y - 3, 7, 7, box_bgra);
    }

    // Draw buttons.
    for (size_t i = 0; i < pad.buttons_length; ++i) {
      float button_val = pad.buttons[i];
      uint32_t colour = static_cast<uint32_t>((button_val * 192) + 63) << 24;
      int x = i * 8 + 10 + offset;
      int y = 10;
      FillRect(pixel_buffer_, x - 3, y - 3, 7, 7, colour);
    }
  }

  // Output to the screen.
  FlushPixelBuffer();
}

void GamepadInstance::CreateContext(const pp::Size& size) {
  if (IsContextValid())
    return;
  graphics_2d_context_ = new pp::Graphics2D(this, size, false);
  if (!BindGraphics(*graphics_2d_context_)) {
    printf("Couldn't bind the device context\n");
  }
}

void GamepadInstance::DestroyContext() {
  if (!IsContextValid())
    return;
  delete graphics_2d_context_;
  graphics_2d_context_ = NULL;
}

void GamepadInstance::FlushPixelBuffer() {
  if (!IsContextValid())
    return;
  // Note that the pixel lock is held while the buffer is copied into the
  // device context and then flushed.
  graphics_2d_context_->PaintImageData(*pixel_buffer_, pp::Point());
  if (flush_pending())
    return;
  set_flush_pending(true);
  graphics_2d_context_->Flush(
      callback_factory_.NewCallback(&GamepadInstance::FlushCallback));
}

void GamepadInstance::FlushCallback(int32_t result) {
  set_flush_pending(false);
  Paint();
}

class GamepadModule : public pp::Module {
 public:
  GamepadModule() : pp::Module() {}
  virtual ~GamepadModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new GamepadInstance(instance);
  }
};

namespace pp {
Module* CreateModule() { return new GamepadModule(); }
}  // namespace pp
