// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "ppapi/c/pp_input_event.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/view.h"
#include "ppapi/utility/graphics/paint_manager.h"

// Number of pixels to each side of the center of the square that we draw.
static const int kSquareRadius = 2;

// We identify our square by the center point. This computes the rect for the
// square given that point.
pp::Rect SquareForPoint(int x, int y) {
  return PP_MakeRectFromXYWH(x - kSquareRadius, y - kSquareRadius,
                             kSquareRadius * 2 + 1, kSquareRadius * 2 + 1);
}

static void FillRect(pp::ImageData* image,
                     int left, int top, int width, int height,
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

class MyInstance : public pp::Instance, public pp::PaintManager::Client {
 public:
  MyInstance(PP_Instance instance)
      : pp::Instance(instance),
        paint_manager_(),
        last_x_(0),
        last_y_(0) {
    paint_manager_.Initialize(this, this, false);
    RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
  }

  virtual bool HandleInputEvent(const pp::InputEvent& event) {
    switch (event.GetType()) {
      case PP_INPUTEVENT_TYPE_MOUSEDOWN: {
        pp::MouseInputEvent mouse_event(event);
        // Update the square on a mouse down.
        if (mouse_event.GetButton() == PP_INPUTEVENT_MOUSEBUTTON_LEFT) {
          UpdateSquare(static_cast<int>(mouse_event.GetPosition().x()),
                       static_cast<int>(mouse_event.GetPosition().y()));
        }
        return true;
      }
      case PP_INPUTEVENT_TYPE_MOUSEMOVE: {
        pp::MouseInputEvent mouse_event(event);
        // Update the square on a drag.
        if (mouse_event.GetButton() == PP_INPUTEVENT_MOUSEBUTTON_LEFT) {
          UpdateSquare(static_cast<int>(mouse_event.GetPosition().x()),
                       static_cast<int>(mouse_event.GetPosition().y()));
        }
        return true;
      }
      default:
        return false;
    }
  }

  virtual void DidChangeView(const pp::View& view) {
    paint_manager_.SetSize(view.GetRect().size());
  }

  // PaintManager::Client implementation.
  virtual bool OnPaint(pp::Graphics2D& graphics_2d,
                       const std::vector<pp::Rect>& paint_rects,
                       const pp::Rect& paint_bounds) {
    // Make an image just large enough to hold all dirty rects. We won't
    // actually paint all of these pixels below, but rather just the dirty
    // ones. Since image allocation can be somewhat heavyweight, we wouldn't
    // want to allocate separate images in the case of multiple dirty rects.
    pp::ImageData updated_image(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                                paint_bounds.size(), false);

    // We could repaint everything inside the image we made above. For this
    // example, that would probably be the easiest thing since updates are
    // small and typically close to each other. However, for the purposes of
    // demonstration, here we only actually paint the pixels that changed,
    // which may be the entire update region, or could be multiple discontigous
    // regions inside the update region.
    //
    // Note that the aggregator used by the paint manager won't give us
    // multiple regions that overlap, so we don't have to worry about double
    // painting in this code.
    for (size_t i = 0; i < paint_rects.size(); i++) {
      // Since our image is just the invalid region, we need to offset the
      // areas we paint by that much. This is just a light blue background.
      FillRect(&updated_image,
               paint_rects[i].x() - paint_bounds.x(),
               paint_rects[i].y() - paint_bounds.y(),
               paint_rects[i].width(),
               paint_rects[i].height(),
               0xFFAAAAFF);
    }

    // Paint the square black. Because we're lazy, we do this outside of the
    // loop above.
    pp::Rect square = SquareForPoint(last_x_, last_y_);
    FillRect(&updated_image,
             square.x() - paint_bounds.x(),
             square.y() - paint_bounds.y(),
             square.width(),
             square.height(),
             0xFF000000);

    graphics_2d.PaintImageData(updated_image, paint_bounds.point());
    return true;
  }

 private:
  void UpdateSquare(int x, int y) {
    if (x == last_x_ && y == last_y_)
      return;  // Nothing changed.

    // Invalidate the region around the old square which needs to be repainted
    // because it's no longer there.
    paint_manager_.InvalidateRect(SquareForPoint(last_x_, last_y_));

    // Update the current position.
    last_x_ = x;
    last_y_ = y;

    // Also invalidate the region around the new square.
    paint_manager_.InvalidateRect(SquareForPoint(last_x_, last_y_));
  }

  pp::PaintManager paint_manager_;

  int last_x_;
  int last_y_;
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
