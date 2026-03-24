// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_IMAGE_REPLACEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_IMAGE_REPLACEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"

namespace blink {

class HTMLImageElement;

// Used by HTMLImageElement when replacing the image with remote content. Allows
// for the original image to continue being painted until the remote content (in
// an iframe) is ready to be shown.
class CORE_EXPORT LayoutImageReplacement final : public LayoutImage {
 public:
  explicit LayoutImageReplacement(HTMLImageElement*);
  ~LayoutImageReplacement() override;
  void Trace(Visitor*) const override;

  LayoutObject* FirstChild() const {
    NOT_DESTROYED();
    DCHECK_EQ(Children(), VirtualChildren());
    return Children()->FirstChild();
  }
  LayoutObject* LastChild() const {
    NOT_DESTROYED();
    DCHECK_EQ(Children(), VirtualChildren());
    return Children()->LastChild();
  }

  // Use FirstChild or LastChild instead.
  void SlowFirstChild() const = delete;
  void SlowLastChild() const = delete;

  const LayoutObjectChildList* Children() const {
    NOT_DESTROYED();
    return &children_;
  }
  LayoutObjectChildList* Children() {
    NOT_DESTROYED();
    return &children_;
  }

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutImageReplacement";
  }

  bool IsLayoutImageReplacement() const final {
    NOT_DESTROYED();
    return true;
  }

 private:
  LayoutObjectChildList* VirtualChildren() final {
    NOT_DESTROYED();
    return Children();
  }
  const LayoutObjectChildList* VirtualChildren() const final {
    NOT_DESTROYED();
    return Children();
  }
  bool CanHaveChildren() const final {
    NOT_DESTROYED();
    return true;
  }
  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const final;

  void PaintReplaced(const PaintInfo&,
                     const PhysicalOffset& paint_offset) const override;

  LayoutObjectChildList children_;
};

template <>
struct DowncastTraits<LayoutImageReplacement> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsLayoutImageReplacement();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_IMAGE_REPLACEMENT_H_
