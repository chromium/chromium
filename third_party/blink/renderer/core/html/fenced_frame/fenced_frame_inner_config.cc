// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_inner_config.h"

namespace blink {

FencedFrameInnerConfig::FencedFrameInnerConfig(const String& src)
    : url_attribute_visibility_(AttributeVisibility::kOpaque), url_(src) {}

V8UnionOpaquePropertyOrUSVString* FencedFrameInnerConfig::url() const {
  return Get<Attribute::kURL>();
}

V8UnionOpaquePropertyOrUnsignedLong* FencedFrameInnerConfig::width() const {
  return Get<Attribute::kWidth>();
}

V8UnionOpaquePropertyOrUnsignedLong* FencedFrameInnerConfig::height() const {
  return Get<Attribute::kHeight>();
}

}  // namespace blink
