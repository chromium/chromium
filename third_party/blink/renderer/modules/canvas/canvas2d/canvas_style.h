/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_STYLE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_STYLE_H_

#include "base/check_op.h"
#include "base/types/pass_key.h"
#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CanvasGradient;
class CanvasPattern;
class CanvasRenderingContext2DState;
class HTMLCanvasElement;

class CanvasStyle final : public GarbageCollected<CanvasStyle> {
 public:
  // Only CanvasRenderingContext2DState is allowed to mutate this.
  using PassKey = base::PassKey<CanvasRenderingContext2DState>;

  explicit CanvasStyle(RGBA32);
  explicit CanvasStyle(CanvasGradient*);
  explicit CanvasStyle(CanvasPattern*);

  // Marks this style as potentially being referenced by multiple
  // CanvasRenderingContext2DStates. If the style is shared, then it should not
  // be mutated.
  void MarkShared(PassKey key) { shared_ = true; }
  bool is_shared() const { return shared_; }

  String GetColorAsString() const {
    DCHECK_EQ(type_, kColorRGBA);
    return Color::FromRGBA32(rgba_).SerializeAsCanvasColor();
  }
  CanvasGradient* GetCanvasGradient() const { return gradient_.Get(); }
  CanvasPattern* GetCanvasPattern() const { return pattern_; }

  void ApplyToFlags(cc::PaintFlags&) const;
  RGBA32 PaintColor() const;

  bool IsEquivalentRGBA(RGBA32 rgba) const {
    return type_ == kColorRGBA && rgba_ == rgba;
  }

  bool IsEquivalentPattern(CanvasPattern* pattern) const {
    return type_ == kImagePattern && pattern_ == pattern;
  }

  bool IsEquivalentGradient(CanvasGradient* gradient) const {
    return type_ == kGradient && gradient_ == gradient;
  }

  void SetColor(PassKey key, RGBA32 color) {
    DCHECK(!shared_);
    type_ = kColorRGBA;
    rgba_ = color;
    gradient_ = nullptr;
    pattern_ = nullptr;
  }

  void SetPattern(PassKey key, CanvasPattern* pattern) {
    DCHECK(!shared_);
    type_ = kImagePattern;
    pattern_ = pattern;
    gradient_ = nullptr;
  }

  void SetGradient(PassKey key, CanvasGradient* gradient) {
    DCHECK(!shared_);
    type_ = kGradient;
    gradient_ = gradient;
    pattern_ = nullptr;
  }

  void Trace(Visitor*) const;

 private:
  enum Type { kColorRGBA, kGradient, kImagePattern };

  Type type_;

  bool shared_ = false;

  // TODO(https://1351544): The CanvasStyle should be Color, not an SkColor or
  // an SkColor4f.
  RGBA32 rgba_;

  Member<CanvasGradient> gradient_;
  Member<CanvasPattern> pattern_;
};

bool ParseColorOrCurrentColor(Color& parsed_color,
                              const String& color_string,
                              HTMLCanvasElement*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_STYLE_H_
