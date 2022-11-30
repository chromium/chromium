// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_CAPTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_CAPTION_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_table_caption.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow_mixin.h"

namespace blink {

class LayoutNGTableInterface;
class NGPhysicalFragment;

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGBlockFlowMixin<LayoutTableCaption>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGMixin<LayoutTableCaption>;

class CORE_EXPORT LayoutNGTableCaption final
    : public LayoutNGBlockFlowMixin<LayoutTableCaption> {
 public:
  explicit LayoutNGTableCaption(Element*);

  void UpdateBlockLayout(bool relayout_children) override;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGTableCaption";
  }

 private:
  // Legacy-only API.
  void InsertedIntoTree() override;
  // Legacy-only API.
  void WillBeRemovedFromTree() override;
  // Legacy-only API.
  void CalculateAndSetMargins(const NGConstraintSpace&,
                              const NGPhysicalFragment&);

  LayoutNGTableInterface* TableInterface() const;
};

// wtf/casting.h helper.
template <>
struct DowncastTraits<LayoutNGTableCaption> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsTableCaption() && object.IsLayoutNGObject();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_CAPTION_H_
