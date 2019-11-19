/*
 * Copyright (C) 2006, 2007, 2009, 2010, 2011, 2012 Apple Inc. All rights
 * reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_PATH_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_PATH_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ExceptionState;

class MODULES_EXPORT CanvasPath {
  DISALLOW_NEW();

 public:
  virtual ~CanvasPath() = default;

  void closePath();
  void moveTo(double double_x, double double_y);
  void lineTo(double double_x, double double_y);
  void quadraticCurveTo(double double_cpx,
                        double double_cpy,
                        double double_x,
                        double double_y);
  void bezierCurveTo(double double_cp1x,
                     double double_cp1y,
                     double double_cp2x,
                     double double_cp2y,
                     double double_x,
                     double double_y);
  void arcTo(double double_x1,
             double double_y1,
             double double_x2,
             double double_y2,
             double double_radius,
             ExceptionState&);
  void arc(double double_x,
           double double_y,
           double double_radius,
           double double_start_angle,
           double double_end_angle,
           bool anticlockwise,
           ExceptionState&);
  void ellipse(double double_x,
               double double_y,
               double double_radius_x,
               double double_radius_y,
               double double_rotation,
               double double_start_angle,
               double double_end_angle,
               bool anticlockwise,
               ExceptionState&);
  void rect(double double_x,
            double double_y,
            double double_width,
            double double_height);

  virtual bool IsTransformInvertible() const { return true; }
  virtual AffineTransform Transform() const {
    return AffineTransform(1, 0, 0, 1, 0, 0);
  }

 protected:
  CanvasPath() { path_.SetIsVolatile(true); }
  CanvasPath(const Path& path) : path_(path) { path_.SetIsVolatile(true); }
  Path path_;
};
}  // namespace blink

#endif
