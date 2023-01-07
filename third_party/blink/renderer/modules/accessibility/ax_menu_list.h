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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_MENU_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_MENU_LIST_H_

#include "third_party/blink/renderer/modules/accessibility/ax_layout_object.h"

namespace blink {

class AXObjectCacheImpl;

class AXMenuList final : public AXLayoutObject {
 public:
  AXMenuList(LayoutObject*, AXObjectCacheImpl&);

  AXMenuList(const AXMenuList&) = delete;
  AXMenuList& operator=(const AXMenuList&) = delete;

  AccessibilityExpanded IsExpanded() const final;
  bool OnNativeClickAction() override;
  void SetNeedsToUpdateChildren() const override;
  void ClearChildren() const override;
  void Detach() override;

  void DidUpdateActiveOption();
  void DidShowPopup();
  void DidHidePopup();

  AXObject* GetOrCreateMockPopupChild();

 private:
  friend class AXMenuListOption;

  bool IsMenuList() const override { return true; }
  ax::mojom::blink::Role NativeRoleIgnoringAria() const final;

  void AddChildren() override;

  bool IsCollapsed() const;
};

template <>
struct DowncastTraits<AXMenuList> {
  static bool AllowFrom(const AXObject& object) { return object.IsMenuList(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_MENU_LIST_H_
