// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_MEDIA_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_MEDIA_ELEMENT_H_

#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_layout_object.h"

namespace blink {

class AXObjectCacheImpl;

class AccessibilityMediaElement : public AXLayoutObject {
 public:
  static AXObject* Create(LayoutObject*, AXObjectCacheImpl&);

  AccessibilityMediaElement(LayoutObject*, AXObjectCacheImpl&);

  AccessibilityMediaElement(const AccessibilityMediaElement&) = delete;
  AccessibilityMediaElement& operator=(const AccessibilityMediaElement&) =
      delete;

  ~AccessibilityMediaElement() override = default;

  // AXLayoutObject overrides.
  String TextAlternative(bool recursive,
                         const AXObject* aria_label_or_description_root,
                         AXObjectSet& visited,
                         ax::mojom::NameFrom&,
                         AXRelatedObjectVector*,
                         NameSources*) const override;

  // AXNodeObject overrides.
  bool CanHaveChildren() const override;
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;
  AXRestriction Restriction() const override;

 protected:
  bool IsUnplayable() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_MEDIA_ELEMENT_H_
