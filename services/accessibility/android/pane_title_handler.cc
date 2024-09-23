// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/android/pane_title_handler.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "services/accessibility/android/accessibility_info_data_wrapper.h"
#include "services/accessibility/android/accessibility_node_info_data_wrapper.h"
#include "services/accessibility/android/android_accessibility_util.h"
#include "services/accessibility/android/ax_tree_source_android.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom-forward.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/geometry/rect.h"

namespace ax::android {

namespace {

class PaneTitleProviderNode : public AccessibilityInfoDataWrapper {
 public:
  PaneTitleProviderNode(AXTreeSourceAndroid* tree_source,
                        int32_t id,
                        std::string name)
      : AccessibilityInfoDataWrapper(tree_source), id_(id), name_(name) {}

  PaneTitleProviderNode(const PaneTitleProviderNode&) = delete;
  PaneTitleProviderNode& operator=(const PaneTitleProviderNode&) = delete;

  // AccessibilityInfoDataWrapper overrides.
  bool IsNode() const override { return false; }
  mojom::AccessibilityNodeInfoData* GetNode() const override { return nullptr; }
  mojom::AccessibilityWindowInfoData* GetWindow() const override {
    return nullptr;
  }
  int32_t GetId() const override { return id_; }
  const gfx::Rect GetBounds() const override { return gfx::Rect(0, 0, 1, 1); }
  bool IsVisibleToUser() const override { return false; }
  bool IsWebNode() const override { return false; }
  bool IsIgnored() const override { return false; }
  bool IsImportantInAndroid() const override { return true; }
  bool IsFocusableInFullFocusMode() const override { return false; }
  bool IsAccessibilityFocusableContainer() const override { return false; }
  void PopulateAXRole(ui::AXNodeData* out_data) const override {
    out_data->role = ax::mojom::Role::kGenericContainer;
  }
  void PopulateAXState(ui::AXNodeData* out_data) const override {}
  void Serialize(ui::AXNodeData* out_data) const override {
    AccessibilityInfoDataWrapper::Serialize(out_data);

    out_data->SetName(ComputeAXName(false));

    out_data->AddStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                                 "polite");
    out_data->AddStringAttribute(
        ax::mojom::StringAttribute::kContainerLiveStatus, "polite");

    out_data->AddState(ax::mojom::State::kInvisible);
  }
  std::string ComputeAXName(bool do_recursive) const override { return name_; }
  void GetChildren(
      std::vector<raw_ptr<AccessibilityInfoDataWrapper, VectorExperimental>>*
          children) const override {}
  int32_t GetWindowId() const override {
    DUMP_WILL_BE_NOTREACHED();
    return -1;
  }

 private:
  const int32_t id_;
  const std::string name_;
};

std::optional<std::string> GetPaneTitle(AccessibilityInfoDataWrapper* node) {
  if (!node || !node->GetNode()) {
    return std::nullopt;
  }
  std::string pane_title;
  if (!GetProperty(node->GetNode()->string_properties,
                   mojom::AccessibilityStringProperty::PANE_TITLE,
                   &pane_title) ||
      pane_title.empty()) {
    return std::nullopt;
  }
  return pane_title;
}

}  // namespace

// static
std::optional<std::pair<int32_t, std::unique_ptr<PaneTitleHandler>>>
PaneTitleHandler::CreateIfNecessary(
    AXTreeSourceAndroid* tree_source,
    const mojom::AccessibilityEventData& event_data) {
  // Creates a handler on PANE_APPEARED event, which is a subtype of
  // WINDOW_STATE_CHANGED. pant title attribute may be added before the event,
  // but triggering on event allows us to only need to check the event source,
  // not the entire tree.
  if (event_data.event_type !=
      mojom::AccessibilityEventType::WINDOW_STATE_CHANGED) {
    return std::nullopt;
  }

  if (!event_data.int_list_properties) {
    return std::nullopt;
  }
  const auto& itr = event_data.int_list_properties->find(
      mojom::AccessibilityEventIntListProperty::CONTENT_CHANGE_TYPES);
  if (itr == event_data.int_list_properties->end() ||
      base::ranges::find(
          itr->second,
          static_cast<int32_t>(mojom::ContentChangeType::PANE_APPEARED)) ==
          itr->second.end()) {
    return std::nullopt;
  }

  auto* source_node = tree_source->GetFromId(event_data.source_id);
  std::optional<std::string> pane_title = GetPaneTitle(source_node);
  if (!pane_title) {
    return std::nullopt;
  }

  // hook on the root and the virtual node is added as its child.
  auto* root_node = tree_source->GetRoot();
  CHECK(root_node);

  // Setting a quite large ID here assuming that when normal ids practically
  // never hit this number.
  static int32_t next_virtual_node_id = 1'000'000'000;
  if (tree_source->GetFromId(next_virtual_node_id)) {
    LOG(ERROR) << "Virtual ID Conflict. Not adding a pane title handler.";
    return std::nullopt;
  }

  return std::make_pair(
      root_node->GetId(),
      std::make_unique<PaneTitleHandler>(next_virtual_node_id++,
                                         source_node->GetId(), *pane_title));
}

bool PaneTitleHandler::PreDispatchEvent(
    AXTreeSourceAndroid* tree_source,
    const mojom::AccessibilityEventData& event_data) {
  auto* source_node = tree_source->GetFromId(pane_node_id_);
  std::optional<std::string> pane_title = GetPaneTitle(source_node);
  name_ = pane_title.value_or(base::EmptyString());

  tree_source->SetVirtualNode(
      tree_source->GetRoot()->GetId(),
      std::make_unique<PaneTitleProviderNode>(
          tree_source, virtual_node_id_,
          creation_done_ ? name_ : base::EmptyString()));
  creation_done_ = true;

  return true;
}

void PaneTitleHandler::PostSerializeNode(ui::AXNodeData* out_data) const {}

bool PaneTitleHandler::ShouldDestroy(AXTreeSourceAndroid* tree_source) const {
  auto* pane_node = tree_source->GetFromId(pane_node_id_);
  std::optional<std::string> pane_title = GetPaneTitle(pane_node);
  return !pane_title;
}

}  // namespace ax::android
