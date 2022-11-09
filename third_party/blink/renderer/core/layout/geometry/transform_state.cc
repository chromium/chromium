/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
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

#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"

namespace blink {

TransformState& TransformState::operator=(const TransformState& other) {
  accumulated_offset_ = other.accumulated_offset_;
  map_point_ = other.map_point_;
  map_quad_ = other.map_quad_;
  if (map_point_)
    last_planar_point_ = other.last_planar_point_;
  if (map_quad_)
    last_planar_quad_ = other.last_planar_quad_;
  force_accumulating_transform_ = other.force_accumulating_transform_;
  direction_ = other.direction_;

  accumulated_transform_.reset();

  if (other.accumulated_transform_) {
    accumulated_transform_ =
        std::make_unique<gfx::Transform>(*other.accumulated_transform_);
  }

  return *this;
}

void TransformState::TranslateTransform(const PhysicalOffset& offset) {
  if (direction_ == kApplyTransformDirection) {
    accumulated_transform_->PostTranslate(offset.left.ToDouble(),
                                          offset.top.ToDouble());
  } else {
    accumulated_transform_->Translate(offset.left.ToDouble(),
                                      offset.top.ToDouble());
  }
}

void TransformState::TranslateMappedCoordinates(const PhysicalOffset& offset) {
  gfx::Vector2dF adjusted_offset(
      (direction_ == kApplyTransformDirection) ? offset : -offset);
  if (map_point_)
    last_planar_point_ += adjusted_offset;
  if (map_quad_)
    last_planar_quad_ += adjusted_offset;
}

void TransformState::Move(const PhysicalOffset& offset,
                          TransformAccumulation accumulate) {
  if (force_accumulating_transform_)
    accumulate = kAccumulateTransform;

  if (accumulate == kFlattenTransform || !accumulated_transform_) {
    accumulated_offset_ += offset;
  } else {
    ApplyAccumulatedOffset();
    if (accumulated_transform_) {
      // If we're accumulating into an existing transform, apply the
      // translation.
      TranslateTransform(offset);
    } else {
      // Just move the point and/or quad.
      TranslateMappedCoordinates(offset);
    }
  }
}

void TransformState::ApplyAccumulatedOffset() {
  PhysicalOffset offset = accumulated_offset_;
  accumulated_offset_ = PhysicalOffset();
  if (!offset.IsZero()) {
    if (accumulated_transform_) {
      TranslateTransform(offset);
      Flatten();
    } else {
      TranslateMappedCoordinates(offset);
    }
  }
}

// FIXME: We transform AffineTransform to gfx::Transform. This is rather
// inefficient.
void TransformState::ApplyTransform(
    const AffineTransform& transform_from_container,
    TransformAccumulation accumulate) {
  ApplyTransform(transform_from_container.ToTransform(), accumulate);
}

void TransformState::ApplyTransform(
    const gfx::Transform& transform_from_container,
    TransformAccumulation accumulate) {
  if (transform_from_container.IsIdentityOrInteger2dTranslation()) {
    Move(PhysicalOffset::FromVector2dFRound(
             transform_from_container.To2dTranslation()),
         accumulate);
    return;
  }

  ApplyAccumulatedOffset();

  // If we have an accumulated transform from last time, multiply in this
  // transform
  if (accumulated_transform_) {
    if (direction_ == kApplyTransformDirection)
      accumulated_transform_ = std::make_unique<gfx::Transform>(
          transform_from_container * *accumulated_transform_);
    else
      accumulated_transform_->PreConcat(transform_from_container);
  } else if (accumulate == kAccumulateTransform) {
    // Make one if we started to accumulate
    accumulated_transform_ =
        std::make_unique<gfx::Transform>(transform_from_container);
  }

  if (accumulate == kFlattenTransform) {
    if (force_accumulating_transform_) {
      accumulated_transform_->Flatten();
    } else {
      const gfx::Transform* final_transform = accumulated_transform_
                                                  ? accumulated_transform_.get()
                                                  : &transform_from_container;
      FlattenWithTransform(*final_transform);
    }
  }
}

void TransformState::Flatten() {
  DCHECK(!force_accumulating_transform_);

  ApplyAccumulatedOffset();

  if (!accumulated_transform_) {
    return;
  }

  FlattenWithTransform(*accumulated_transform_);
}

PhysicalOffset TransformState::MappedPoint() const {
  gfx::PointF point = last_planar_point_;
  point += gfx::Vector2dF(direction_ == kApplyTransformDirection
                              ? accumulated_offset_
                              : -accumulated_offset_);
  if (accumulated_transform_) {
    point =
        direction_ == kApplyTransformDirection
            ? accumulated_transform_->MapPoint(point)
            : accumulated_transform_->InverseOrIdentity().ProjectPoint(point);
  }
  return PhysicalOffset::FromPointFRound(point);
}

gfx::QuadF TransformState::MappedQuad() const {
  gfx::QuadF quad = last_planar_quad_;
  quad += gfx::Vector2dF((direction_ == kApplyTransformDirection)
                             ? accumulated_offset_
                             : -accumulated_offset_);
  if (!accumulated_transform_)
    return quad;

  if (direction_ == kApplyTransformDirection)
    return accumulated_transform_->MapQuad(quad);

  return accumulated_transform_->InverseOrIdentity().ProjectQuad(quad);
}

const gfx::Transform& TransformState::AccumulatedTransform() const {
  DCHECK(force_accumulating_transform_);
  return *accumulated_transform_;
}

void TransformState::FlattenWithTransform(const gfx::Transform& t) {
  if (direction_ == kApplyTransformDirection) {
    if (map_point_)
      last_planar_point_ = t.MapPoint(last_planar_point_);
    if (map_quad_)
      last_planar_quad_ = t.MapQuad(last_planar_quad_);
  } else {
    gfx::Transform inverse_transform;
    if (t.GetInverse(&inverse_transform)) {
      if (map_point_)
        last_planar_point_ = inverse_transform.ProjectPoint(last_planar_point_);
      if (map_quad_)
        last_planar_quad_ = inverse_transform.ProjectQuad(last_planar_quad_);
    }
  }

  // We could throw away m_accumulatedTransform if we wanted to here, but that
  // would cause thrash when traversing hierarchies with alternating
  // preserve-3d and flat elements.
  if (accumulated_transform_)
    accumulated_transform_->MakeIdentity();
}

}  // namespace blink
