// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_inner_config.h"

namespace blink {

FencedFrameInnerConfig* FencedFrameInnerConfig::Create(const String& url) {
  return MakeGarbageCollected<FencedFrameInnerConfig>(url);
}

FencedFrameInnerConfig::FencedFrameInnerConfig(const String& url)
    : url_(url), url_attribute_visibility_(AttributeVisibility::kTransparent) {}

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
