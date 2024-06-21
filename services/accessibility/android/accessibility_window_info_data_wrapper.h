// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_ANDROID_ACCESSIBILITY_WINDOW_INFO_DATA_WRAPPER_H_
#define SERVICES_ACCESSIBILITY_ANDROID_ACCESSIBILITY_WINDOW_INFO_DATA_WRAPPER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "services/accessibility/android/accessibility_info_data_wrapper.h"
#include "ui/accessibility/ax_node_data.h"

namespace ax::android {

class AXTreeSourceAndroid;

// Wrapper class for an AccessibilityWindowInfoData.
class AccessibilityWindowInfoDataWrapper : public AccessibilityInfoDataWrapper {
 public:
  AccessibilityWindowInfoDataWrapper(
      AXTreeSourceAndroid* tree_source,
      mojom::AccessibilityWindowInfoData* window);

  ~AccessibilityWindowInfoDataWrapper() override;

  AccessibilityWindowInfoDataWrapper(
      const AccessibilityWindowInfoDataWrapper&) = delete;
  AccessibilityWindowInfoDataWrapper& operator=(
      const AccessibilityWindowInfoDataWrapper&) = delete;

  // AccessibilityInfoDataWrapper overrides.
  bool IsNode() const override;
  mojom::AccessibilityNodeInfoData* GetNode() const override;
  mojom::AccessibilityWindowInfoData* GetWindow() const override;
  int32_t GetId() const override;
  const gfx::Rect GetBounds() const override;
  bool IsVisibleToUser() const override;
  bool IsWebNode() const override;
  bool IsIgnored() const override;
  bool IsImportantInAndroid() const override;
  bool IsFocusableInFullFocusMode() const override;
  bool IsAccessibilityFocusableContainer() const override;
  void PopulateAXRole(ui::AXNodeData* out_data) const override;
  void PopulateAXState(ui::AXNodeData* out_data) const override;
  void Serialize(ui::AXNodeData* out_data) const override;
  std::string ComputeAXName(bool do_recursive) const override;
  void GetChildren(
      std::vector<raw_ptr<AccessibilityInfoDataWrapper, VectorExperimental>>*
          children) const override;
  int32_t GetWindowId() const override;

  void AddVirtualChild(int32_t child_id);

 private:
  bool GetProperty(mojom::AccessibilityWindowBooleanProperty prop) const;
  bool GetProperty(mojom::AccessibilityWindowIntProperty prop,
                   int32_t* out_value) const;
  bool HasProperty(mojom::AccessibilityWindowStringProperty prop) const;
  bool GetProperty(mojom::AccessibilityWindowStringProperty prop,
                   std::string* out_value) const;
  bool GetProperty(mojom::AccessibilityWindowIntListProperty prop,
                   std::vector<int32_t>* out_value) const;

  raw_ptr<mojom::AccessibilityWindowInfoData, DanglingUntriaged> window_ptr_ =
      nullptr;

  std::vector<int32_t> virtual_child_ids_ = std::vector<int32_t>(0);
};

}  // namespace ax::android

#endif  // SERVICES_ACCESSIBILITY_ANDROID_ACCESSIBILITY_WINDOW_INFO_DATA_WRAPPER_H_
