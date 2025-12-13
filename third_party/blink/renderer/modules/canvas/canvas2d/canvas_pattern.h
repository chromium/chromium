/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_PATTERN_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_PATTERN_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/canvas_high_entropy_op_type.h"
#include "third_party/blink/renderer/platform/graphics/pattern.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DOMMatrix2DInit;
class ExceptionState;
class Image;

class MODULES_EXPORT CanvasPattern final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Pattern::RepeatMode ParseRepetitionType(const String&,
                                                 ExceptionState&);

  CanvasPattern(scoped_refptr<Image>,
                Pattern::RepeatMode,
                bool origin_clean,
                HighEntropyCanvasOpType high_entropy_canvas_op_types);

  Pattern* GetPattern() const { return pattern_.get(); }
  const AffineTransform& GetTransform() const { return pattern_transform_; }

  bool OriginClean() const { return origin_clean_; }

  HighEntropyCanvasOpType HighEntropyCanvasOpTypes() const {
    return high_entropy_canvas_op_types_;
  }
  bool HasHighEntropyCanvasOpTypes() const {
    return high_entropy_canvas_op_types_ != HighEntropyCanvasOpType::kNone;
  }

  void setTransform(DOMMatrix2DInit*, ExceptionState&);

 private:
  std::unique_ptr<Pattern> pattern_;
  AffineTransform pattern_transform_;
  const bool origin_clean_;
  const HighEntropyCanvasOpType high_entropy_canvas_op_types_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_PATTERN_H_
