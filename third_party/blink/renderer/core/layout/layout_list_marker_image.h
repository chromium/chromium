// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_LIST_MARKER_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_LIST_MARKER_IMAGE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"

namespace blink {

class Document;

class CORE_EXPORT LayoutListMarkerImage final : public LayoutImage {
 public:
  explicit LayoutListMarkerImage(Element*);
  static LayoutListMarkerImage* CreateAnonymous(Document*);

  bool IsLayoutNGObject() const override {
    return IsLayoutNGObjectForListMarkerImage();
  }
  LayoutSize DefaultSize() const;

 private:
  bool IsOfType(LayoutObjectType) const override;

  void ComputeIntrinsicSizingInfoByDefaultSize(IntrinsicSizingInfo&) const;
  void ComputeIntrinsicSizingInfo(IntrinsicSizingInfo&) const final;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutListMarkerImage, IsListMarkerImage());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_LIST_MARKER_IMAGE_H_
