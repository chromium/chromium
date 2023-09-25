// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_VIEW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_VIEW_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"

namespace blink {

class CORE_EXPORT LayoutNGView : public LayoutView {
 public:
  explicit LayoutNGView(ContainerNode*);
  ~LayoutNGView() override;

  bool IsFragmentationContextRoot() const override;

  void UpdateLayout() final;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGView";
  }

 protected:
  bool IsOfType(LayoutObjectType) const override;

 private:
  AtomicString NamedPageAtIndex(wtf_size_t page_index) const override;
};

template <>
struct DowncastTraits<LayoutNGView> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsLayoutNGView();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_VIEW_H_
