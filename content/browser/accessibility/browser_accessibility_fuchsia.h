// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_FUCHSIA_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_FUCHSIA_H_

#include <fuchsia/accessibility/semantics/cpp/fidl.h>

#include <vector>

#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager_fuchsia.h"
#include "content/common/content_export.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_relative_bounds.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia.h"
#include "ui/accessibility/platform/fuchsia/ax_platform_node_fuchsia.h"

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
  fuchsia::accessibility::semantics::Node ToFuchsiaNodeData() const;

  // Returns the fuchsia ID of this node's offset container if the offset
  // container ID is valid. Otherwise, returns the ID of this tree's root node.
  uint32_t GetOffsetContainerOrRootNodeID() const;

  // BrowserAccessibility overrides.
  void OnDataChanged() override;
  void OnLocationChanged() override;
  bool AccessibilityPerformAction(const ui::AXActionData& action_data) override;

  // Returns this object's AXUniqueID as a uint32_t.
  uint32_t GetFuchsiaNodeID() const;

 protected:
  friend class BrowserAccessibility;  // Needs access to our constructor.

 private:
  ui::AccessibilityBridgeFuchsia* GetAccessibilityBridge() const;

  void UpdateNode();
  void DeleteNode();
  std::vector<fuchsia::accessibility::semantics::Action> GetFuchsiaActions()
      const;
  fuchsia::accessibility::semantics::Role GetFuchsiaRole() const;
  fuchsia::accessibility::semantics::States GetFuchsiaStates() const;
  fuchsia::accessibility::semantics::Attributes GetFuchsiaAttributes() const;
  fuchsia::ui::gfx::BoundingBox GetFuchsiaLocation() const;
  fuchsia::ui::gfx::mat4 GetFuchsiaTransform() const;
  std::vector<uint32_t> GetFuchsiaChildIDs() const;

  // Returns true if this AXNode has role AXRole::kList.
  // This may need to be expanded later to include more roles, maybe using
  // ui::IsList
  // (https://source.chromium.org/chromium/chromium/src/+/main:ui/accessibility/ax_role_properties.cc;l=399;drc=2c712b0d61f0788c0ed1b05176ae7430e8c705e5;bpv=1;bpt=1).
  bool IsList() const;

  // Returns true if this AXNode has role AXRole::klistItem.
  // This may need to be expanded later to include more roles, maybe using
  // ui::IsListItem
  // (https://source.chromium.org/chromium/chromium/src/+/main:ui/accessibility/ax_role_properties.cc;drc=2c712b0d61f0788c0ed1b05176ae7430e8c705e5;l=413).
  bool IsListElement() const;

  // Fuchsia-specific representation of this node.
  ui::AXPlatformNodeFuchsia* platform_node_;
};

BrowserAccessibilityFuchsia* CONTENT_EXPORT
ToBrowserAccessibilityFuchsia(BrowserAccessibility* obj);

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_FUCHSIA_H_
