// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_ANDROID_DRAWER_LAYOUT_HANDLER_H_
#define SERVICES_ACCESSIBILITY_ANDROID_DRAWER_LAYOUT_HANDLER_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "services/accessibility/android/ax_tree_source_android.h"

namespace ui {
struct AXNodeData;
}

namespace ax::android {

namespace mojom {
class AccessibilityEventData;
}

class DrawerLayoutHandler : public AXTreeSourceAndroid::Hook {
 public:
  static std::optional<std::pair<int32_t, std::unique_ptr<DrawerLayoutHandler>>>
  CreateIfNecessary(AXTreeSourceAndroid* tree_source,
                    const mojom::AccessibilityEventData& event_data);

  explicit DrawerLayoutHandler(const int32_t node_id, const std::string& name)
      : node_id_(node_id), name_(name) {}

  // AXTreeSourceAndroid::Hook overrides:
  bool PreDispatchEvent(
      AXTreeSourceAndroid* tree_source,
      const mojom::AccessibilityEventData& event_data) override;
  void PostSerializeNode(ui::AXNodeData* out_data) const override;
  bool ShouldDestroy(AXTreeSourceAndroid* tree_source) const override;

 private:
  const int32_t node_id_;
  const std::string name_;
};

}  // namespace ax::android

#endif  // SERVICES_ACCESSIBILITY_ANDROID_DRAWER_LAYOUT_HANDLER_H_
