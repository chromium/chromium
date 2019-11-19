// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_WIN_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_WIN_H_

#include "base/win/atl.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_com_win.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT BrowserAccessibilityWin : public BrowserAccessibility {
 public:
  BrowserAccessibilityWin();
  ~BrowserAccessibilityWin() override;

  // This is used to call UpdateStep1ComputeWinAttributes, ... above when
  // a node needs to be updated for some other reason other than via
  // OnAtomicUpdateFinished.
  void UpdatePlatformAttributes() override;

  //
  // BrowserAccessibility methods.
  //
  ui::AXPlatformNode* GetAXPlatformNode() const override;
  bool IsNative() const override;
  void OnLocationChanged() override;
  base::string16 GetText() const override;
  base::string16 GetHypertext() const override;

  gfx::NativeViewAccessible GetNativeViewAccessible() override;

  class BrowserAccessibilityComWin* GetCOM() const;

 protected:
  ui::TextAttributeList ComputeTextAttributes() const override;

 private:
  CComObject<BrowserAccessibilityComWin>* browser_accessibility_com_;
  // Give BrowserAccessibility::Create access to our constructor.
  friend class BrowserAccessibility;
  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityWin);
};

CONTENT_EXPORT BrowserAccessibilityWin* ToBrowserAccessibilityWin(
    BrowserAccessibility* obj);

CONTENT_EXPORT const BrowserAccessibilityWin* ToBrowserAccessibilityWin(
    const BrowserAccessibility* obj);

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_WIN_H_
