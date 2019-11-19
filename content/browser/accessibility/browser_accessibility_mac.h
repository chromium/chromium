// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MAC_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MAC_H_

#include <map>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/common/content_export.h"

@class BrowserAccessibilityCocoa;

namespace content {

#if __OBJC__
CONTENT_EXPORT const BrowserAccessibilityCocoa* ToBrowserAccessibilityCocoa(
    const BrowserAccessibility* obj);
CONTENT_EXPORT BrowserAccessibilityCocoa* ToBrowserAccessibilityCocoa(
    BrowserAccessibility* obj);
#endif

class BrowserAccessibilityMac : public BrowserAccessibility {
 public:
  // BrowserAccessibility overrides.
  void NativeReleaseReference() override;
  bool IsNative() const override;
  void OnDataChanged() override;
  uint32_t PlatformChildCount() const override;
  BrowserAccessibility* PlatformGetChild(uint32_t child_index) const override;

  BrowserAccessibility* PlatformGetFirstChild() const override;
  BrowserAccessibility* PlatformGetLastChild() const override;
  BrowserAccessibility* PlatformGetNextSibling() const override;
  BrowserAccessibility* PlatformGetPreviousSibling() const override;

  // The BrowserAccessibilityCocoa associated with us.
  BrowserAccessibilityCocoa* native_view() const {
    return browser_accessibility_cocoa_;
  }

 private:
  // This gives BrowserAccessibility::Create access to the class constructor.
  friend class BrowserAccessibility;

  BrowserAccessibilityMac();

  // Allows access to the BrowserAccessibilityCocoa which wraps this.
  // BrowserAccessibility.
  // We own this object until our manager calls ReleaseReference;
  // thereafter, the cocoa object owns us.
  BrowserAccessibilityCocoa* browser_accessibility_cocoa_;
  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MAC_H_
