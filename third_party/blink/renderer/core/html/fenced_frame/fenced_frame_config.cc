// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_config.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"

namespace blink {

// static
FencedFrameConfig* FencedFrameConfig::Create(const String& url) {
  return MakeGarbageCollected<FencedFrameConfig>(url);
}

// static
FencedFrameConfig* FencedFrameConfig::From(
    const FencedFrame::RedactedFencedFrameConfig& config) {
  return MakeGarbageCollected<FencedFrameConfig>(config);
}

FencedFrameConfig::FencedFrameConfig(const String& url)
    : url_(url), url_attribute_visibility_(AttributeVisibility::kTransparent) {}

FencedFrameConfig::FencedFrameConfig(
    const FencedFrame::RedactedFencedFrameConfig& config) {
  const absl::optional<FencedFrame::RedactedFencedFrameProperty<GURL>>&
      mapped_url = config.mapped_url();
  if (!mapped_url) {
    url_attribute_visibility_ = AttributeVisibility::kNull;
  } else if (!mapped_url.value().potentially_opaque_value) {
    url_attribute_visibility_ = AttributeVisibility::kOpaque;
  } else {
    url_attribute_visibility_ = AttributeVisibility::kTransparent;
    url_ = KURL(mapped_url.value().potentially_opaque_value.value());
  }

  const absl::optional<GURL>& urn = config.urn_uuid();
  CHECK(blink::IsValidUrnUuidURL(*urn));
  KURL urn_uuid = KURL(*urn);
  urn_uuid_.emplace(std::move(urn_uuid));
}

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
