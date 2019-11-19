// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/picture_matchers.h"

#include <utility>

#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace blink {

namespace {

class DrawsRectangleCanvas : public SkCanvas {
 public:
  DrawsRectangleCanvas()
      : SkCanvas(800, 600),
        save_count_(0),
        alpha_(255),
        alpha_save_layer_count_(-1) {}
  const Vector<RectWithColor>& RectsWithColor() const { return rects_; }

  void onDrawRect(const SkRect& rect, const SkPaint& paint) override {
    SkPoint quad[4];
    getTotalMatrix().mapRectToQuad(quad, rect);

    SkRect device_rect;
    device_rect.setBounds(quad, 4);
    SkIRect device_clip_bounds;
    FloatRect clipped_rect;
    if (getDeviceClipBounds(&device_clip_bounds) &&
        device_rect.intersect(SkRect::Make(device_clip_bounds)))
      clipped_rect = device_rect;

    unsigned paint_alpha = static_cast<unsigned>(paint.getAlpha());
    SkPaint paint_with_alpha(paint);
    paint_with_alpha.setAlpha(static_cast<U8CPU>(alpha_ * paint_alpha / 255));
    Color color = Color(paint_with_alpha.getColor());

    rects_.emplace_back(clipped_rect, color);
    SkCanvas::onDrawRect(rect, paint);
  }

  SkCanvas::SaveLayerStrategy getSaveLayerStrategy(
      const SaveLayerRec& rec) override {
    save_count_++;
    unsigned layer_alpha = static_cast<unsigned>(rec.fPaint->getAlpha());
    if (layer_alpha < 255) {
      DCHECK_EQ(alpha_save_layer_count_, -1);
      alpha_save_layer_count_ = save_count_;
      alpha_ = layer_alpha;
    }
    return SkCanvas::getSaveLayerStrategy(rec);
  }

  void willSave() override {
    save_count_++;
    SkCanvas::willSave();
  }

  void willRestore() override {
    DCHECK_GT(save_count_, 0);
    if (alpha_save_layer_count_ == save_count_) {
      alpha_ = 255;
      alpha_save_layer_count_ = -1;
    }
    save_count_--;
    SkCanvas::willRestore();
  }

 private:
  Vector<RectWithColor> rects_;
  int save_count_;
  unsigned alpha_;
  int alpha_save_layer_count_;
};

class DrawsRectanglesMatcher
    : public testing::MatcherInterface<const SkPicture&> {
 public:
  DrawsRectanglesMatcher(const Vector<RectWithColor>& rects_with_color)
      : rects_with_color_(rects_with_color) {}

  bool MatchAndExplain(const SkPicture& picture,
                       testing::MatchResultListener* listener) const override {
    DrawsRectangleCanvas canvas;
    picture.playback(&canvas);
    const auto& actual_rects = canvas.RectsWithColor();
    if (actual_rects.size() != rects_with_color_.size()) {
      *listener << "which draws " << actual_rects.size() << " rects";
      return false;
    }

    for (unsigned index = 0; index < actual_rects.size(); index++) {
      const auto& actual_rect_with_color = actual_rects[index];
      const auto& expect_rect_with_color = rects_with_color_[index];

      if (EnclosingIntRect(actual_rect_with_color.rect) !=
              EnclosingIntRect(expect_rect_with_color.rect) ||
          actual_rect_with_color.color != expect_rect_with_color.color) {
        if (listener->IsInterested()) {
          *listener << "at index " << index << " which draws "
                    << actual_rect_with_color.rect << " with color "
                    << actual_rect_with_color.color.Serialized() << "\n";
        }
        return false;
      }
    }

    return true;
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << "\n";
    for (unsigned index = 0; index < rects_with_color_.size(); index++) {
      const auto& rect_with_color = rects_with_color_[index];
      *os << "at index " << index << " rect draws " << rect_with_color.rect
          << " with color " << rect_with_color.color.Serialized() << "\n";
    }
  }

 private:
  const Vector<RectWithColor> rects_with_color_;
};

}  // namespace

testing::Matcher<const SkPicture&> DrawsRectangle(const FloatRect& rect,
                                                  Color color) {
  Vector<RectWithColor> rects_with_color;
  rects_with_color.push_back(RectWithColor(rect, color));
  return testing::MakeMatcher(new DrawsRectanglesMatcher(rects_with_color));
}

testing::Matcher<const SkPicture&> DrawsRectangles(
    const Vector<RectWithColor>& rects_with_color) {
  return testing::MakeMatcher(new DrawsRectanglesMatcher(rects_with_color));
}

}  // namespace blink
