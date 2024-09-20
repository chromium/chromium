// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/android/pane_title_handler.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "services/accessibility/android/accessibility_info_data_wrapper.h"
#include "services/accessibility/android/accessibility_node_info_data_wrapper.h"
#include "services/accessibility/android/accessibility_window_info_data_wrapper.h"
#include "services/accessibility/android/android_accessibility_util.h"
#include "services/accessibility/android/ax_tree_source_android.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom.h"
#include "services/accessibility/android/test/android_accessibility_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"

namespace ax::android {

using AXBooleanProperty = mojom::AccessibilityBooleanProperty;
using AXEventData = mojom::AccessibilityEventData;
using AXEventType = mojom::AccessibilityEventType;
using AXEventIntListProperty = mojom::AccessibilityEventIntListProperty;
using AXIntListProperty = mojom::AccessibilityIntListProperty;
using AXNodeInfoData = mojom::AccessibilityNodeInfoData;
using AXStringProperty = mojom::AccessibilityStringProperty;
using AXWindowInfoData = mojom::AccessibilityWindowInfoData;

class PaneTitleHandlerTest : public testing::Test,
                             public AXTreeSourceAndroid::Delegate {
 public:
  class TestSerializationDelegate
      : public AXTreeSourceAndroid::SerializationDelegate {
    // AXTreeSourceAndroid::SerializationDelegate overrides.
    void PopulateBounds(const AccessibilityInfoDataWrapper& node,
                        ui::AXNodeData& out_data) const override {}
  };

  class TestAXTreeSourceAndroid : public AXTreeSourceAndroid {
   public:
    explicit TestAXTreeSourceAndroid(AXTreeSourceAndroid::Delegate* delegate)
        : AXTreeSourceAndroid(delegate,
                              std::make_unique<TestSerializationDelegate>(),
                              /*window=*/nullptr) {}

    // AXTreeSourceAndroid overrides.
    AccessibilityInfoDataWrapper* GetRoot() const override {
      return root_id_ ? GetFromId(*root_id_) : nullptr;
    }
    AccessibilityInfoDataWrapper* GetFromId(int32_t id) const override {
      auto itr = wrapper_map_.find(id);
      if (itr == wrapper_map_.end()) {
        return AXTreeSourceAndroid::GetFromId(id);
      }
      return itr->second.get();
    }

    void SetId(std::unique_ptr<AccessibilityInfoDataWrapper>&& wrapper) {
      wrapper_map_[wrapper->GetId()] = std::move(wrapper);
    }

    void SetRoot(int32_t id) { root_id_ = id; }

   private:
    std::map<int32_t, std::unique_ptr<AccessibilityInfoDataWrapper>>
        wrapper_map_;
    std::optional<int32_t> root_id_;
  };

  PaneTitleHandlerTest() : tree_source_(new TestAXTreeSourceAndroid(this)) {}

  void SetNodeIdToTree(mojom::AccessibilityNodeInfoData* wrapper) {
    tree_source_->SetId(std::make_unique<AccessibilityNodeInfoDataWrapper>(
        tree_source(), wrapper));
  }

  void SetWindowIdToTree(mojom::AccessibilityWindowInfoData* wrapper) {
    tree_source_->SetId(std::make_unique<AccessibilityWindowInfoDataWrapper>(
        tree_source(), wrapper));
  }

  // AXTreeSourceAndroid::Delegate overrides.
  bool UseFullFocusMode() const override { return true; }
  void OnAction(const ui::AXActionData& data) const override {}

  AXTreeSourceAndroid* tree_source() { return tree_source_.get(); }

  mojom::AccessibilityEventDataPtr CreateEventWithPaneTitle() {
    auto event = AXEventData::New();
    event->source_id = 1;
    event->task_id = 1;
    event->event_type = AXEventType::WINDOW_STATE_CHANGED;
    SetProperty(event->int_list_properties,
                AXEventIntListProperty::CONTENT_CHANGE_TYPES,
                std::vector<int>({static_cast<int32_t>(
                    mojom::ContentChangeType::PANE_APPEARED)}));

    event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
    event->window_data->push_back(AXWindowInfoData::New());
    AXWindowInfoData* root_window = event->window_data->back().get();
    root_window->window_id = 100;
    root_window->root_node_id = 10;
    SetWindowIdToTree(root_window);
    tree_source_->SetRoot(root_window->window_id);

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* root = event->node_data.back().get();
    root->id = 10;
    SetNodeIdToTree(root);
    SetProperty(root, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({1}));

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* node = event->node_data.back().get();
    node->id = 1;
    SetNodeIdToTree(node);
    SetProperty(node, AXBooleanProperty::VISIBLE_TO_USER, true);
    SetProperty(node, AXStringProperty::PANE_TITLE, "test window pane");

    return event;
  }

 private:
  const std::unique_ptr<TestAXTreeSourceAndroid> tree_source_;
};

TEST_F(PaneTitleHandlerTest, CreateAndEvents) {
  auto event_data = CreateEventWithPaneTitle();

  auto create_result =
      PaneTitleHandler::CreateIfNecessary(tree_source(), *event_data);

  ASSERT_TRUE(create_result.has_value());
  ASSERT_EQ(100, create_result.value().first);

  PaneTitleHandler& handler = *create_result->second;

  // On the first event, live region is created but the name is empty.

  handler.PreDispatchEvent(tree_source(), *event_data);

  std::vector<raw_ptr<AccessibilityInfoDataWrapper, VectorExperimental>>
      children;
  tree_source()->GetRoot()->GetChildren(&children);
  ASSERT_EQ(2U, children.size());
  ASSERT_EQ(10, children.at(0)->GetId());

  ui::AXNodeData data;
  children.at(1)->Serialize(&data);
  int32_t virtual_node_id = data.id;
  std::string val;
  ASSERT_TRUE(data.HasStringAttribute(ax::mojom::StringAttribute::kName));
  val = data.GetStringAttribute(ax::mojom::StringAttribute::kName);
  ASSERT_EQ("", val);

  ASSERT_TRUE(data.HasStringAttribute(ax::mojom::StringAttribute::kLiveStatus));
  val = data.GetStringAttribute(ax::mojom::StringAttribute::kLiveStatus);
  ASSERT_EQ("polite", val);

  ASSERT_TRUE(data.HasStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus));
  val =
      data.GetStringAttribute(ax::mojom::StringAttribute::kContainerLiveStatus);
  ASSERT_EQ("polite", val);

