// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper_transform_cache.h"

#include <memory>

#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"

namespace blink {

// All transform caches invalidate themselves by tracking a local cache
// generation, and invalidating their cache if their cache generation disagrees
// with s_global_generation.
unsigned GeometryMapperTransformCache::s_global_generation = 1;

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
    to_2d_translation_root_ = gfx::Vector2dF();
    root_of_2d_translation_ = &node;
    plane_root_transform_ = nullptr;
    screen_transform_ = std::nullopt;
    screen_transform_updated_ = true;

    DCHECK(node.ScrollNode());
    nearest_scroll_translation_ = scroll_translation_state_ = &node;
    return;
  }

  const GeometryMapperTransformCache& parent =
      node.UnaliasedParent()->GetTransformCache();

  has_sticky_or_anchor_position_ =
      node.RequiresCompositingForStickyPosition() ||
      node.RequiresCompositingForAnchorPosition() ||
      parent.has_sticky_or_anchor_position_;

  is_backface_hidden_ =
      node.IsBackfaceHiddenInternal(parent.is_backface_hidden_);

  nearest_scroll_translation_ =
      node.ScrollNode() ? &node : parent.nearest_scroll_translation_.Get();
  scroll_translation_state_ = node.ScrollTranslationForFixed()
                                  ? node.ScrollTranslationForFixed()
                                  : nearest_scroll_translation_.Get();

  nearest_directly_composited_ancestor_ =
      node.HasDirectCompositingReasons()
          ? &node
          : parent.nearest_directly_composited_ancestor_.Get();

  if (node.IsIdentityOr2dTranslation() && !node.HasActiveTransformAnimation()) {
    root_of_2d_translation_ = parent.root_of_2d_translation_;
    to_2d_translation_root_ = parent.to_2d_translation_root_;
    const auto& translation = node.Get2dTranslation();
    to_2d_translation_root_ += translation;

    if (parent.plane_root_transform_) {
      plane_root_transform_ = MakeGarbageCollected<PlaneRootTransform>();
      plane_root_transform_->plane_root = parent.plane_root();
      plane_root_transform_->to_plane_root = parent.to_plane_root();
      plane_root_transform_->to_plane_root.Translate(translation.x(),
                                                     translation.y());
      plane_root_transform_->from_plane_root = parent.from_plane_root();
      plane_root_transform_->from_plane_root.PostTranslate(-translation.x(),
                                                           -translation.y());
      plane_root_transform_->has_animation =
          parent.has_animation_to_plane_root();
    } else {
      // The parent doesn't have plane_root_transform_ means that the parent's
      // plane root is the same as the 2d translation root, so this node
      // which is a 2d translation also doesn't need plane root transform
      // because the plane root is still the same as the 2d translation root.
      plane_root_transform_ = nullptr;
    }
  } else {
    root_of_2d_translation_ = &node;
    to_2d_translation_root_ = gfx::Vector2dF();

    gfx::Transform local = node.MatrixWithOriginApplied();
    bool is_plane_root = !local.IsFlat() || !local.IsInvertible();
    if (is_plane_root) {
      // We don't need plane root transform because the plane root is the same
      // as the 2d translation root.
      plane_root_transform_ = nullptr;
    } else {
      plane_root_transform_ = MakeGarbageCollected<PlaneRootTransform>();
      plane_root_transform_->plane_root = parent.plane_root();
      plane_root_transform_->to_plane_root.MakeIdentity();
      parent.ApplyToPlaneRoot(plane_root_transform_->to_plane_root);
      plane_root_transform_->to_plane_root.PreConcat(local);
      plane_root_transform_->from_plane_root = local.GetCheckedInverse();
      parent.ApplyFromPlaneRoot(plane_root_transform_->from_plane_root);
      plane_root_transform_->has_animation =
          parent.has_animation_to_plane_root() ||
          node.HasActiveTransformAnimation();
    }
  }

  // screen_transform_ will be updated only when needed.
  if (plane_root()->IsRoot()) {
    // We won't need screen_transform_.
    screen_transform_ = std::nullopt;
    screen_transform_updated_ = true;
  } else {
    screen_transform_updated_ = false;
  }
}

void GeometryMapperTransformCache::UpdateScreenTransform(
    const TransformPaintPropertyNode& node) {
  // The cache should have been updated.
  DCHECK_EQ(cache_generation_, s_global_generation);

  if (screen_transform_updated_)
    return;

  screen_transform_updated_ = true;

  // screen_transform_updated_ should have set to true in Update() if any of
  // the following DCHECKs would fail.
  DCHECK(!plane_root()->IsRoot());
  DCHECK(!node.IsRoot());

  auto* parent_node = node.UnaliasedParent();
  parent_node->UpdateScreenTransform();
  const auto& parent = parent_node->GetTransformCache();

  screen_transform_.emplace(ScreenTransform());
  parent.ApplyToScreen(screen_transform_->to_screen);
  if (node.FlattensInheritedTransform())
    screen_transform_->to_screen.Flatten();
  if (node.IsIdentityOr2dTranslation()) {
    const auto& translation = node.Get2dTranslation();
    screen_transform_->to_screen.Translate(translation.x(), translation.y());
  } else {
    screen_transform_->to_screen.PreConcat(node.MatrixWithOriginApplied());
  }

  auto to_screen_flattened = screen_transform_->to_screen;
  to_screen_flattened.Flatten();
  screen_transform_->projection_from_screen_is_valid =
      to_screen_flattened.GetInverse(
          &screen_transform_->projection_from_screen);

  screen_transform_->has_animation |= node.HasActiveTransformAnimation();
}

}  // namespace blink
