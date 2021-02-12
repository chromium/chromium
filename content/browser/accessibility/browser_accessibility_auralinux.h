// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_AURALINUX_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_AURALINUX_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/common/content_export.h"

namespace ui {
class AXPlatformNodeAuraLinux;
}

namespace content {

class BrowserAccessibilityAuraLinux : public BrowserAccessibility {
 public:
  BrowserAccessibilityAuraLinux();

  ~BrowserAccessibilityAuraLinux() override;

  CONTENT_EXPORT ui::AXPlatformNodeAuraLinux* GetNode() const;

  // This is used to call UpdateHypertext, when a node needs to be
  // updated for some other reason other than via OnAtomicUpdateFinished.
  void UpdatePlatformAttributes() override;

  // BrowserAccessibility methods.
  void OnDataChanged() override;
  ui::AXPlatformNode* GetAXPlatformNode() const override;
  base::string16 GetText() const override;
  base::string16 GetHypertext() const override;

  gfx::NativeViewAccessible GetNativeViewAccessible() override;

  ui::TextAttributeList ComputeTextAttributes() const override;

 private:
  // Give BrowserAccessibility::Create access to our constructor.
  friend class BrowserAccessibility;

  ui::AXPlatformNodeAuraLinux* node_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityAuraLinux);
};

CONTENT_EXPORT BrowserAccessibilityAuraLinux* ToBrowserAccessibilityAuraLinux(
    BrowserAccessibility* obj);

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_AURALINUX_H_
