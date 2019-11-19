// Copyright (C) 2013 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GRAPHICS_CONTEXT_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GRAPHICS_CONTEXT_STATE_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/platform/graphics/draw_looper_builder.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/stroke_data.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

// Encapsulates the state information we store for each pushed graphics state.
// Only GraphicsContext can use this class.
class PLATFORM_EXPORT GraphicsContextState final {
  USING_FAST_MALLOC(GraphicsContextState);

 public:
  static std::unique_ptr<GraphicsContextState> CreateAndCopy(
      const GraphicsContextState& other) {
    return base::WrapUnique(new GraphicsContextState(other));
  }

  GraphicsContextState();

  void Copy(const GraphicsContextState&);

  // PaintFlags objects that reflect the current state. If the length of the
  // path to be stroked is known, pass it in for correct dash or dot placement.
  const PaintFlags& StrokeFlags(const int stroked_path_length = 0,
                                const int dash_thickness = 0) const;
  const PaintFlags& FillFlags() const { return fill_flags_; }

  uint16_t SaveCount() const { return save_count_; }
  void IncrementSaveCount() { ++save_count_; }
  void DecrementSaveCount() { --save_count_; }

  // Stroke data
  Color StrokeColor() const { return stroke_flags_.getColor(); }
  void SetStrokeColor(const Color&);

  const StrokeData& GetStrokeData() const { return stroke_data_; }
  void SetStrokeStyle(StrokeStyle);
  void SetStrokeThickness(float);
  void SetLineCap(LineCap);
  void SetLineJoin(LineJoin);
  void SetMiterLimit(float);
  void SetLineDash(const DashArray&, float);

  // Fill data
  Color FillColor() const { return fill_flags_.getColor(); }
  void SetFillColor(const Color&);

  // Shadow. (This will need tweaking if we use draw loopers for other things.)
  SkDrawLooper* DrawLooper() const {
    DCHECK_EQ(fill_flags_.getLooper(), stroke_flags_.getLooper());
    return fill_flags_.getLooper().get();
  }
  void SetDrawLooper(sk_sp<SkDrawLooper>);

  // Text. (See TextModeFill & friends.)
  TextDrawingModeFlags TextDrawingMode() const { return text_drawing_mode_; }
  void SetTextDrawingMode(TextDrawingModeFlags mode) {
    text_drawing_mode_ = mode;
  }

  SkColorFilter* GetColorFilter() const {
    DCHECK_EQ(fill_flags_.getColorFilter(), stroke_flags_.getColorFilter());
    return fill_flags_.getColorFilter().get();
  }
  void SetColorFilter(sk_sp<SkColorFilter>);

  // Image interpolation control.
  InterpolationQuality GetInterpolationQuality() const {
    return interpolation_quality_;
  }
  void SetInterpolationQuality(InterpolationQuality);

  bool ShouldAntialias() const { return should_antialias_; }
  void SetShouldAntialias(bool);

 private:
  explicit GraphicsContextState(const GraphicsContextState&);
  GraphicsContextState& operator=(const GraphicsContextState&) = delete;

  // This is mutable to enable dash path effect updates when the paint is
  // fetched for use.
  mutable PaintFlags stroke_flags_;
  PaintFlags fill_flags_;

  StrokeData stroke_data_;

  TextDrawingModeFlags text_drawing_mode_;

  InterpolationQuality interpolation_quality_;

  uint16_t save_count_;

  bool should_antialias_ : 1;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GRAPHICS_CONTEXT_STATE_H_
