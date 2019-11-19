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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_IDENTITY_TRANSFORM_OPERATION_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_IDENTITY_TRANSFORM_OPERATION_H_

#include "third_party/blink/renderer/platform/transforms/transform_operation.h"

namespace blink {

class PLATFORM_EXPORT IdentityTransformOperation final
    : public TransformOperation {
 public:
  static scoped_refptr<IdentityTransformOperation> Create() {
    return base::AdoptRef(new IdentityTransformOperation());
  }

  bool CanBlendWith(const TransformOperation& other) const override {
    return IsSameType(other);
  }

 private:
  OperationType GetType() const override { return kIdentity; }

  bool operator==(const TransformOperation& o) const override {
    return IsSameType(o);
  }

  void Apply(TransformationMatrix&, const FloatSize&) const override {}

  scoped_refptr<TransformOperation> Accumulate(
      const TransformOperation& other) override {
    NOTREACHED();
    return this;
  }

  scoped_refptr<TransformOperation> Blend(
      const TransformOperation*,
      double progress,
      bool blend_to_identity = false) override {
    return this;
  }

  scoped_refptr<TransformOperation> Zoom(double factor) final { return this; }

  bool PreservesAxisAlignment() const final { return true; }

  IdentityTransformOperation() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_IDENTITY_TRANSFORM_OPERATION_H_
