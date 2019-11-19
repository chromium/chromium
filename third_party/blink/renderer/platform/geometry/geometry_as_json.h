// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_GEOMETRY_AS_JSON_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_GEOMETRY_AS_JSON_H_

#include <memory>

#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

template <typename T>
static std::unique_ptr<JSONArray> RectAsJSONArray(const T& rect) {
  auto array = std::make_unique<JSONArray>();
  array->PushDouble(rect.X());
  array->PushDouble(rect.Y());
  array->PushDouble(rect.Width());
  array->PushDouble(rect.Height());
  return array;
}

template <typename T>
std::unique_ptr<JSONArray> PointAsJSONArray(const T& point) {
  auto array = std::make_unique<JSONArray>();
  array->PushDouble(point.X());
  array->PushDouble(point.Y());
  return array;
}

template <typename T>
std::unique_ptr<JSONArray> SizeAsJSONArray(const T& size) {
  auto array = std::make_unique<JSONArray>();
  array->PushDouble(size.Width());
  array->PushDouble(size.Height());
  return array;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_GEOMETRY_AS_JSON_H_
