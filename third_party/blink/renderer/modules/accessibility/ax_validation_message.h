// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_VALIDATION_MESSAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_VALIDATION_MESSAGE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/modules/accessibility/ax_mock_object.h"

namespace blink {

class AXObjectCacheImpl;
class ListedElement;

// The AXValidationMessage is a mock object that exposes an alert for a native
// error message popup for an invalid HTML control, aka a validation message.
// The alert is exposed with a name containing the text of the popup..

class AXValidationMessage final : public AXMockObject {
 public:
  explicit AXValidationMessage(AXObjectCacheImpl&);
  ~AXValidationMessage() override;

 private:
  // AXObject:
  bool CanHaveChildren() const override { return false; }
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;
  AXObject* ComputeParent() const override;
  void GetRelativeBounds(AXObject** out_container,
                         FloatRect& out_bounds_in_container,
                         SkMatrix44& out_container_transform,
                         bool* clips_children) const override;
  const AtomicString& LiveRegionStatus() const override;
  const AtomicString& LiveRegionRelevant() const override;
  bool IsOffScreen() const override;
  bool IsValidationMessage() const override { return true; }
  bool IsVisible() const override;
  String TextAlternative(bool recursive,
                         bool in_aria_labelled_by_traversal,
                         AXObjectSet& visited,
                         ax::mojom::NameFrom&,
                         AXRelatedObjectVector*,
                         NameSources*) const override;
  ax::mojom::Role RoleValue() const override;

  ListedElement* RelatedFormControlIfVisible() const;

  DISALLOW_COPY_AND_ASSIGN(AXValidationMessage);
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXValidationMessage, IsValidationMessage());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_VALIDATION_MESSAGE_H_
