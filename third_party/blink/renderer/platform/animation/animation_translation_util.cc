/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/animation/animation_translation_util.h"

#include "third_party/blink/renderer/platform/transforms/interpolated_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/matrix_3d_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/matrix_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/perspective_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/rotate_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/scale_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/skew_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/transform_operations.h"
#include "third_party/blink/renderer/platform/transforms/translate_transform_operation.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_operations.h"

namespace blink {

void ToGfxTransformOperations(
    const TransformOperations& transform_operations,
    gfx::TransformOperations* out_transform_operations,
    const gfx::SizeF& box_size) {
  // We need to do a deep copy the transformOperations may contain ref pointers
  // to TransformOperation objects.
  for (const auto& operation : transform_operations.Operations()) {
    switch (operation->GetType()) {
      case TransformOperation::kScaleX:
      case TransformOperation::kScaleY:
      case TransformOperation::kScaleZ:
      case TransformOperation::kScale3D:
      case TransformOperation::kScale: {
        auto* transform =
            static_cast<const ScaleTransformOperation*>(operation.Get());
        out_transform_operations->AppendScale(SkDoubleToScalar(transform->X()),
                                              SkDoubleToScalar(transform->Y()),
                                              SkDoubleToScalar(transform->Z()));
        break;
      }
      case TransformOperation::kTranslateX:
      case TransformOperation::kTranslateY:
      case TransformOperation::kTranslateZ:
      case TransformOperation::kTranslate3D:
      case TransformOperation::kTranslate: {
        auto* transform =
            static_cast<const TranslateTransformOperation*>(operation.Get());
        out_transform_operations->AppendTranslate(
            SkDoubleToScalar(transform->X(box_size)),
            SkDoubleToScalar(transform->Y(box_size)),
            SkDoubleToScalar(transform->Z()));
        break;
      }
      case TransformOperation::kRotateX:
      case TransformOperation::kRotateY:
      case TransformOperation::kRotateZ:
      case TransformOperation::kRotate3D:
      case TransformOperation::kRotate: {
        auto* transform =
            static_cast<const RotateTransformOperation*>(operation.Get());
        out_transform_operations->AppendRotate(
            SkDoubleToScalar(transform->X()), SkDoubleToScalar(transform->Y()),
            SkDoubleToScalar(transform->Z()),
            SkDoubleToScalar(transform->Angle()));
        break;
      }
      case TransformOperation::kSkewX: {
        auto* transform =
            static_cast<const SkewTransformOperation*>(operation.Get());
        out_transform_operations->AppendSkewX(
            SkDoubleToScalar(transform->AngleX()));
        break;
      }
      case TransformOperation::kSkewY: {
        auto* transform =
            static_cast<const SkewTransformOperation*>(operation.Get());
        out_transform_operations->AppendSkewY(
            SkDoubleToScalar(transform->AngleY()));
        break;
      }
      case TransformOperation::kSkew: {
        auto* transform =
            static_cast<const SkewTransformOperation*>(operation.Get());
        out_transform_operations->AppendSkew(
            SkDoubleToScalar(transform->AngleX()),
            SkDoubleToScalar(transform->AngleY()));
        break;
      }
      case TransformOperation::kMatrix: {
        auto* transform =
            static_cast<const MatrixTransformOperation*>(operation.Get());
        out_transform_operations->AppendMatrix(transform->Matrix());
        break;
      }
      case TransformOperation::kMatrix3D: {
        auto* transform =
            static_cast<const Matrix3DTransformOperation*>(operation.Get());
        out_transform_operations->AppendMatrix(transform->Matrix());
        break;
      }
      case TransformOperation::kPerspective: {
        auto* transform =
            static_cast<const PerspectiveTransformOperation*>(operation.Get());
        std::optional<double> depth = transform->Perspective();
        if (depth) {
          out_transform_operations->AppendPerspective(
              SkDoubleToScalar(std::max(*depth, 1.0)));
        } else {
          out_transform_operations->AppendPerspective(std::nullopt);
        }
        break;
      }
      case TransformOperation::kRotateAroundOrigin:
      case TransformOperation::kInterpolated: {
        gfx::Transform m;
        operation->Apply(m, box_size);
        out_transform_operations->AppendMatrix(m);
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }  // switch
  }    // for each operation
}

}  // namespace blink
