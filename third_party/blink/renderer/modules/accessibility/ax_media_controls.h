/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_MEDIA_CONTROLS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_MEDIA_CONTROLS_H_

#include "base/macros.h"
#include "third_party/blink/renderer/modules/accessibility/ax_slider.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_element_type.h"

namespace blink {

class AXObjectCacheImpl;

class AccessibilityMediaControl : public AXLayoutObject {
 public:
  static AXObject* Create(LayoutObject*, AXObjectCacheImpl&);
  ~AccessibilityMediaControl() override = default;

  ax::mojom::Role RoleValue() const override;

  String TextAlternative(bool recursive,
                         bool in_aria_labelled_by_traversal,
                         AXObjectSet& visited,
                         ax::mojom::NameFrom&,
                         AXRelatedObjectVector*,
                         NameSources*) const override;
  String Description(ax::mojom::NameFrom,
                     ax::mojom::DescriptionFrom&,
                     AXObjectVector* description_objects) const override;

  bool InternalSetAccessibilityFocusAction() override;
  bool InternalClearAccessibilityFocusAction() override;

 protected:
  AccessibilityMediaControl(LayoutObject*, AXObjectCacheImpl&);
  MediaControlElementType ControlType() const;
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityMediaControl);
};

class AccessibilityMediaTimeline final : public AXSlider {
 public:
  static AXObject* Create(LayoutObject*, AXObjectCacheImpl&);
  ~AccessibilityMediaTimeline() override = default;

  String Description(ax::mojom::NameFrom,
                     ax::mojom::DescriptionFrom&,
                     AXObjectVector* description_objects) const override;

 private:
  AccessibilityMediaTimeline(LayoutObject*, AXObjectCacheImpl&);

  DISALLOW_COPY_AND_ASSIGN(AccessibilityMediaTimeline);
};

class AccessibilityMediaVolumeSlider final : public AXSlider {
 public:
  static AXObject* Create(LayoutObject*, AXObjectCacheImpl&);
  ~AccessibilityMediaVolumeSlider() override = default;

  String Description(ax::mojom::NameFrom,
                     ax::mojom::DescriptionFrom&,
                     AXObjectVector* description_objects) const override;

  bool InternalSetAccessibilityFocusAction() override;
  bool InternalClearAccessibilityFocusAction() override;

 private:
  AccessibilityMediaVolumeSlider(LayoutObject*, AXObjectCacheImpl&);

  DISALLOW_COPY_AND_ASSIGN(AccessibilityMediaVolumeSlider);
};

class AXMediaControlsContainer final : public AccessibilityMediaControl {
 public:
  static AXObject* Create(LayoutObject*, AXObjectCacheImpl&);
  ~AXMediaControlsContainer() override = default;

  ax::mojom::Role RoleValue() const override {
    return ax::mojom::Role::kToolbar;
  }

  String TextAlternative(bool recursive,
                         bool in_aria_labelled_by_traversal,
                         AXObjectSet& visited,
                         ax::mojom::NameFrom&,
                         AXRelatedObjectVector*,
                         NameSources*) const override;
  String Description(ax::mojom::NameFrom,
                     ax::mojom::DescriptionFrom&,
                     AXObjectVector* description_objects) const override;

 private:
  AXMediaControlsContainer(LayoutObject*, AXObjectCacheImpl&);
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;

  DISALLOW_COPY_AND_ASSIGN(AXMediaControlsContainer);
};

class AccessibilityMediaTimeDisplay final : public AccessibilityMediaControl {
 public:
  static AXObject* Create(LayoutObject*, AXObjectCacheImpl&);
  ~AccessibilityMediaTimeDisplay() override = default;

  ax::mojom::Role RoleValue() const override {
    return ax::mojom::Role::kStaticText;
  }

  String StringValue() const override;
  String TextAlternative(bool recursive,
                         bool in_aria_labelled_by_traversal,
                         AXObjectSet& visited,
                         ax::mojom::NameFrom&,
                         AXRelatedObjectVector*,
                         NameSources*) const override;

 private:
  AccessibilityMediaTimeDisplay(LayoutObject*, AXObjectCacheImpl&);
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityMediaTimeDisplay);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_MEDIA_CONTROLS_H_
