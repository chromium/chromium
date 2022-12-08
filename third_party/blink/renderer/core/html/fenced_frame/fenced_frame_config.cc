// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_config.h"

namespace blink {

FencedFrameConfig* FencedFrameConfig::Create(const String& url) {
  return MakeGarbageCollected<FencedFrameConfig>(url);
}

FencedFrameConfig::FencedFrameConfig(const String& url)
    : url_(url), url_attribute_visibility_(AttributeVisibility::kTransparent) {}

V8UnionOpaquePropertyOrUSVString* FencedFrameConfig::url() const {
  return Get<Attribute::kURL>();
}

V8UnionOpaquePropertyOrUnsignedLong* FencedFrameConfig::width() const {
  return Get<Attribute::kWidth>();
}

V8UnionOpaquePropertyOrUnsignedLong* FencedFrameConfig::height() const {
  return Get<Attribute::kHeight>();
}

}  // namespace blink
