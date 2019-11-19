/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_TRANSFORM_OPERATION_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_TRANSFORM_OPERATION_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

// CSS Transforms (may become part of CSS3)

class PLATFORM_EXPORT TransformOperation
    : public RefCounted<TransformOperation> {
 public:
  enum OperationType {
    kScaleX,
    kScaleY,
    kScale,
    kTranslateX,
    kTranslateY,
    kTranslate,
    kRotate,
    kRotateZ = kRotate,
    kSkewX,
    kSkewY,
    kSkew,
    kMatrix,
    kScaleZ,
    kScale3D,
    kTranslateZ,
    kTranslate3D,
    kRotateX,
    kRotateY,
    kRotate3D,
    kMatrix3D,
    kPerspective,
    kInterpolated,
    kIdentity,
    kRotateAroundOrigin,
  };

  TransformOperation() = default;
  virtual ~TransformOperation() = default;

  virtual bool operator==(const TransformOperation&) const = 0;
  bool operator!=(const TransformOperation& o) const { return !(*this == o); }

  virtual void Apply(TransformationMatrix&,
                     const FloatSize& border_box_size) const = 0;

  // Implements the accumulative behavior described in
  // https://drafts.csswg.org/css-transforms-2/#combining-transform-lists
  virtual scoped_refptr<TransformOperation> Accumulate(
      const TransformOperation& other) = 0;

  virtual scoped_refptr<TransformOperation> Blend(
      const TransformOperation* from,
      double progress,
      bool blend_to_identity = false) = 0;
  virtual scoped_refptr<TransformOperation> Zoom(double factor) = 0;

  virtual OperationType GetType() const = 0;

  // https://drafts.csswg.org/css-transforms/#transform-primitives
  virtual OperationType PrimitiveType() const { return GetType(); }

  bool IsSameType(const TransformOperation& other) const {
    return other.GetType() == GetType();
  }
  virtual bool CanBlendWith(const TransformOperation& other) const = 0;

  virtual bool PreservesAxisAlignment() const { return false; }

  bool Is3DOperation() const {
    OperationType op_type = GetType();
    return op_type == kScaleZ || op_type == kScale3D ||
           op_type == kTranslateZ || op_type == kTranslate3D ||
           op_type == kRotateX || op_type == kRotateY || op_type == kRotate3D ||
           op_type == kMatrix3D || op_type == kPerspective ||
           op_type == kInterpolated;
  }

  virtual bool HasNonTrivial3DComponent() const { return Is3DOperation(); }

  virtual bool DependsOnBoxSize() const { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TransformOperation);
};

#define DEFINE_TRANSFORM_TYPE_CASTS(thisType)                                \
  DEFINE_TYPE_CASTS(thisType, TransformOperation, transform,                 \
                    thisType::IsMatchingOperationType(transform->GetType()), \
                    thisType::IsMatchingOperationType(transform.GetType()))

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_TRANSFORM_OPERATION_H_
