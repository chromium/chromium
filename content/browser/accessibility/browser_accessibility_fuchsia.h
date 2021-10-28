// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_FUCHSIA_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_FUCHSIA_H_

#include <fuchsia/accessibility/semantics/cpp/fidl.h>

#include "content/browser/accessibility/browser_accessibility.h"
#include "ui/accessibility/ax_relative_bounds.h"

namespace content {

// Fuchsia-specific wrapper for a AXPlatformNode. Each
// BrowserAccessibilityFuchsia object is owned by a
// BrowserAccessibilityManagerFuchsia.
class BrowserAccessibilityFuchsia : public BrowserAccessibility {
 public:
  BrowserAccessibilityFuchsia(BrowserAccessibilityManager* manager,
                              ui::AXNode* node);
  ~BrowserAccessibilityFuchsia() override;

  // Disallow copy and assign.
  BrowserAccessibilityFuchsia(const BrowserAccessibilityFuchsia&) = delete;
  BrowserAccessibilityFuchsia& operator=(const BrowserAccessibilityFuchsia&) =
      delete;

  // Returns the fuchsia representation of the AXNode to which this
  // BrowserAccessibility object refers.
  fuchsia::accessibility::semantics::Node ToFuchsiaNode() const;

  // BrowserAccessibility overrides.
  void OnDataChanged() override;
  void OnLocationChanged() override;

 protected:
  friend class BrowserAccessibility;  // Needs access to our constructor.
};

BrowserAccessibilityFuchsia* ToBrowserAccessibilityFuchsia(
    BrowserAccessibility* obj);

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_FUCHSIA_H_
