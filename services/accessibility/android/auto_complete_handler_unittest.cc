// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/android/auto_complete_handler.h"

#include <map>
#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "services/accessibility/android/accessibility_info_data_wrapper.h"
#include "services/accessibility/android/accessibility_node_info_data_wrapper.h"
#include "services/accessibility/android/accessibility_window_info_data_wrapper.h"
#include "services/accessibility/android/android_accessibility_util.h"
#include "services/accessibility/android/ax_tree_source_android.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom.h"
#include "services/accessibility/android/test/android_accessibility_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/platform/ax_android_constants.h"

namespace ax::android {

using AXBooleanProperty = mojom::AccessibilityBooleanProperty;
using AXCollectionItemInfoData = mojom::AccessibilityCollectionItemInfoData;
using AXEventData = mojom::AccessibilityEventData;
using AXEventType = mojom::AccessibilityEventType;
using AXIntListProperty = mojom::AccessibilityIntListProperty;
using AXNodeInfoData = mojom::AccessibilityNodeInfoData;
using AXStringProperty = mojom::AccessibilityStringProperty;
using AXWindowInfoData = mojom::AccessibilityWindowInfoData;
using AXWindowIntProperty = mojom::AccessibilityWindowIntProperty;
using AXWindowIntListProperty = mojom::AccessibilityWindowIntListProperty;

class AutoCompleteHandlerTest : public testing::Test,
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

  AutoCompleteHandlerTest() : tree_source_(new TestAXTreeSourceAndroid(this)) {}

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

  mojom::AccessibilityEventDataPtr CreateEventWithEditables() {
    auto event = AXEventData::New();
    event->task_id = 1;

    event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
    event->window_data->push_back(AXWindowInfoData::New());
    AXWindowInfoData* root_window = event->window_data->back().get();
    root_window->window_id = 100;
    root_window->root_node_id = 10;
    SetWindowIdToTree(root_window);

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* root = event->node_data.back().get();
    root->id = 10;
    root->window_id = 100;
    SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
                std::vector<int>({1, 2}));
    SetProperty(root, AXBooleanProperty::IMPORTANCE, true);
    SetNodeIdToTree(root);

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* editable1 = event->node_data.back().get();
    editable1->id = 1;
    editable1->window_id = 100;
    SetProperty(editable1, AXBooleanProperty::IMPORTANCE, true);
    SetProperty(editable1, AXBooleanProperty::VISIBLE_TO_USER, true);
    SetProperty(editable1, AXBooleanProperty::EDITABLE, true);
    SetNodeIdToTree(editable1);

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* editable2 = event->node_data.back().get();
    editable2->id = 2;
    editable2->window_id = 100;
    SetProperty(editable2, AXBooleanProperty::IMPORTANCE, true);
    SetProperty(editable2, AXBooleanProperty::VISIBLE_TO_USER, true);
    SetProperty(editable2, AXBooleanProperty::EDITABLE, true);
    SetNodeIdToTree(editable2);

    return event;
  }

  void AddSubWindow(mojom::AccessibilityEventDataPtr& event,
                    int32_t window_id,
                    int32_t node_id_offset,
                    size_t num_items) {
    event->window_data->push_back(AXWindowInfoData::New());
    AXWindowInfoData* popup_window = event->window_data->back().get();
    popup_window->window_id = window_id;
    popup_window->root_node_id = node_id_offset;
    SetProperty(event->window_data->at(0).get(),
                AXWindowIntListProperty::CHILD_WINDOW_IDS, {node_id_offset});
    SetWindowIdToTree(popup_window);

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* candidate_list = event->node_data.back().get();
    candidate_list->id = node_id_offset;
    candidate_list->window_id = window_id;
    SetProperty(candidate_list, AXBooleanProperty::IMPORTANCE, true);
    SetProperty(candidate_list, AXBooleanProperty::VISIBLE_TO_USER, true);
    SetNodeIdToTree(candidate_list);

    std::vector<int> child_ids;
    for (size_t i = 0; i < num_items; i++) {
      event->node_data.push_back(AXNodeInfoData::New());
      AXNodeInfoData* item = event->node_data.back().get();
      item->id = node_id_offset + 1 + i;
      item->window_id = window_id;
      SetProperty(item, AXBooleanProperty::IMPORTANCE, true);
      SetProperty(item, AXBooleanProperty::VISIBLE_TO_USER, true);
      item->collection_item_info = AXCollectionItemInfoData::New();
      child_ids.push_back(item->id);
      SetNodeIdToTree(item);
    }

    SetProperty(candidate_list, AXIntListProperty::CHILD_NODE_IDS,
                std::move(child_ids));
  }

 private:
  const std::unique_ptr<TestAXTreeSourceAndroid> tree_source_;
};

TEST_F(AutoCompleteHandlerTest, Create) {
  auto event_data = CreateEventWithEditables();
  event_data->event_type = AXEventType::WINDOW_CONTENT_CHANGED;
  event_data->source_id = 10;  // root

  // No autocomplete class name. No modifier should be created.
  auto create_result =
      AutoCompleteHandler::CreateIfNecessary(tree_source(), *event_data);
  ASSERT_TRUE(create_result.empty());

  // Set one editable as autocomplete.
  SetProperty(event_data->node_data[1].get(), AXStringProperty::CLASS_NAME,
              ui::kAXAutoCompleteTextViewClassname);
  create_result =
      AutoCompleteHandler::CreateIfNecessary(tree_source(), *event_data);
  ASSERT_EQ(1U, create_result.size());
  ASSERT_EQ(1, create_result[0].first);

  // Set another editable as autocomplete as well.
  SetProperty(event_data->node_data[2].get(), AXStringProperty::CLASS_NAME,
              ui::kAXMultiAutoCompleteTextViewClassname);
  create_result =
      AutoCompleteHandler::CreateIfNecessary(tree_source(), *event_data);
  ASSERT_EQ(2U, create_result.size());

  // Check both IDs are included.
  ASSERT_TRUE(base::Contains(create_result, 1,
                             &AutoCompleteHandler::IdAndHandler::first));
  ASSERT_TRUE(base::Contains(create_result, 2,
                             &AutoCompleteHandler::IdAndHandler::first));
}

