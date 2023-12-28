// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LIST_LAYOUT_LIST_MARKER_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LIST_LAYOUT_LIST_MARKER_IMAGE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"

namespace blink {

class Document;

class CORE_EXPORT LayoutListMarkerImage final : public LayoutImage {
 public:
  explicit LayoutListMarkerImage(Element*);
  static LayoutListMarkerImage* CreateAnonymous(Document*);

  bool IsLayoutNGObject() const override {
    NOT_DESTROYED();
    return true;
  }
  gfx::SizeF DefaultSize() const;

 private:
  bool IsListMarkerImage() const final {
    NOT_DESTROYED();
    return true;
  }

  void ComputeIntrinsicSizingInfoByDefaultSize(IntrinsicSizingInfo&) const;
  void ComputeIntrinsicSizingInfo(IntrinsicSizingInfo&) const final;
};

template <>
struct DowncastTraits<LayoutListMarkerImage> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsListMarkerImage();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LIST_LAYOUT_LIST_MARKER_IMAGE_H_
