// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_ANDROID_ACCESSIBILITY_NODE_INFO_DATA_WRAPPER_H_
#define SERVICES_ACCESSIBILITY_ANDROID_ACCESSIBILITY_NODE_INFO_DATA_WRAPPER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "services/accessibility/android/accessibility_info_data_wrapper.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_node_data.h"

namespace ax::android {

class AXTreeSourceAndroid;

// Wrapper class for an AccessibilityWindowInfoData.
class AccessibilityNodeInfoDataWrapper : public AccessibilityInfoDataWrapper {
 public:
  AccessibilityNodeInfoDataWrapper(AXTreeSourceAndroid* tree_source,
                                   mojom::AccessibilityNodeInfoData* node);

  AccessibilityNodeInfoDataWrapper(const AccessibilityNodeInfoDataWrapper&) =
      delete;
  AccessibilityNodeInfoDataWrapper& operator=(
      const AccessibilityNodeInfoDataWrapper&) = delete;

  ~AccessibilityNodeInfoDataWrapper() override;

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

  mojom::AccessibilityNodeInfoData* node() { return node_ptr_; }

 private:
  bool GetProperty(mojom::AccessibilityBooleanProperty prop) const;
  bool GetProperty(mojom::AccessibilityIntProperty prop,
                   int32_t* out_value) const;
  bool HasProperty(mojom::AccessibilityStringProperty prop) const;
  bool GetProperty(mojom::AccessibilityStringProperty prop,
                   std::string* out_value) const;
  bool GetProperty(mojom::AccessibilityIntListProperty prop,
                   std::vector<int32_t>* out_value) const;
  bool GetProperty(mojom::AccessibilityStringListProperty prop,
                   std::vector<std::string>* out_value) const;

  bool HasStandardAction(mojom::AccessibilityActionType action) const;

  bool HasCoveringSpan(mojom::AccessibilityStringProperty prop,
                       mojom::SpanType span_type) const;

  bool HasText() const;
  bool HasAccessibilityFocusableText() const;

  void ComputeNameFromContents(std::vector<std::string>* names) const;
  void ComputeNameFromContentsInternal(std::vector<std::string>* names) const;

  bool IsClickable() const;
  bool IsLongClickable() const;
  bool IsFocusable() const;

  bool IsScrollableContainer() const;
  bool IsToplevelScrollItem() const;

  bool HasImportantProperty() const;
  bool HasImportantPropertyInternal() const;

  ax::mojom::Role GetChromeRole() const;

  raw_ptr<mojom::AccessibilityNodeInfoData, DanglingUntriaged> node_ptr_ =
      nullptr;

  // Properties which should be checked for recursive text computation.
  // It's not clear whether labeled by should be taken into account here.
  static constexpr mojom::AccessibilityStringProperty text_properties_[3] = {
      mojom::AccessibilityStringProperty::TEXT,
      mojom::AccessibilityStringProperty::CONTENT_DESCRIPTION,
      mojom::AccessibilityStringProperty::STATE_DESCRIPTION};

  // These properties are a cached value so that we can avoid same computation.
  // mutable because once the value is computed it won't change.
  mutable std::optional<bool> has_important_property_cache_;
  mutable std::optional<bool> is_web_node_;
};

}  // namespace ax::android

#endif  // SERVICES_ACCESSIBILITY_ANDROID_ACCESSIBILITY_NODE_INFO_DATA_WRAPPER_H_
