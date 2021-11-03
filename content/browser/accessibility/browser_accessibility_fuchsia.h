// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_FUCHSIA_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_FUCHSIA_H_

#include <fuchsia/accessibility/semantics/cpp/fidl.h>

#include "content/browser/accessibility/browser_accessibility.h"
#include "content/common/content_export.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_relative_bounds.h"
#include "ui/accessibility/platform/fuchsia/fuchsia_types.h"

namespace content {

// Fuchsia-specific wrapper for a AXPlatformNode. Each
// BrowserAccessibilityFuchsia object is owned by a
// BrowserAccessibilityManagerFuchsia.
class CONTENT_EXPORT BrowserAccessibilityFuchsia : public BrowserAccessibility {
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
  // NOTE: BrowserAccessibilityFuchsia does not have access to the
  // (AXTreeID, AXNodeID) <-> FuchsiaNodeID mapping, this method will NOT fill
  // any of the ID fields in the fuchsia node (container_id, child_ids,
  // node_id).
  fuchsia::accessibility::semantics::Node ToFuchsiaNodeData() const;

  // BrowserAccessibility overrides.
  void OnDataChanged() override;
  void OnLocationChanged() override;

 protected:
  friend class BrowserAccessibility;  // Needs access to our constructor.

 private:
  std::vector<fuchsia::accessibility::semantics::Action> GetFuchsiaActions()
      const;
  fuchsia::accessibility::semantics::Role GetFuchsiaRole() const;
  fuchsia::accessibility::semantics::States GetFuchsiaStates() const;
  fuchsia::accessibility::semantics::Attributes GetFuchsiaAttributes() const;
  fuchsia::ui::gfx::BoundingBox GetFuchsiaLocation() const;
  fuchsia::ui::gfx::mat4 GetFuchsiaTransform() const;
};

BrowserAccessibilityFuchsia* CONTENT_EXPORT
ToBrowserAccessibilityFuchsia(BrowserAccessibility* obj);

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_FUCHSIA_H_
