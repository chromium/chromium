/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SHAPE_CLIP_PATH_OPERATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SHAPE_CLIP_PATH_OPERATION_H_

#include "third_party/blink/renderer/core/style/basic_shapes.h"
#include "third_party/blink/renderer/core/style/clip_path_operation.h"
#include "third_party/blink/renderer/platform/graphics/path.h"

namespace blink {

class ShapeClipPathOperation final : public ClipPathOperation {
 public:
  static scoped_refptr<ShapeClipPathOperation> Create(
      scoped_refptr<BasicShape> shape) {
    return base::AdoptRef(new ShapeClipPathOperation(std::move(shape)));
  }

  const BasicShape* GetBasicShape() const { return shape_.get(); }
  bool IsValid() const { return shape_.get(); }
  Path GetPath(const FloatRect& bounding_rect) const {
    DCHECK(shape_);
    Path path;
    shape_->GetPath(path, bounding_rect);
    path.SetWindRule(shape_->GetWindRule());
    return path;
  }

 private:
  bool operator==(const ClipPathOperation&) const override;
  OperationType GetType() const override { return SHAPE; }

  ShapeClipPathOperation(scoped_refptr<BasicShape> shape)
      : shape_(std::move(shape)) {}

  scoped_refptr<BasicShape> shape_;
};

template <>
struct DowncastTraits<ShapeClipPathOperation> {
  static bool AllowFrom(const ClipPathOperation& op) {
    return op.GetType() == ClipPathOperation::SHAPE;
  }
};

inline bool ShapeClipPathOperation::operator==(
    const ClipPathOperation& o) const {
  if (!IsSameType(o))
    return false;
  BasicShape* other_shape = To<ShapeClipPathOperation>(o).shape_.get();
  if (!shape_.get() || !other_shape)
    return static_cast<bool>(shape_.get()) == static_cast<bool>(other_shape);
  return *shape_ == *other_shape;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SHAPE_CLIP_PATH_OPERATION_H_
