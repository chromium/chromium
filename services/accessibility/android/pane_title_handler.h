// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_ANDROID_PANE_TITLE_HANDLER_H_
#define SERVICES_ACCESSIBILITY_ANDROID_PANE_TITLE_HANDLER_H_

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

class PaneTitleHandler : public AXTreeSourceAndroid::Hook {
 public:
  static std::optional<std::pair<int32_t, std::unique_ptr<PaneTitleHandler>>>
  CreateIfNecessary(AXTreeSourceAndroid* tree_source,
                    const mojom::AccessibilityEventData& event_data);

  PaneTitleHandler(int32_t virtual_node_id,
                   int32_t pane_node_id,
                   const std::string& name)
      : virtual_node_id_(virtual_node_id),
        pane_node_id_(pane_node_id),
        name_(name) {}

  // AXTreeSourceAndroid::Hook overrides:
  bool PreDispatchEvent(
      AXTreeSourceAndroid* tree_source,
      const mojom::AccessibilityEventData& event_data) override;
  void PostSerializeNode(ui::AXNodeData* out_data) const override;
  bool ShouldDestroy(AXTreeSourceAndroid* tree_source) const override;

 private:
  const int32_t virtual_node_id_;
  const int32_t pane_node_id_;
  std::string name_;

  // We'd like to ensure that ChromeVox announces the content as live region
  // change. ChromeVox makes an announcement when the content changes, not when
  // it is added. Thus, the first serialization creates a node with an empty
  // text, and later serialization updates with a real content.
  bool creation_done_ = false;
};

}  // namespace ax::android

#endif  // SERVICES_ACCESSIBILITY_ANDROID_PANE_TITLE_HANDLER_H_
