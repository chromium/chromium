// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_ANDROID_AUTO_COMPLETE_HANDLER_H_
#define SERVICES_ACCESSIBILITY_ANDROID_AUTO_COMPLETE_HANDLER_H_

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "services/accessibility/android/ax_tree_source_android.h"

namespace ui {
struct AXNodeData;
}

namespace ax::android {

namespace mojom {
class AccessibilityEventData;
}

class AutoCompleteHandler : public AXTreeSourceAndroid::Hook {
 public:
  using IdAndHandler = std::pair<int32_t, std::unique_ptr<AutoCompleteHandler>>;
  static std::vector<IdAndHandler> CreateIfNecessary(
      AXTreeSourceAndroid* tree_source,
      const mojom::AccessibilityEventData& event_data);

  explicit AutoCompleteHandler(const int32_t editable_node_id);

  ~AutoCompleteHandler() override;

  // AXTreeSourceAndroid::Hook overrides:
  bool PreDispatchEvent(
      AXTreeSourceAndroid* tree_source,
      const mojom::AccessibilityEventData& event_data) override;
  void PostSerializeNode(ui::AXNodeData* out_data) const override;
  bool ShouldDestroy(AXTreeSourceAndroid* tree_source) const override;

 private:
  const int32_t anchored_node_id_;
  std::optional<int32_t> suggestion_window_id_;
  std::optional<int32_t> selected_node_id_;
};

}  // namespace ax::android

#endif  // SERVICES_ACCESSIBILITY_ANDROID_AUTO_COMPLETE_HANDLER_H_