TEST_F(AutoCompleteHandlerTest, PreEventAndPostSerialize) {
  // Similar to AXTreeSourceAndroidTest.AutoComplete, but handle multiple
  // editable and more patterns.
  auto event_data = CreateEventWithEditables();
  event_data->event_type = AXEventType::WINDOW_CONTENT_CHANGED;
  event_data->source_id = 10;  // root

  SetProperty(event_data->node_data[1].get(), AXStringProperty::CLASS_NAME,
              ui::kAXAutoCompleteTextViewClassname);
  SetProperty(event_data->node_data[2].get(), AXStringProperty::CLASS_NAME,
              ui::kAXMultiAutoCompleteTextViewClassname);

  auto create_result =
      AutoCompleteHandler::CreateIfNecessary(tree_source(), *event_data);
  ASSERT_EQ(2U, create_result.size());

  auto editable1_handler = base::ranges::find(
      create_result, 1, &AutoCompleteHandler::IdAndHandler::first);
  auto editable2_handler = base::ranges::find(
      create_result, 2, &AutoCompleteHandler::IdAndHandler::first);
  ASSERT_NE(editable1_handler, create_result.end());
  ASSERT_NE(editable2_handler, create_result.end());

  ui::AXNodeData data;
  data.role = ax::mojom::Role::kTextField;  // Should be populated by default.
  editable1_handler->second->PostSerializeNode(&data);
  ASSERT_EQ("list",
            data.GetStringAttribute(ax::mojom::StringAttribute::kAutoComplete));
  ASSERT_TRUE(data.HasState(ax::mojom::State::kCollapsed));
  ASSERT_FALSE(data.HasState(ax::mojom::State::kExpanded));

  // Add popup window and anchor the first editable.
  event_data->event_type = AXEventType::WINDOWS_CHANGED;
  AddSubWindow(event_data, /*window_id*/ 200, /*node_id_offset*/ 20,
               /*num_items*/ 2);
  SetProperty(event_data->window_data->at(1).get(),
              AXWindowIntProperty::ANCHOR_NODE_ID, 1);

  // The first handler requests to dispatch an event, while the second doesn't.
  ASSERT_TRUE(
      editable1_handler->second->PreDispatchEvent(tree_source(), *event_data));
  ASSERT_FALSE(
      editable2_handler->second->PreDispatchEvent(tree_source(), *event_data));

  data = ui::AXNodeData();
  data.role = ax::mojom::Role::kTextField;
  editable1_handler->second->PostSerializeNode(&data);
  ASSERT_FALSE(data.HasState(ax::mojom::State::kCollapsed));
  ASSERT_TRUE(data.HasState(ax::mojom::State::kExpanded));

  data = ui::AXNodeData();
  data.role = ax::mojom::Role::kTextField;
  editable2_handler->second->PostSerializeNode(&data);
  ASSERT_TRUE(data.HasState(ax::mojom::State::kCollapsed));
  ASSERT_FALSE(data.HasState(ax::mojom::State::kExpanded));

  // Select an element.
  event_data->event_type = AXEventType::VIEW_SELECTED;
  SetProperty(event_data->node_data[3].get(), AXBooleanProperty::SELECTED,
              true);
  event_data->source_id = 21;

  ASSERT_TRUE(
      editable1_handler->second->PreDispatchEvent(tree_source(), *event_data));
  ASSERT_FALSE(
      editable2_handler->second->PreDispatchEvent(tree_source(), *event_data));

  data = ui::AXNodeData();
  data.role = ax::mojom::Role::kTextField;
  editable1_handler->second->PostSerializeNode(&data);
  ASSERT_EQ(21,
            data.GetIntAttribute(ax::mojom::IntAttribute::kActivedescendantId));

  data = ui::AXNodeData();
  data.role = ax::mojom::Role::kTextField;
  editable2_handler->second->PostSerializeNode(&data);
  ASSERT_FALSE(
      data.HasIntAttribute(ax::mojom::IntAttribute::kActivedescendantId));

  // Select an element again. It won't update.
  ASSERT_FALSE(
      editable1_handler->second->PreDispatchEvent(tree_source(), *event_data));
  ASSERT_FALSE(
      editable2_handler->second->PreDispatchEvent(tree_source(), *event_data));

  // Select another element.
  SetProperty(event_data->node_data[3].get(), AXBooleanProperty::SELECTED,
              false);
  SetProperty(event_data->node_data[4].get(), AXBooleanProperty::SELECTED,
              true);
  event_data->source_id = 22;

  ASSERT_TRUE(
      editable1_handler->second->PreDispatchEvent(tree_source(), *event_data));
  ASSERT_FALSE(
      editable2_handler->second->PreDispatchEvent(tree_source(), *event_data));

  data = ui::AXNodeData();
  data.role = ax::mojom::Role::kTextField;
  editable1_handler->second->PostSerializeNode(&data);
  ASSERT_EQ(22,
            data.GetIntAttribute(ax::mojom::IntAttribute::kActivedescendantId));

  data = ui::AXNodeData();
  data.role = ax::mojom::Role::kTextField;
  editable2_handler->second->PostSerializeNode(&data);
  ASSERT_FALSE(
      data.HasIntAttribute(ax::mojom::IntAttribute::kActivedescendantId));
}

}  // namespace ax::android
