/*
 * Copyright (C) 2010 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_MENU_LIST_OPTION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_MENU_LIST_OPTION_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_mock_object.h"

namespace blink {

class AXObjectCacheImpl;

class AXMenuListOption final : public AXMockObject {
 public:
  AXMenuListOption(HTMLOptionElement*, AXObjectCacheImpl&);
  ~AXMenuListOption() override;

  int PosInSet() const override;
  int SetSize() const override;

 private:
  void Trace(blink::Visitor*) override;

  bool IsMenuListOption() const override { return true; }

  Node* GetNode() const override { return element_; }
  void Detach() override;
  bool IsDetached() const override { return !element_; }
  LocalFrameView* DocumentFrameView() const override;
  ax::mojom::Role RoleValue() const override;
  bool CanHaveChildren() const override { return false; }
  AXObject* ComputeParent() const override;

  Element* ActionElement() const override;
  bool IsVisible() const override;
  bool IsOffScreen() const override;
  AccessibilitySelectedState IsSelected() const override;
  bool OnNativeSetSelectedAction(bool) override;

  void GetRelativeBounds(AXObject** out_container,
                         FloatRect& out_bounds_in_container,
                         SkMatrix44& out_container_transform,
                         bool* clips_children = nullptr) const override;
  String TextAlternative(bool recursive,
                         bool in_aria_labelled_by_traversal,
                         AXObjectSet& visited,
                         ax::mojom::NameFrom&,
                         AXRelatedObjectVector*,
                         NameSources*) const override;
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;
  HTMLSelectElement* ParentSelectNode() const;

  Member<HTMLOptionElement> element_;

  DISALLOW_COPY_AND_ASSIGN(AXMenuListOption);
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXMenuListOption, IsMenuListOption());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_MENU_LIST_OPTION_H_
