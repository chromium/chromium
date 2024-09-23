// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/android/drawer_layout_handler.h"

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "services/accessibility/android/accessibility_info_data_wrapper.h"
#include "services/accessibility/android/android_accessibility_util.h"
#include "services/accessibility/android/ax_tree_source_android.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom-forward.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"

namespace {

constexpr char kDrawerLayoutClassNameAndroidX[] =
    "androidx.drawerlayout.widget.DrawerLayout";
constexpr char kDrawerLayoutClassNameLegacy[] =
    "android.support.v4.widget.DrawerLayout";

bool IsDrawerLayout(ax::android::mojom::AccessibilityNodeInfoData* node) {
  if (!node || !node->string_properties) {
    return false;
  }

  auto it = node->string_properties->find(
      ax::android::mojom::AccessibilityStringProperty::CLASS_NAME);
  if (it == node->string_properties->end()) {
    return false;
  }

  return it->second == kDrawerLayoutClassNameAndroidX ||
         it->second == kDrawerLayoutClassNameLegacy;
}

}  // namespace

namespace ax::android {

// static
std::optional<std::pair<int32_t, std::unique_ptr<DrawerLayoutHandler>>>
DrawerLayoutHandler::CreateIfNecessary(
    AXTreeSourceAndroid* tree_source,
    const mojom::AccessibilityEventData& event_data) {
  if (event_data.event_type !=
      mojom::AccessibilityEventType::WINDOW_STATE_CHANGED) {
    return std::nullopt;
  }

  AccessibilityInfoDataWrapper* source_node =
      tree_source->GetFromId(event_data.source_id);
  if (!source_node || !IsDrawerLayout(source_node->GetNode())) {
    return std::nullopt;
  }

  // Find a node with accessibility importance. That is a menu node opened now.
  // Extract the accessibility name of the drawer menu from the event text.
  std::vector<raw_ptr<AccessibilityInfoDataWrapper, VectorExperimental>>
      children;
  source_node->GetChildren(&children);
  for (ax::android::AccessibilityInfoDataWrapper* child : children) {
    if (!child->IsNode() || !child->IsVisibleToUser() ||
        !GetBooleanProperty(child->GetNode(),
                            mojom::AccessibilityBooleanProperty::IMPORTANCE)) {
      continue;
    }
    return std::make_pair(
        child->GetId(),
        std::make_unique<DrawerLayoutHandler>(
            child->GetId(),
            base::JoinString(
                event_data.event_text.value_or<std::vector<std::string>>({}),
                " ")));
  }
  return std::nullopt;
}

bool DrawerLayoutHandler::PreDispatchEvent(
    AXTreeSourceAndroid* tree_source,
    const mojom::AccessibilityEventData& event_data) {
  return false;
}

void DrawerLayoutHandler::PostSerializeNode(ui::AXNodeData* out_data) const {
  out_data->role = ax::mojom::Role::kMenu;
  if (!name_.empty()) {
    out_data->SetName(name_);
  }
}

bool DrawerLayoutHandler::ShouldDestroy(
    AXTreeSourceAndroid* tree_source) const {
  return tree_source->GetFromId(node_id_) == nullptr;
}

}  // namespace ax::android
