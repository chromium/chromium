/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"

#include <algorithm>
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace blink {

PageScaleConstraintsSet::PageScaleConstraintsSet(Page* page)
    : default_constraints_(-1, 1, 1),
      final_constraints_(ComputeConstraintsStack()),
      page_(page),
      last_contents_width_(0),
      last_vertical_scrollbar_width_(0),
      needs_reset_(false),
      constraints_dirty_(false) {}

void PageScaleConstraintsSet::Trace(Visitor* visitor) const {
  visitor->Trace(page_);
}

void PageScaleConstraintsSet::SetDefaultConstraints(
    const PageScaleConstraints& default_constraints) {
  default_constraints_ = default_constraints;
  constraints_dirty_ = true;
}

const PageScaleConstraints& PageScaleConstraintsSet::DefaultConstraints()
    const {
  return default_constraints_;
}

void PageScaleConstraintsSet::UpdatePageDefinedConstraints(
    const ViewportDescription& description,
    const Length& legacy_fallback_width) {
  page_defined_constraints_ =
      description.Resolve(gfx::SizeF(icb_size_), legacy_fallback_width);

  constraints_dirty_ = true;
}

void PageScaleConstraintsSet::ClearPageDefinedConstraints() {
  page_defined_constraints_ = PageScaleConstraints();
  constraints_dirty_ = true;
}

void PageScaleConstraintsSet::SetUserAgentConstraints(
    const PageScaleConstraints& user_agent_constraints) {
  user_agent_constraints_ = user_agent_constraints;
  constraints_dirty_ = true;
}

void PageScaleConstraintsSet::SetFullscreenConstraints(
    const PageScaleConstraints& fullscreen_constraints) {
  fullscreen_constraints_ = fullscreen_constraints;
  constraints_dirty_ = true;
}

PageScaleConstraints PageScaleConstraintsSet::ComputeConstraintsStack() const {
  PageScaleConstraints constraints = DefaultConstraints();
  constraints.OverrideWith(page_defined_constraints_);
  constraints.OverrideWith(user_agent_constraints_);
  constraints.OverrideWith(fullscreen_constraints_);
  return constraints;
}

void PageScaleConstraintsSet::ComputeFinalConstraints() {
  final_constraints_ = ComputeConstraintsStack();
  AdjustFinalConstraintsToContentsSize();
  constraints_dirty_ = false;
}

void PageScaleConstraintsSet::AdjustFinalConstraintsToContentsSize() {
  if (page_->GetSettings().GetShrinksViewportContentToFit()) {
    final_constraints_.FitToContentsWidth(
        last_contents_width_,
        icb_size_.width() - last_vertical_scrollbar_width_);
  }

  final_constraints_.ResolveAutoInitialScale();
}

void PageScaleConstraintsSet::SetNeedsReset(bool needs_reset) {
  needs_reset_ = needs_reset;
  if (needs_reset)
    constraints_dirty_ = true;
}

void PageScaleConstraintsSet::DidChangeContentsSize(
    gfx::Size contents_size,
    int vertical_scrollbar_width,
    float page_scale_factor) {
  // If a large fixed-width element expanded the size of the document late in
  // loading and our initial scale is not set (or set to be less than the last
  // minimum scale), reset the page scale factor to the new initial scale.
  if (contents_size.width() > last_contents_width_ &&
      page_scale_factor == FinalConstraints().minimum_scale &&
      ComputeConstraintsStack().initial_scale <
          FinalConstraints().minimum_scale)
    SetNeedsReset(true);

  constraints_dirty_ = true;
  last_vertical_scrollbar_width_ = vertical_scrollbar_width;
  last_contents_width_ = contents_size.width();
}

static float ComputeDeprecatedTargetDensityDPIFactor(
    const ViewportDescription& description) {
  if (description.deprecated_target_density_dpi ==
      ViewportDescription::kValueDeviceDPI)
    return 1.0f;

  float target_dpi = -1.0f;
  if (description.deprecated_target_density_dpi ==
      ViewportDescription::kValueLowDPI)
    target_dpi = 120.0f;
  else if (description.deprecated_target_density_dpi ==
           ViewportDescription::kValueMediumDPI)
    target_dpi = 160.0f;
  else if (description.deprecated_target_density_dpi ==
           ViewportDescription::kValueHighDPI)
    target_dpi = 240.0f;
  else if (description.deprecated_target_density_dpi !=
           ViewportDescription::kValueAuto)
    target_dpi = description.deprecated_target_density_dpi;
  return target_dpi > 0 ? 160.0f / target_dpi : 1.0f;
}

static float GetLayoutWidthForNonWideViewport(const gfx::Size& device_size,
                                              float initial_scale) {
  return initial_scale == -1 ? device_size.width()
                             : device_size.width() / initial_scale;
}

static float ComputeHeightByAspectRatio(float width,
                                        const gfx::Size& device_size) {
  return width * device_size.height() / device_size.width();
}

void PageScaleConstraintsSet::DidChangeInitialContainingBlockSize(
    const gfx::Size& size) {
  if (icb_size_ == size)
    return;

  icb_size_ = size;
  constraints_dirty_ = true;
}

gfx::Size PageScaleConstraintsSet::GetLayoutSize() const {
  return gfx::ToFlooredSize(ComputeConstraintsStack().layout_size);
}

void PageScaleConstraintsSet::AdjustForAndroidWebViewQuirks(
    const ViewportDescription& description,
    int layout_fallback_width,
    bool support_target_density_dpi,
    bool wide_viewport_quirk_enabled,
    bool use_wide_viewport,
    bool load_with_overview_mode,
    bool non_user_scalable_quirk_enabled) {
  if (!support_target_density_dpi && !wide_viewport_quirk_enabled &&
      load_with_overview_mode && !non_user_scalable_quirk_enabled)
    return;

  const float old_initial_scale = page_defined_constraints_.initial_scale;
  if (!load_with_overview_mode) {
    bool reset_initial_scale = false;
    if (description.zoom == -1) {
      if (description.max_width.IsAuto() ||
          description.max_width.IsExtendToZoom())
        reset_initial_scale = true;
      if (use_wide_viewport || description.max_width.IsDeviceWidth())
        reset_initial_scale = true;
    }
    if (reset_initial_scale)
      page_defined_constraints_.initial_scale = 1.0f;
  }

  float adjusted_layout_size_width =
      page_defined_constraints_.layout_size.width();
  float adjusted_layout_size_height =
      page_defined_constraints_.layout_size.height();
  float target_density_dpi_factor = 1.0f;

  if (support_target_density_dpi) {
    target_density_dpi_factor =
        ComputeDeprecatedTargetDensityDPIFactor(description);
    if (page_defined_constraints_.initial_scale != -1)
      page_defined_constraints_.initial_scale *= target_density_dpi_factor;
    if (page_defined_constraints_.minimum_scale != -1)
      page_defined_constraints_.minimum_scale *= target_density_dpi_factor;
    if (page_defined_constraints_.maximum_scale != -1)
      page_defined_constraints_.maximum_scale *= target_density_dpi_factor;
    if (wide_viewport_quirk_enabled &&
        (!use_wide_viewport || description.max_width.IsDeviceWidth())) {
      adjusted_layout_size_width /= target_density_dpi_factor;
      adjusted_layout_size_height /= target_density_dpi_factor;
    }
  }

  if (wide_viewport_quirk_enabled) {
    if (use_wide_viewport &&
        (description.max_width.IsAuto() ||
         description.max_width.IsExtendToZoom()) &&
        description.zoom != 1.0f) {
      if (layout_fallback_width)
        adjusted_layout_size_width = layout_fallback_width;
      adjusted_layout_size_height =
          ComputeHeightByAspectRatio(adjusted_layout_size_width, icb_size_);
    } else if (!use_wide_viewport) {
      const float non_wide_scale =
          description.zoom < 1 && !description.max_width.IsDeviceWidth() &&
                  !description.max_width.IsDeviceHeight()
              ? -1
              : old_initial_scale;
      adjusted_layout_size_width =
          GetLayoutWidthForNonWideViewport(icb_size_, non_wide_scale) /
          target_density_dpi_factor;
      float new_initial_scale = target_density_dpi_factor;
      if (user_agent_constraints_.initial_scale != -1 &&
          (description.max_width.IsDeviceWidth() ||
           ((description.max_width.IsAuto() ||
             description.max_width.IsExtendToZoom()) &&
            description.zoom == -1))) {
        adjusted_layout_size_width /= user_agent_constraints_.initial_scale;
        new_initial_scale = user_agent_constraints_.initial_scale;
      }
      adjusted_layout_size_height =
          ComputeHeightByAspectRatio(adjusted_layout_size_width, icb_size_);
      if (description.zoom < 1) {
        page_defined_constraints_.initial_scale = new_initial_scale;
        if (page_defined_constraints_.minimum_scale != -1)
          page_defined_constraints_.minimum_scale =
              std::min<float>(page_defined_constraints_.minimum_scale,
                              page_defined_constraints_.initial_scale);
        if (page_defined_constraints_.maximum_scale != -1)
          page_defined_constraints_.maximum_scale =
              std::max<float>(page_defined_constraints_.maximum_scale,
                              page_defined_constraints_.initial_scale);
      }
    }
  }

  if (non_user_scalable_quirk_enabled && !description.user_zoom) {
    page_defined_constraints_.initial_scale = target_density_dpi_factor;
    page_defined_constraints_.minimum_scale =
        page_defined_constraints_.initial_scale;
    page_defined_constraints_.maximum_scale =
        page_defined_constraints_.initial_scale;
    if (description.max_width.IsAuto() ||
        description.max_width.IsExtendToZoom() ||
        description.max_width.IsDeviceWidth()) {
      adjusted_layout_size_width =
          icb_size_.width() / target_density_dpi_factor;
      adjusted_layout_size_height =
          ComputeHeightByAspectRatio(adjusted_layout_size_width, icb_size_);
    }
  }

  page_defined_constraints_.layout_size.set_width(adjusted_layout_size_width);
  page_defined_constraints_.layout_size.set_height(adjusted_layout_size_height);
}

}  // namespace blink
