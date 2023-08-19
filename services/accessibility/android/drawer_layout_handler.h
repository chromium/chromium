// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_ANDROID_DRAWER_LAYOUT_HANDLER_H_
#define SERVICES_ACCESSIBILITY_ANDROID_DRAWER_LAYOUT_HANDLER_H_

#include <memory>
#include <string>
#include <utility>

#include "services/accessibility/android/ax_tree_source_android.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ui {
struct AXNodeData;
}

namespace ax::android {

namespace mojom {
class AccessibilityEventData;
}

class DrawerLayoutHandler : public AXTreeSourceAndroid::Hook {
 public:
  static absl::optional<
      std::pair<int32_t, std::unique_ptr<DrawerLayoutHandler>>>
  CreateIfNecessary(AXTreeSourceAndroid* tree_source,
                    const mojom::AccessibilityEventData& event_data);

  explicit DrawerLayoutHandler(const std::string& name) : name_(name) {}

  // AXTreeSourceAndroid::Hook overrides:
  bool PreDispatchEvent(
      AXTreeSourceAndroid* tree_source,
      const mojom::AccessibilityEventData& event_data) override;
  void PostSerializeNode(ui::AXNodeData* out_data) const override;

 private:
  const std::string name_;
};

}  // namespace ax::android

#endif
