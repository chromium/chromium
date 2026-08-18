// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/geometry.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkRect.h"

namespace skia {

float FractionTriangleCovered(std::array<SkPoint, 3>& a,
                              std::array<SkPoint, 3>& b) {
  if (!a[0].isFinite() || !a[1].isFinite() || !a[2].isFinite() ||
      !b[0].isFinite() || !b[1].isFinite() || !b[2].isFinite()) {
    return 0.f;
  }

  // Ensure triangle `a` is oriented counter-clockwise in-place.
  float area_a_signed = SkPoint::CrossProduct(a[1] - a[0], a[2] - a[0]) * 0.5f;
  if (area_a_signed < 0.f) {
    std::swap(a[1], a[2]);
  } else if (area_a_signed == 0.f) {
    return 0.f;
  }

  // Ensure triangle `b` is oriented counter-clockwise in-place.
  float area_b_signed = SkPoint::CrossProduct(b[1] - b[0], b[2] - b[0]) * 0.5f;
  if (area_b_signed < 0.f) {
    std::swap(b[1], b[2]);
    area_b_signed = -area_b_signed;
  } else if (area_b_signed == 0.f) {
    return 0.f;
  }
  const float area_b = area_b_signed;

  // Set up subject polygon (initially triangle `b`).
  // Clipping a triangle against 3 half-planes produces at most 6 vertices.
  std::array<SkPoint, 8> p = {b[0], b[1], b[2]};
  size_t p_count = 3;

  // Sutherland-Hodgman clipping: clip subject polygon `p` against each edge
  // of `a`.
  for (size_t a_edge_index = 0; a_edge_index < 3; ++a_edge_index) {
    if (p_count < 3) {
      return 0.f;
    }
    // `a1`, `a2`, and `a_edge` represent the `a_edge_index`-th edge of `a`.
    const SkPoint& a1 = a[a_edge_index];
    const SkPoint& a2 = a[(a_edge_index + 1) % 3];
    const SkVector a_edge = a2 - a1;

    // Intersect this edge of `a` with all edges of `p`, producing the new
    // polygon `next_p`.
    std::array<SkPoint, 8> next_p;
    size_t next_p_count = 0;
    for (size_t p_edge_index = 0; p_edge_index < p_count; ++p_edge_index) {
      const SkPoint& p1 = p[p_edge_index];
      const SkPoint& p2 = p[(p_edge_index + 1) % p_count];

      // Determine if `p1` and `p2` are inside the half-plane bounded by
      // `a_edge` (points to the left of the directed edge have cross
      // product >= 0).
      float d1 = a_edge.cross(p1 - a1);
      float d2 = a_edge.cross(p2 - a1);
      bool p1_inside = (d1 >= 0.f);
      bool p2_inside = (d2 >= 0.f);

      // Compute the intersection if `p1` and `p2` are on opposite sides of
      // `a_edge`.
      SkPoint intersection;
      if (p1_inside != p2_inside) {
        float denom = d1 - d2;
        float t = std::abs(denom) > 1e-6f ? (d1 / denom) : 0.f;
        intersection = p1 + (p2 - p1) * t;
      }

      // Insert `p2` and/or `intersection` into `next_p`.
      if (p2_inside) {
        if (!p1_inside) {
          next_p[next_p_count++] = intersection;
        }
        next_p[next_p_count++] = p2;
      } else if (p1_inside) {
        next_p[next_p_count++] = intersection;
      }
    }

    // Replace `p` with `next_p` for the next iteration.
    p = next_p;
    p_count = next_p_count;
  }
  if (p_count < 3) {
    return 0.f;
  }

  // Compute intersection polygon area using the Shoelace formula.
  float intersection_area = 0.f;
  for (size_t i = 0; i < p_count; ++i) {
    intersection_area += SkPoint::CrossProduct(p[i], p[(i + 1) % p_count]);
  }
  intersection_area = std::abs(intersection_area * 0.5f);

  return std::clamp(intersection_area / area_b, 0.f, 1.f);
}

SkRect ScaleSkRectProportional(const SkRect& output_bounds,
                               const SkRect& input_bounds,
                               const SkRect& input_rect) {
  float scale_x = output_bounds.width() / input_bounds.width();
  float scale_y = output_bounds.height() / input_bounds.height();
  return SkRect::MakeLTRB(
      (input_rect.fLeft - input_bounds.fLeft) * scale_x + output_bounds.fLeft,
      (input_rect.fTop - input_bounds.fTop) * scale_y + output_bounds.fTop,
      (input_rect.fRight - input_bounds.fRight) * scale_x +
          output_bounds.fRight,
      (input_rect.fBottom - input_bounds.fBottom) * scale_y +
          output_bounds.fBottom);
}

Tiling::Tiling(const SkRect& dest_rect,
               std::vector<SkRect> source_rects,
               std::vector<sk_sp<SkImage>> source_images,
               int32_t source_max_size)
    : dest_rect_(dest_rect),
      source_count_(source_rects.size()),
      source_rects_(std::move(source_rects)),
      source_images_(std::move(source_images)) {
  if (source_max_size == 0) {
    // A maximum size of 0 is specified for software rasterization.
    tile_size_ = SkSize::Make(dest_rect_.width(), dest_rect_.height());
  } else {
    // Compute the amount by which `dest_rect_` would need to be scaled down to
    // ensure all of `source_rects` fit in `source_max_size`.
    float tile_dest_width = dest_rect_.width();
    float tile_dest_height = dest_rect_.height();
    for (size_t i = 0; i < source_count_; ++i) {
      // Source images that are already textures don't need to be tiled for
      // upload.
      if (source_images_[i]->isTextureBacked()) {
        continue;
      }

      // Find the dest tile width and height that corresponds to sampling
      // `source_max_size` pixels. In this computation, add a padding of 2
      // pixels to the source rect, to account for sampling pixels outside
      // of the rect.
      float kPadding = 2.f;
      tile_dest_width = std::min(
          tile_dest_width,
          dest_rect_.width() *
              (source_max_size / (source_rects_[i].width() + kPadding)));
      tile_dest_height = std::min(
          tile_dest_height,
          dest_rect_.height() *
              (source_max_size / (source_rects_[i].height() + kPadding)));
    }

    // Prefer integer-sized tiles, so round down to the nearest integer, unless
    // that would take us to zero.
    if (tile_dest_width > 1.f) {
      tile_dest_width = std::floor(tile_dest_width);
    }
    if (tile_dest_height > 1.f) {
      tile_dest_height = std::floor(tile_dest_height);
    }

    tile_size_ = SkSize::Make(tile_dest_width, tile_dest_height);
  }
  tile_count_ =
      SkISize::Make(std::ceil(dest_rect_.width() / tile_size_.width()),
                    std::ceil(dest_rect_.height() / tile_size_.height()));
}

Tiling::~Tiling() = default;

void Tiling::GetTileRect(
    int x,
    int y,
    SkRect& tile_dest_rect,
    std::vector<SkRect>& tile_source_rects,
    std::vector<std::optional<SkIRect>>& tile_source_subset_rects) {
  // Compute the destination tile rect.
  tile_dest_rect =
      SkRect::MakeLTRB(dest_rect_.x() + x * tile_size_.width(),
                       dest_rect_.y() + y * tile_size_.height(),
                       dest_rect_.x() + (x + 1) * tile_size_.width(),
                       dest_rect_.y() + (y + 1) * tile_size_.height());
  tile_dest_rect.intersect(dest_rect_);

  tile_source_rects.resize(source_count_);
  tile_source_subset_rects.resize(source_count_);
  for (size_t i = 0; i < source_rects_.size(); ++i) {
    // Compute the tile's source rect to have the same relationship to the
    // source rect as the tile's dest rect has to the dest rect.
    SkRect source_rect =
        ScaleSkRectProportional(source_rects_[i], dest_rect_, tile_dest_rect);

    const auto& image = source_images_[i];

    // If the image is texture-backed, then use it directly.
    if (image->isTextureBacked()) {
      tile_source_rects[i] = source_rect;
      tile_source_subset_rects[i] = std::nullopt;
      continue;
    }

    // Otherwise, find the subset of the image that could be sampled, and report
    // that subset rectangle and the source rectangle to sample it.
    SkIRect subset_rect = SkIRect::MakeEmpty();
    source_rect.roundOut(&subset_rect);
    if (subset_rect.intersect(image->bounds())) {
      source_rect =
          skia::ScaleSkRectProportional(SkRect::Make(subset_rect.size()),
                                        SkRect::Make(subset_rect), source_rect);
    } else {
      subset_rect = SkIRect::MakeEmpty();
      source_rect = SkRect::MakeEmpty();
    }
    tile_source_rects[i] = source_rect;
    tile_source_subset_rects[i] = subset_rect;
  }
}

}  // namespace skia
