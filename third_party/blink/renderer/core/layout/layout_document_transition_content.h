// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_DOCUMENT_TRANSITION_CONTENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_DOCUMENT_TRANSITION_CONTENT_H_

#include "base/memory/scoped_refptr.h"
#include "cc/layers/document_transition_content_layer.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_content_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"

namespace blink {

class CORE_EXPORT LayoutDocumentTransitionContent : public LayoutReplaced {
 public:
  explicit LayoutDocumentTransitionContent(DocumentTransitionContentElement*);
  ~LayoutDocumentTransitionContent() override;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutDocumentTransitionContent";
  }
  void OnIntrinsicSizeUpdated(const LayoutSize& intrinsic_size);

  bool IsDocumentTransitionContent() const override {
    NOT_DESTROYED();
    return true;
  }

 protected:
  PaintLayerType LayerTypeRequired() const override;
  void IntrinsicSizeChanged() override { NOT_DESTROYED(); }
  void PaintReplaced(const PaintInfo&,
                     const PhysicalOffset& paint_offset) const override;

 private:
  scoped_refptr<cc::DocumentTransitionContentLayer> layer_;
};

template <>
struct DowncastTraits<LayoutDocumentTransitionContent> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsDocumentTransitionContent();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_DOCUMENT_TRANSITION_CONTENT_H_
