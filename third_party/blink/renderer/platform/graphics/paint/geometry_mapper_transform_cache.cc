// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper_transform_cache.h"

#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"

namespace blink {

// All transform caches invalidate themselves by tracking a local cache
// generation, and invalidating their cache if their cache generation disagrees
// with s_global_generation.
unsigned GeometryMapperTransformCache::s_global_generation;

void GeometryMapperTransformCache::ClearCache() {
  s_global_generation++;
}

bool GeometryMapperTransformCache::IsValid() const {
  return cache_generation_ == s_global_generation;
}

void GeometryMapperTransformCache::Update(
    const TransformPaintPropertyNode& node) {
  DCHECK_NE(cache_generation_, s_global_generation);
  cache_generation_ = s_global_generation;

  if (node.IsRoot()) {
    DCHECK(node.IsIdentity());
    to_2d_translation_root_ = FloatSize();
    root_of_2d_translation_ = &node;
    plane_root_transform_ = nullptr;
    screen_transform_ = nullptr;
    return;
  }

  const GeometryMapperTransformCache& parent =
      node.Parent()->GetTransformCache();

  // screen_transform_ will be updated only when needed.
  screen_transform_ = nullptr;

  if (node.IsIdentityOr2DTranslation()) {
    root_of_2d_translation_ = parent.root_of_2d_translation_;
    to_2d_translation_root_ = parent.to_2d_translation_root_;
    const auto& translation = node.Translation2D();
    to_2d_translation_root_ += translation;

    if (parent.plane_root_transform_) {
      if (!plane_root_transform_)
        plane_root_transform_.reset(new PlaneRootTransform());
      plane_root_transform_->plane_root = parent.plane_root();
      plane_root_transform_->to_plane_root = parent.to_plane_root();
      plane_root_transform_->to_plane_root.Translate(translation.Width(),
                                                     translation.Height());
      plane_root_transform_->from_plane_root = parent.from_plane_root();
      plane_root_transform_->from_plane_root.PostTranslate(
          -translation.Width(), -translation.Height());
    } else {
      // The parent doesn't have plane_root_transform_ means that the parent's
      // plane root is the same as the 2d translation root, so this node
      // which is a 2d translation also doesn't need plane root transform
      // because the plane root is still the same as the 2d translation root.
      plane_root_transform_ = nullptr;
    }
    return;
  }

  root_of_2d_translation_ = &node;
  to_2d_translation_root_ = FloatSize();

  TransformationMatrix local = node.MatrixWithOriginApplied();
  bool is_plane_root = !local.IsFlat() || !local.IsInvertible();
  if (is_plane_root && root_of_2d_translation_ == &node) {
    // We don't need plane root transform because the plane root is the same
    // as the 2d translation root.
    plane_root_transform_ = nullptr;
    return;
  }

  if (!plane_root_transform_)
    plane_root_transform_.reset(new PlaneRootTransform());

  if (is_plane_root) {
    plane_root_transform_->plane_root = &node;
    plane_root_transform_->to_plane_root.MakeIdentity();
    plane_root_transform_->from_plane_root.MakeIdentity();
  } else {
    plane_root_transform_->plane_root = parent.plane_root();
    plane_root_transform_->to_plane_root.MakeIdentity();
    parent.ApplyToPlaneRoot(plane_root_transform_->to_plane_root);
    plane_root_transform_->to_plane_root.Multiply(local);
    plane_root_transform_->from_plane_root = local.Inverse();
    parent.ApplyFromPlaneRoot(plane_root_transform_->from_plane_root);
  }
}

void GeometryMapperTransformCache::UpdateScreenTransform(
    const TransformPaintPropertyNode& node) {
  // The cache should have been updated.
  DCHECK_EQ(cache_generation_, s_global_generation);

  // If the plane root is the root of the tree, we can just use the plane root
  // transform as the screen transform.
  if (plane_root()->IsRoot())
    return;

  // If the node is the root, then its plane root is itself, and we should have
  // returned above.
  DCHECK(!node.IsRoot());
  node.Parent()->UpdateScreenTransform();
  const auto& parent = node.Parent()->GetTransformCache();

  screen_transform_.reset(new ScreenTransform());
  parent.ApplyToScreen(screen_transform_->to_screen);
  if (node.FlattensInheritedTransform())
    screen_transform_->to_screen.FlattenTo2d();
  if (node.IsIdentityOr2DTranslation()) {
    const auto& translation = node.Translation2D();
    screen_transform_->to_screen.Translate(translation.Width(),
                                           translation.Height());
  } else {
    screen_transform_->to_screen.Multiply(node.MatrixWithOriginApplied());
  }

  auto to_screen_flattened = screen_transform_->to_screen;
  to_screen_flattened.FlattenTo2d();
  screen_transform_->projection_from_screen_is_valid =
      to_screen_flattened.IsInvertible();
  if (screen_transform_->projection_from_screen_is_valid)
    screen_transform_->projection_from_screen = to_screen_flattened.Inverse();
}

#if DCHECK_IS_ON()
void GeometryMapperTransformCache::CheckScreenTransformUpdated() const {
  // We should create screen transform iff the plane root is not the root.
  DCHECK_EQ(plane_root()->IsRoot(), !screen_transform_);
}
#endif

}  // namespace blink
