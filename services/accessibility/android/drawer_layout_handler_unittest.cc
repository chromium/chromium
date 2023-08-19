// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/android/drawer_layout_handler.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

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
using AXIntListProperty = mojom::AccessibilityIntListProperty;
using AXNodeInfoData = mojom::AccessibilityNodeInfoData;
using AXStringProperty = mojom::AccessibilityStringProperty;
using AXWindowInfoData = mojom::AccessibilityWindowInfoData;

class DrawerLayoutHandlerTest : public testing::Test,
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
    AccessibilityInfoDataWrapper* GetFromId(int32_t id) const override {
      auto itr = wrapper_map_.find(id);
      if (itr == wrapper_map_.end()) {
        return nullptr;
      }
      return itr->second.get();
    }

    void SetId(std::unique_ptr<AccessibilityInfoDataWrapper>&& wrapper) {
      wrapper_map_[wrapper->GetId()] = std::move(wrapper);
    }

   private:
    std::map<int32_t, std::unique_ptr<AccessibilityInfoDataWrapper>>
        wrapper_map_;
  };

  DrawerLayoutHandlerTest() : tree_source_(new TestAXTreeSourceAndroid(this)) {}

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

  mojom::AccessibilityEventDataPtr CreateEventWithDrawer() {
    auto event = AXEventData::New();
    event->source_id = 10;
    event->task_id = 1;
    event->event_type = AXEventType::WINDOW_STATE_CHANGED;

    event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
    event->window_data->push_back(AXWindowInfoData::New());
    AXWindowInfoData* root_window = event->window_data->back().get();
    root_window->window_id = 100;
    root_window->root_node_id = 10;
    SetWindowIdToTree(root_window);

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* root = event->node_data.back().get();
    root->id = 10;
    SetNodeIdToTree(root);
    SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
                std::vector<int>({1, 2}));
    SetProperty(root, AXBooleanProperty::IMPORTANCE, true);
    SetProperty(root, AXStringProperty::CLASS_NAME,
                "android.support.v4.widget.DrawerLayout");

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* node1 = event->node_data.back().get();
    node1->id = 1;
    SetNodeIdToTree(node1);
    SetProperty(node1, AXBooleanProperty::VISIBLE_TO_USER, true);

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* node2 = event->node_data.back().get();
    node2->id = 2;
    SetNodeIdToTree(node2);
    SetProperty(node2, AXIntListProperty::CHILD_NODE_IDS,
                std::vector<int>({3}));
    SetProperty(node2, AXBooleanProperty::IMPORTANCE, true);
    SetProperty(node2, AXBooleanProperty::VISIBLE_TO_USER, true);

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* node3 = event->node_data.back().get();
    node3->id = 3;
    SetNodeIdToTree(node3);
    SetProperty(node3, AXBooleanProperty::IMPORTANCE, true);
    SetProperty(node3, AXBooleanProperty::VISIBLE_TO_USER, true);
    SetProperty(node3, AXStringProperty::TEXT, "sample string.");

    return event;
  }

 private:
  const std::unique_ptr<TestAXTreeSourceAndroid> tree_source_;
};

TEST_F(DrawerLayoutHandlerTest, CreateAndSerialize) {
  auto event_data = CreateEventWithDrawer();
  event_data->event_text = std::vector<std::string>({"Test", "Navigation"});

  auto create_result =
      DrawerLayoutHandler::CreateIfNecessary(tree_source(), *event_data);

  ASSERT_TRUE(create_result.has_value());
  ASSERT_EQ(2, create_result.value().first);

  ui::AXNodeData data;
  create_result.value().second->PostSerializeNode(&data);

  ASSERT_EQ(ax::mojom::Role::kMenu, data.role);
  ASSERT_EQ("Test Navigation",
            data.GetStringAttribute(ax::mojom::StringAttribute::kName));
}

TEST_F(DrawerLayoutHandlerTest, CreateAndSerializeWithoutText) {
  auto event_data = CreateEventWithDrawer();

  auto create_result =
      DrawerLayoutHandler::CreateIfNecessary(tree_source(), *event_data);

  ASSERT_TRUE(create_result.has_value());
  ASSERT_EQ(2, create_result.value().first);

  ui::AXNodeData data;
  // A valid role must be set prior to calling SetName.
  data.role = ax::mojom::Role::kMenu;
  data.SetName("node description");
  create_result.value().second->PostSerializeNode(&data);

  // Modifier doesn't override the name by an empty string, or change the role.
  ASSERT_EQ(ax::mojom::Role::kMenu, data.role);
  ASSERT_EQ("node description",
            data.GetStringAttribute(ax::mojom::StringAttribute::kName));
}

TEST_F(DrawerLayoutHandlerTest, NoCreation) {
  auto event_data = CreateEventWithDrawer();
  event_data->event_type = AXEventType::WINDOW_CONTENT_CHANGED;

  auto create_result =
      DrawerLayoutHandler::CreateIfNecessary(tree_source(), *event_data);

  ASSERT_FALSE(create_result.has_value());
}

}  // namespace ax::android
