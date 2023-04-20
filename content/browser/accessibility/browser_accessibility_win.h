// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_WIN_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_WIN_H_

#include <vector>

#include "base/memory/raw_ptr_exclusion.h"
#include "base/win/atl.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_com_win.h"
#include "content/common/content_export.h"
#include "ui/accessibility/ax_node.h"

namespace content {

class CONTENT_EXPORT BrowserAccessibilityWin : public BrowserAccessibility {
 public:
  ~BrowserAccessibilityWin() override;
  BrowserAccessibilityWin(const BrowserAccessibilityWin&) = delete;
  BrowserAccessibilityWin& operator=(const BrowserAccessibilityWin&) = delete;

  // This is used to call UpdateStep1ComputeWinAttributes, ... above when
  // a node needs to be updated for some other reason other than via
  // OnAtomicUpdateFinished.
  void UpdatePlatformAttributes() override;

  //
  // BrowserAccessibility overrides.
  //

  bool CanFireEvents() const override;
  ui::AXPlatformNode* GetAXPlatformNode() const override;
  void OnLocationChanged() override;
  std::u16string GetHypertext() const override;

  const std::vector<gfx::NativeViewAccessible> GetUIADirectChildrenInRange(
      ui::AXPlatformNodeDelegate* start,
      ui::AXPlatformNodeDelegate* end) override;

  gfx::NativeViewAccessible GetNativeViewAccessible() override;

  class BrowserAccessibilityComWin* GetCOM() const;

 protected:
  BrowserAccessibilityWin(BrowserAccessibilityManager* manager,
                          ui::AXNode* node);

  ui::TextAttributeList ComputeTextAttributes() const override;

  bool ShouldHideChildrenForUIA() const;

  friend class BrowserAccessibility;  // Needs access to our constructor.

 private:
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION CComObject<BrowserAccessibilityComWin>*
      browser_accessibility_com_;
};

CONTENT_EXPORT BrowserAccessibilityWin* ToBrowserAccessibilityWin(
    BrowserAccessibility* obj);

CONTENT_EXPORT const BrowserAccessibilityWin* ToBrowserAccessibilityWin(
    const BrowserAccessibility* obj);

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_WIN_H_
