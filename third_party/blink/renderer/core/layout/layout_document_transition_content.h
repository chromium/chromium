// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_DOCUMENT_TRANSITION_CONTENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_DOCUMENT_TRANSITION_CONTENT_H_

#include "base/memory/scoped_refptr.h"
#include "cc/layers/document_transition_content_layer.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_content_element.h"
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

 protected:
  PaintLayerType LayerTypeRequired() const override;

  void PaintReplaced(const PaintInfo&,
                     const PhysicalOffset& paint_offset) const override;

 private:
  bool CanHaveAdditionalCompositingReasons() const override {
    NOT_DESTROYED();
    return true;
  }
  CompositingReasons AdditionalCompositingReasons() const override;

  scoped_refptr<cc::DocumentTransitionContentLayer> layer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_DOCUMENT_TRANSITION_CONTENT_H_
