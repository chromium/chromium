// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_ANDROID_ACCESSIBILITY_INFO_DATA_WRAPPER_H_
#define SERVICES_ACCESSIBILITY_ANDROID_ACCESSIBILITY_INFO_DATA_WRAPPER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
struct AXNodeData;
}  // namespace ui

namespace ax::android {
class AXTreeSourceAndroid;

// AccessibilityInfoDataWrapper represents a single Android node or window. This
// class can be used by AXTreeSourceAndroid to encapsulate Android-side
// information which maps to a single AXNodeData.
class AccessibilityInfoDataWrapper {
 public:
  explicit AccessibilityInfoDataWrapper(AXTreeSourceAndroid* tree_source);
  virtual ~AccessibilityInfoDataWrapper();

  // True if this AccessibilityInfoDataWrapper represents an Android node, false
  // if it represents an Android window.
  virtual bool IsNode() const = 0;

  // These getters return nullptr if the class doesn't hold the specified type
  // of data.
  virtual mojom::AccessibilityNodeInfoData* GetNode() const = 0;
  virtual mojom::AccessibilityWindowInfoData* GetWindow() const = 0;

  virtual int32_t GetId() const = 0;
  virtual const gfx::Rect GetBounds() const = 0;
  virtual bool IsVisibleToUser() const = 0;
  virtual bool IsWebNode() const = 0;
  virtual bool IsIgnored() const = 0;
  virtual bool IsImportantInAndroid() const = 0;
  virtual bool IsFocusableInFullFocusMode() const = 0;
  virtual bool IsAccessibilityFocusableContainer() const = 0;
  virtual void PopulateAXRole(ui::AXNodeData* out_data) const = 0;
  virtual void PopulateAXState(ui::AXNodeData* out_data) const = 0;
  virtual void Serialize(ui::AXNodeData* out_data) const;
  virtual std::string ComputeAXName(bool do_recursive) const = 0;
  virtual void GetChildren(
      std::vector<raw_ptr<AccessibilityInfoDataWrapper, VectorExperimental>>*
          children) const = 0;
  virtual int32_t GetWindowId() const = 0;

 protected:
  raw_ptr<AXTreeSourceAndroid> tree_source_;
  std::optional<
      std::vector<raw_ptr<AccessibilityInfoDataWrapper, VectorExperimental>>>
      cached_children_;

 private:
  friend class AXTreeSourceAndroid;
};

}  // namespace ax::android

#endif  // SERVICES_ACCESSIBILITY_ANDROID_ACCESSIBILITY_INFO_DATA_WRAPPER_H_
