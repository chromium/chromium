// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdint.h>

#include <algorithm>

#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/var.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "ppapi/utility/graphics/paint_manager.h"

static const int kSquareSpacing = 98;
static const int kSquareSize = 5;

static const int kAdvanceXPerFrame = 0;
static const int kAdvanceYPerFrame = -3;

void FillRect(pp::ImageData* image, const pp::Rect& rect, uint32_t color) {
  for (int y = std::max(0, rect.y());
       y < std::min(image->size().height(), rect.bottom());
       y++) {
    for (int x = std::max(0, rect.x());
         x < std::min(image->size().width(), rect.right());
         x++)
      *image->GetAddr32(pp::Point(x, y)) = color;
  }
}

class MyInstance : public pp::Instance, public pp::PaintManager::Client {
 public:
  MyInstance(PP_Instance instance)
      : pp::Instance(instance),
        current_step_(0),
        kicked_off_(false) {
    factory_.Initialize(this);
    paint_manager_.Initialize(this, this, false);
  }

  virtual void DidChangeView(const pp::Rect& position, const pp::Rect& clip) {
    paint_manager_.SetSize(position.size());
  }

  void OnTimer(int32_t) {
    pp::Module::Get()->core()->CallOnMainThread(
        16, factory_.NewCallback(&MyInstance::OnTimer), 0);
    // The scroll and the invalidate will do the same thing in this example,
    // but the invalidate will cause a large repaint, whereas the scroll will
    // be faster and cause a smaller repaint.
#if 1
    paint_manager_.ScrollRect(pp::Rect(paint_manager_.graphics().size()),
                              pp::Point(kAdvanceXPerFrame, kAdvanceYPerFrame));
#else
    paint_manager_.Invalidate();
#endif
    current_step_++;
  }

 private:
  // PaintManager::Client implementation.
  virtual bool OnPaint(pp::Graphics2D& graphics,
                       const std::vector<pp::Rect>& paint_rects,
                       const pp::Rect& paint_bounds) {
    if (!kicked_off_) {
      pp::Module::Get()->core()->CallOnMainThread(
          16, factory_.NewCallback(&MyInstance::OnTimer), 0);
      kicked_off_ = true;
    }

    // Paint the background.
    pp::ImageData updated_image(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                                paint_bounds.size(), false);
    FillRect(&updated_image, pp::Rect(updated_image.size()), 0xFF8888FF);

    int x_origin = current_step_ * kAdvanceXPerFrame;
    int y_origin = current_step_ * kAdvanceYPerFrame;

    int x_offset = x_origin % kSquareSpacing;
    int y_offset = y_origin % kSquareSpacing;

    for (int ys = 0; ys < graphics.size().height() / kSquareSpacing + 2; ys++) {
      for (int xs = 0; xs < graphics.size().width() / kSquareSpacing + 2;
           xs++) {
        int x = xs * kSquareSpacing + x_offset - paint_bounds.x();
        int y = ys * kSquareSpacing + y_offset - paint_bounds.y();
        FillRect(&updated_image, pp::Rect(x, y, kSquareSize, kSquareSize),
                 0xFF000000);
      }
    }
    graphics.PaintImageData(updated_image, paint_bounds.point());
    return true;
  }

  pp::CompletionCallbackFactory<MyInstance> factory_;

  pp::PaintManager paint_manager_;

  int current_step_;

  bool kicked_off_;
};

class MyModule : public pp::Module {
 public:
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
