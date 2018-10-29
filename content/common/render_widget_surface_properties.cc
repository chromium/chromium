// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/render_widget_surface_properties.h"

namespace content {

// static
RenderWidgetSurfaceProperties
RenderWidgetSurfaceProperties::FromCompositorFrame(
    const viz::CompositorFrame& frame) {
  RenderWidgetSurfaceProperties properties;
  properties.size = frame.size_in_pixels();
  properties.device_scale_factor = frame.device_scale_factor();
  properties.top_controls_height = frame.metadata.top_controls_height;
  properties.top_controls_shown_ratio = frame.metadata.top_controls_shown_ratio;
#ifdef OS_ANDROID
  properties.bottom_controls_height = frame.metadata.bottom_controls_height;
  properties.bottom_controls_shown_ratio =
      frame.metadata.bottom_controls_shown_ratio;
  properties.selection = frame.metadata.selection;
  properties.has_transparent_background =
      frame.render_pass_list.back()->has_transparent_background;
#endif
  return properties;
}

RenderWidgetSurfaceProperties::RenderWidgetSurfaceProperties() = default;

RenderWidgetSurfaceProperties::RenderWidgetSurfaceProperties(
    const RenderWidgetSurfaceProperties& other) = default;

RenderWidgetSurfaceProperties::~RenderWidgetSurfaceProperties() = default;

RenderWidgetSurfaceProperties& RenderWidgetSurfaceProperties::operator=(
    const RenderWidgetSurfaceProperties& other) = default;

bool RenderWidgetSurfaceProperties::operator==(
    const RenderWidgetSurfaceProperties& other) const {
  return other.device_scale_factor == device_scale_factor &&
         other.top_controls_height == top_controls_height &&
         other.top_controls_shown_ratio == top_controls_shown_ratio &&
#ifdef OS_ANDROID
         other.bottom_controls_height == bottom_controls_height &&
         other.bottom_controls_shown_ratio == bottom_controls_shown_ratio &&
         other.selection == selection &&
         other.has_transparent_background == has_transparent_background &&
#endif
         other.size == size;
}

bool RenderWidgetSurfaceProperties::operator!=(
    const RenderWidgetSurfaceProperties& other) const {
  return !(*this == other);
}

std::string RenderWidgetSurfaceProperties::ToDiffString(
    const RenderWidgetSurfaceProperties& other) const {
  if (*this == other)
    return std::string();

  std::ostringstream stream;
  stream << "RenderWidgetSurfaceProperties(";
  uint32_t changed_properties = 0;
  if (size != other.size) {
    stream << "size(this: " << size.ToString()
           << ", other: " << other.size.ToString() << ")";
    ++changed_properties;
  }

  if (device_scale_factor != other.device_scale_factor) {
    if (changed_properties > 0)
      stream << ", ";
    stream << "device_scale_factor(this: " << device_scale_factor
           << ", other: " << other.device_scale_factor << ")";
    ++changed_properties;
  }

  if (top_controls_height != other.top_controls_height) {
    if (changed_properties > 0)
      stream << ", ";
    stream << "top_controls_height(this: " << top_controls_height
           << ", other: " << other.top_controls_height << ")";
    ++changed_properties;
  }

  if (top_controls_shown_ratio != other.top_controls_shown_ratio) {
    if (changed_properties > 0)
      stream << ", ";
    stream << "top_controls_shown_ratio(this: " << top_controls_shown_ratio
           << ", other: " << other.top_controls_shown_ratio << ")";
    ++changed_properties;
  }

#ifdef OS_ANDROID

  if (bottom_controls_height != other.bottom_controls_height) {
    if (changed_properties > 0)
      stream << ", ";
    stream << "bottom_controls_height(this: " << bottom_controls_height
           << ", other: " << other.bottom_controls_height << ")";
    ++changed_properties;
  }

  if (bottom_controls_shown_ratio != other.bottom_controls_shown_ratio) {
    if (changed_properties > 0)
      stream << ", ";
    stream << "bottom_controls_shown_ratio(this: "
           << bottom_controls_shown_ratio
           << ", other: " << other.bottom_controls_shown_ratio << ")";
    ++changed_properties;
  }

  if (selection != other.selection) {
    if (changed_properties > 0)
      stream << ", ";
    stream << "selection(this: " << selection.ToString()
           << ", other: " << other.selection.ToString() << ")";
    ++changed_properties;
  }

  if (has_transparent_background != other.has_transparent_background) {
    if (changed_properties > 0)
      stream << ", ";
    stream << "has_transparent_background(this: " << has_transparent_background
           << ", other: " << other.has_transparent_background << ")";
    ++changed_properties;
  }

#endif

  stream << ")";

  return stream.str();
}

}  // namespace content
