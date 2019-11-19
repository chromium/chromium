// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_LIST_MARKER_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_LIST_MARKER_IMAGE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"

namespace blink {

class Document;

class CORE_EXPORT LayoutNGListMarkerImage final : public LayoutImage {
 public:
  explicit LayoutNGListMarkerImage(Element*);
  static LayoutNGListMarkerImage* CreateAnonymous(Document*);

  bool IsLayoutNGObject() const override { return true; }

  Node* NodeForHitTest() const final;

 private:
  bool IsOfType(LayoutObjectType) const override;

  void ComputeIntrinsicSizingInfoByDefaultSize(IntrinsicSizingInfo&) const;
  void ComputeIntrinsicSizingInfo(IntrinsicSizingInfo&) const final;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutNGListMarkerImage,
                                IsLayoutNGListMarkerImage());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_LIST_MARKER_IMAGE_H_