  ASSERT_FALSE(handler.ShouldDestroy(tree_source()));

  // On the second event, the same node exists, and the name is also populated.

  handler.PreDispatchEvent(tree_source(), *event_data);

  children.clear();
  tree_source()->GetRoot()->GetChildren(&children);
  ASSERT_EQ(2U, children.size());
  ASSERT_EQ(10, children.at(0)->GetId());

  data = ui::AXNodeData();
  children.at(1)->Serialize(&data);
  ASSERT_EQ(virtual_node_id, data.id);
  ASSERT_TRUE(data.HasStringAttribute(ax::mojom::StringAttribute::kName));
  val = data.GetStringAttribute(ax::mojom::StringAttribute::kName);
  ASSERT_EQ("test window pane", val);

  ASSERT_TRUE(data.HasStringAttribute(ax::mojom::StringAttribute::kLiveStatus));
  val = data.GetStringAttribute(ax::mojom::StringAttribute::kLiveStatus);
  ASSERT_EQ("polite", val);

  ASSERT_TRUE(data.HasStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus));
  val =
      data.GetStringAttribute(ax::mojom::StringAttribute::kContainerLiveStatus);
  ASSERT_EQ("polite", val);

  ASSERT_FALSE(handler.ShouldDestroy(tree_source()));

  // Changes the pane title of the source node.
  // Serialized name value of the virtual node should be updated.

  AXNodeInfoData* node =
      event_data->node_data.back().get();  // This is a node with pane title.
  SetProperty(node, AXStringProperty::PANE_TITLE, "updated title");

  handler.PreDispatchEvent(tree_source(), *event_data);

  children.clear();
  tree_source()->GetRoot()->GetChildren(&children);
  ASSERT_EQ(2U, children.size());

  data = ui::AXNodeData();
  children.at(1)->Serialize(&data);
  ASSERT_EQ(virtual_node_id, data.id);
  ASSERT_TRUE(data.HasStringAttribute(ax::mojom::StringAttribute::kName));
  val = data.GetStringAttribute(ax::mojom::StringAttribute::kName);
  ASSERT_EQ("updated title", val);

  ASSERT_FALSE(handler.ShouldDestroy(tree_source()));
}

TEST_F(PaneTitleHandlerTest, CreateAndDestroy) {
  auto event_data = CreateEventWithPaneTitle();

  auto create_result =
      PaneTitleHandler::CreateIfNecessary(tree_source(), *event_data);

  ASSERT_TRUE(create_result.has_value());
  ASSERT_EQ(100, create_result.value().first);

  PaneTitleHandler& handler = *create_result->second;

  ASSERT_FALSE(handler.ShouldDestroy(tree_source()));

  AXNodeInfoData* node = event_data->node_data.back().get();
  SetProperty(node, AXStringProperty::PANE_TITLE, base::EmptyString());

  ASSERT_TRUE(handler.ShouldDestroy(tree_source()));
}

TEST_F(PaneTitleHandlerTest, NoCreationWithoutPaneTitle) {
  auto event_data = CreateEventWithPaneTitle();

  // Remove pant title, it should not create a handler.
  AXNodeInfoData* node = event_data->node_data.back().get();
  SetProperty(node, AXStringProperty::PANE_TITLE, base::EmptyString());

  auto create_result =
      PaneTitleHandler::CreateIfNecessary(tree_source(), *event_data);

  ASSERT_FALSE(create_result.has_value());
}

TEST_F(PaneTitleHandlerTest, NoCreationUnrelatedEventType) {
  auto event_data = CreateEventWithPaneTitle();

  // Remove pant title, it should not create a handler.
  event_data->event_type = AXEventType::WINDOW_CONTENT_CHANGED;
  SetProperty(event_data->int_list_properties,
              AXEventIntListProperty::CONTENT_CHANGE_TYPES, std::vector<int>());

  auto create_result =
      PaneTitleHandler::CreateIfNecessary(tree_source(), *event_data);

  ASSERT_FALSE(create_result.has_value());
}

}  // namespace ax::android
