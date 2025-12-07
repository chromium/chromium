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
FencedFrameConfig* FencedFrameConfig::Create(
    const KURL url,
    const String& shared_storage_context,
    std::optional<KURL> urn_uuid,
    std::optional<gfx::Size> container_size,
    std::optional<gfx::Size> content_size,
    AttributeVisibility url_visibility,
    bool freeze_initial_size) {
  return MakeGarbageCollected<FencedFrameConfig>(
      url, shared_storage_context, urn_uuid, container_size, content_size,
      url_visibility, freeze_initial_size);
}

// static
FencedFrameConfig* FencedFrameConfig::From(
    const FencedFrame::RedactedFencedFrameConfig& config) {
  return MakeGarbageCollected<FencedFrameConfig>(config);
}

FencedFrameConfig::FencedFrameConfig(const String& url)
    : url_(url), url_attribute_visibility_(AttributeVisibility::kTransparent) {}

FencedFrameConfig::FencedFrameConfig(const KURL url,
                                     const String& shared_storage_context,
                                     std::optional<KURL> urn_uuid,
                                     std::optional<gfx::Size> container_size,
                                     std::optional<gfx::Size> content_size,
                                     AttributeVisibility url_visibility,
                                     bool freeze_initial_size)
    : url_(url),
      shared_storage_context_(shared_storage_context),
      url_attribute_visibility_(url_visibility),
      urn_uuid_(urn_uuid),
      container_size_(container_size),
      content_size_(content_size),
      deprecated_should_freeze_initial_size_(freeze_initial_size) {}

FencedFrameConfig::FencedFrameConfig(
    const FencedFrame::RedactedFencedFrameConfig& config) {
  const std::optional<FencedFrame::RedactedFencedFrameProperty<GURL>>&
      mapped_url = config.mapped_url();
  if (!mapped_url) {
    url_attribute_visibility_ = AttributeVisibility::kNull;
  } else if (!mapped_url.value().potentially_opaque_value) {
    url_attribute_visibility_ = AttributeVisibility::kOpaque;
  } else {
    url_attribute_visibility_ = AttributeVisibility::kTransparent;
    url_ = KURL(mapped_url.value().potentially_opaque_value.value());
  }

  const std::optional<GURL>& urn = config.urn_uuid();
  CHECK(blink::IsValidUrnUuidURL(*urn));
  KURL urn_uuid = KURL(*urn);
  urn_uuid_.emplace(std::move(urn_uuid));

  const std::optional<FencedFrame::RedactedFencedFrameProperty<gfx::Size>>&
      container_size = config.container_size();
  if (container_size.has_value() &&
      container_size->potentially_opaque_value.has_value()) {
    container_size_.emplace(*container_size->potentially_opaque_value);
  }

  // `content_size` and `deprecated_should_freeze_initial_size` temporarily need
  // to be treated differently than other fields, because for implementation
  // convenience the fenced frame size is frozen by the embedder. In the long
  // term, it should be frozen by the browser (i.e. neither the embedder's
  // renderer nor the fenced frame's renderer), so that it is secure to
  // compromised renderers.
  const std::optional<FencedFrame::RedactedFencedFrameProperty<gfx::Size>>&
      content_size = config.content_size();
  if (content_size.has_value() &&
      content_size->potentially_opaque_value.has_value()) {
    content_size_.emplace(*content_size->potentially_opaque_value);
  }

  const std::optional<FencedFrame::RedactedFencedFrameProperty<bool>>&
      deprecated_should_freeze_initial_size =
          config.deprecated_should_freeze_initial_size();
  if (deprecated_should_freeze_initial_size.has_value()) {
    deprecated_should_freeze_initial_size_ =
        *deprecated_should_freeze_initial_size->potentially_opaque_value;
  }
}

V8UnionOpaquePropertyOrUSVString* FencedFrameConfig::url() const {
  return Get<Attribute::kURL>();
}

void FencedFrameConfig::setSharedStorageContext(const String& context) {
  shared_storage_context_ =
      (context.length() <= kFencedFrameConfigSharedStorageContextMaxLength)
          ? context
          : context.Substring(0,
                              kFencedFrameConfigSharedStorageContextMaxLength);
}

String FencedFrameConfig::GetSharedStorageContext() const {
  return shared_storage_context_;
}

}  // namespace blink
