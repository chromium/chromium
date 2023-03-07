/*
 * Copyright (C) 2012, 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_PATH_2D_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_PATH_2D_H_

#include "base/notreached.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_matrix_2d_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_path2d_string.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_path.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/identifiability_study_helper.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

class ExceptionState;

class MODULES_EXPORT Path2D final : public ScriptWrappable, public CanvasPath {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Path2D* Create(ExecutionContext* context,
                        const V8UnionPath2DOrString* path) {
    DCHECK(path);
    switch (path->GetContentType()) {
      case V8UnionPath2DOrString::ContentType::kPath2D:
        return MakeGarbageCollected<Path2D>(context, path->GetAsPath2D());
      case V8UnionPath2DOrString::ContentType::kString:
        return MakeGarbageCollected<Path2D>(context, path->GetAsString());
    }
    NOTREACHED();
    return nullptr;
  }
  static Path2D* Create(ExecutionContext* context) {
    return MakeGarbageCollected<Path2D>(context);
  }
  static Path2D* Create(ExecutionContext* context, const Path& path) {
    return MakeGarbageCollected<Path2D>(context, path);
  }

  void addPath(Path2D* path,
               DOMMatrix2DInit* transform,
               ExceptionState& exception_state) {
    DOMMatrixReadOnly* matrix =
        DOMMatrixReadOnly::fromMatrix2D(transform, exception_state);
    if (!matrix || !std::isfinite(matrix->m11()) ||
        !std::isfinite(matrix->m12()) || !std::isfinite(matrix->m21()) ||
        !std::isfinite(matrix->m22()) || !std::isfinite(matrix->m41()) ||
        !std::isfinite(matrix->m42()))
      return;
    GetModifiablePath().AddPath(path->GetPath(), matrix->GetAffineTransform());
    if (identifiability_study_helper_.ShouldUpdateBuilder()) {
      identifiability_study_helper_.UpdateBuilder(CanvasOps::kAddPath,
                                                  path->GetIdentifiableToken());
    }
  }

  void Trace(Visitor*) const override;

  ExecutionContext* GetTopExecutionContext() const override { return context_; }

  explicit Path2D(ExecutionContext* context) : context_(context) {
    identifiability_study_helper_.SetExecutionContext(context);
    GetModifiablePath().SetIsVolatile(false);
  }
  Path2D(ExecutionContext* context, const Path& path)
      : CanvasPath(path), context_(context) {
    identifiability_study_helper_.SetExecutionContext(context);
    GetModifiablePath().SetIsVolatile(false);
  }
  Path2D(ExecutionContext* context, Path2D* path)
      : Path2D(context, path->GetPath()) {}
  Path2D(ExecutionContext* context, const String& path_data)
      : context_(context) {
    identifiability_study_helper_.SetExecutionContext(context);
    BuildPathFromString(path_data, GetModifiablePath());
    GetModifiablePath().SetIsVolatile(false);
  }

  Path2D(const Path2D&) = delete;
  Path2D& operator=(const Path2D&) = delete;

  ~Path2D() override = default;

 private:
  Member<ExecutionContext> context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_PATH_2D_H_
