// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/automation/automation_internal_custom_bindings.h"

#include "base/test/bind.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/platform/automation/automation_ax_tree_wrapper.h"

namespace extensions {

class AutomationInternalCustomBindingsTest
    : public NativeExtensionBindingsSystemUnittest {
 public:
  void SetUp() override {
    NativeExtensionBindingsSystemUnittest::SetUp();

    // Bootstrap a simple extension with desktop automation permissions.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("testExtension")
            .SetManifestPath({"automation", "desktop"}, true)
            .SetLocation(mojom::ManifestLocation::kComponent)
            .Build();
    RegisterExtension(extension);
    v8::HandleScope handle_scope(isolate());
    v8::Local<v8::Context> context = MainContext();
    ScriptContext* script_context = CreateScriptContext(
        context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
    script_context->set_url(extension->url());
    bindings_system()->UpdateBindingsForContext(script_context);

    auto automation_internal_bindings =
        std::make_unique<AutomationInternalCustomBindings>(script_context,
                                                           bindings_system());
    automation_internal_bindings_ = automation_internal_bindings.get();
    script_context->module_system()->RegisterNativeHandler(
        "automationInternal", std::move(automation_internal_bindings));

    // Validate api access.
    const Feature* automation_api =
        FeatureProvider::GetAPIFeature("automation");
    ASSERT_TRUE(automation_api);
    Feature::Availability availability =
        automation_api->IsAvailableToExtension(extension.get());
    EXPECT_TRUE(availability.is_available()) << availability.message();
  }

  std::map<ui::AXTreeID, std::unique_ptr<ui::AutomationAXTreeWrapper>>&
  GetTreeIDToTreeMap() {
    return automation_internal_bindings_->tree_id_to_tree_wrapper_map_;
  }

  void SendOnAccessibilityEvents(
      const ExtensionMsg_AccessibilityEventBundleParams& event_bundle,
      bool is_active_profile) {
    automation_internal_bindings_->HandleAccessibilityEvents(event_bundle,
                                                             is_active_profile);
  }

  bool CallGetFocusInternal(ui::AutomationAXTreeWrapper* top_wrapper,
                            ui::AutomationAXTreeWrapper** focused_wrapper,
                            ui::AXNode** focused_node) {
    return automation_internal_bindings_->GetFocusInternal(
        top_wrapper, focused_wrapper, focused_node);
  }

  gfx::Rect CallComputeGlobalNodeBounds(ui::AutomationAXTreeWrapper* wrapper,
                                        ui::AXNode* node) {
    return automation_internal_bindings_->ComputeGlobalNodeBounds(wrapper,
                                                                  node);
  }

  std::vector<ui::AXNode*> CallGetRootsOfChildTree(ui::AXNode* node) {
    return automation_internal_bindings_->GetRootsOfChildTree(node);
  }

  void AddAutomationEventCallback(
      base::RepeatingCallback<void(api::automation::EventType)> callback) {
    automation_internal_bindings_->notify_event_for_testing_ =
        std::move(callback);
  }

 private:
  AutomationInternalCustomBindings* automation_internal_bindings_ = nullptr;
};

TEST_F(AutomationInternalCustomBindingsTest, GetDesktop) {
  EXPECT_TRUE(GetTreeIDToTreeMap().empty());

  // A desktop tree.
  ExtensionMsg_AccessibilityEventBundleParams bundle;
  bundle.updates.emplace_back();
  auto& tree_update = bundle.updates.back();
  tree_update.root_id = 1;
  tree_update.nodes.emplace_back();
  auto& node_data = tree_update.nodes.back();
  node_data.role = ax::mojom::Role::kDesktop;
  node_data.id = 1;
  SendOnAccessibilityEvents(bundle, true /* active profile */);

  ASSERT_EQ(1U, GetTreeIDToTreeMap().size());

  ui::AutomationAXTreeWrapper* desktop =
      GetTreeIDToTreeMap().begin()->second.get();
  ASSERT_TRUE(desktop);
  EXPECT_TRUE(desktop->IsDesktopTree());
}

TEST_F(AutomationInternalCustomBindingsTest, GetFocusOneTree) {
  // A desktop tree with focus on a button.
  ExtensionMsg_AccessibilityEventBundleParams bundle;
  bundle.updates.emplace_back();
  auto& tree_update = bundle.updates.back();
  tree_update.has_tree_data = true;
  tree_update.root_id = 1;
  auto& tree_data = tree_update.tree_data;
  tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  tree_data.focus_id = 2;
  tree_update.nodes.emplace_back();
  auto& node_data1 = tree_update.nodes.back();
  node_data1.id = 1;
  node_data1.role = ax::mojom::Role::kDesktop;
  node_data1.child_ids.push_back(2);
  tree_update.nodes.emplace_back();
  auto& node_data2 = tree_update.nodes.back();
  node_data2.id = 2;
  node_data2.role = ax::mojom::Role::kButton;
  bundle.tree_id = tree_data.tree_id;
  SendOnAccessibilityEvents(bundle, true /* active profile */);

  ASSERT_EQ(1U, GetTreeIDToTreeMap().size());

  ui::AutomationAXTreeWrapper* desktop =
      GetTreeIDToTreeMap().begin()->second.get();
  ASSERT_TRUE(desktop);

  ui::AutomationAXTreeWrapper* focused_wrapper = nullptr;
  ui::AXNode* focused_node = nullptr;
  CallGetFocusInternal(desktop, &focused_wrapper, &focused_node);
  ASSERT_TRUE(focused_wrapper);
  ASSERT_TRUE(focused_node);
  EXPECT_EQ(desktop, focused_wrapper);
  EXPECT_EQ(ax::mojom::Role::kButton, focused_node->GetRole());

  // Push an update where we change the focus.
  focused_wrapper = nullptr;
  focused_node = nullptr;
  tree_data.focus_id = 1;
  SendOnAccessibilityEvents(bundle, true /* active profile */);
  CallGetFocusInternal(desktop, &focused_wrapper, &focused_node);
  ASSERT_TRUE(focused_wrapper);
  ASSERT_TRUE(focused_node);
  EXPECT_EQ(desktop, focused_wrapper);
  EXPECT_EQ(ax::mojom::Role::kDesktop, focused_node->GetRole());

  // Push an update where we change the focus to nothing.
  focused_wrapper = nullptr;
  focused_node = nullptr;
  tree_data.focus_id = 100;
  SendOnAccessibilityEvents(bundle, true /* active profile */);
  CallGetFocusInternal(desktop, &focused_wrapper, &focused_node);
  ASSERT_FALSE(focused_wrapper);
  ASSERT_FALSE(focused_node);
}

TEST_F(AutomationInternalCustomBindingsTest,
       GetFocusMultipleTreesChildTreeConstruction) {
  // Three trees each with a button and link.
  std::vector<ExtensionMsg_AccessibilityEventBundleParams> bundles;
  for (int i = 0; i < 3; i++) {
    bundles.emplace_back();
    auto& bundle = bundles.back();
    bundle.updates.emplace_back();
    auto& tree_update = bundle.updates.back();
    tree_update.has_tree_data = true;
    tree_update.root_id = 1;
    auto& tree_data = tree_update.tree_data;

    // This is a point of inconsistency as the mojo representation allows
    // updates from multiple trees.
    tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    bundle.tree_id = tree_data.tree_id;
    tree_data.focus_id = 2;
    tree_update.nodes.emplace_back();
    auto& node_data1 = tree_update.nodes.back();
    node_data1.id = 1;
    node_data1.role = ax::mojom::Role::kRootWebArea;
    node_data1.child_ids.push_back(2);
    node_data1.child_ids.push_back(3);
    tree_update.nodes.emplace_back();
    auto& node_data2 = tree_update.nodes.back();
    node_data2.id = 2;
    node_data2.role = ax::mojom::Role::kButton;
    tree_update.nodes.emplace_back();
    auto& node_data3 = tree_update.nodes.back();
    node_data3.id = 3;
    node_data3.role = ax::mojom::Role::kLink;
  }

  // Link up the trees so that the first is a parent of the other two using
  // child tree id.
  ui::AXTreeID tree_0_id = bundles[0].updates[0].tree_data.tree_id;
  ui::AXTreeID tree_1_id = bundles[1].updates[0].tree_data.tree_id;
  ui::AXTreeID tree_2_id = bundles[2].updates[0].tree_data.tree_id;
  bundles[0].updates[0].nodes[1].AddChildTreeId(tree_1_id);
  bundles[0].updates[0].nodes[2].AddChildTreeId(tree_2_id);

  for (auto& bundle : bundles)
    SendOnAccessibilityEvents(bundle, true /* active profile */);

  ASSERT_EQ(3U, GetTreeIDToTreeMap().size());

  ui::AutomationAXTreeWrapper* wrapper_0 =
      GetTreeIDToTreeMap()[tree_0_id].get();
  ASSERT_TRUE(wrapper_0);
  ui::AutomationAXTreeWrapper* wrapper_1 =
      GetTreeIDToTreeMap()[tree_1_id].get();
  ASSERT_TRUE(wrapper_1);
  ui::AutomationAXTreeWrapper* wrapper_2 =
      GetTreeIDToTreeMap()[tree_2_id].get();
  ASSERT_TRUE(wrapper_2);

  ui::AutomationAXTreeWrapper* focused_wrapper = nullptr;
  ui::AXNode* focused_node = nullptr;
  CallGetFocusInternal(wrapper_0, &focused_wrapper, &focused_node);
  ASSERT_TRUE(focused_wrapper);
  ASSERT_TRUE(focused_node);
  EXPECT_EQ(wrapper_1, focused_wrapper);
  EXPECT_EQ(tree_1_id, focused_node->tree()->GetAXTreeID());
  EXPECT_EQ(ax::mojom::Role::kButton, focused_node->GetRole());

  // Push an update where we change the focus.
  focused_wrapper = nullptr;
  focused_node = nullptr;

  // The link in wrapper 0 which has a child tree id pointing to wrapper 2.
  bundles[0].updates[0].tree_data.focus_id = 3;
  SendOnAccessibilityEvents(bundles[0], true /* active profile */);
  CallGetFocusInternal(wrapper_0, &focused_wrapper, &focused_node);
  ASSERT_TRUE(focused_wrapper);
  ASSERT_TRUE(focused_node);
  EXPECT_EQ(wrapper_2, focused_wrapper);
  EXPECT_EQ(tree_2_id, focused_node->tree()->GetAXTreeID());
  EXPECT_EQ(ax::mojom::Role::kButton, focused_node->GetRole());
}

TEST_F(AutomationInternalCustomBindingsTest,
       GetFocusMultipleTreesAppIdConstruction) {
  // Three trees each with a button and link.
  std::vector<ExtensionMsg_AccessibilityEventBundleParams> bundles;
  for (int i = 0; i < 3; i++) {
    bundles.emplace_back();
    auto& bundle = bundles.back();
    bundle.updates.emplace_back();
    auto& tree_update = bundle.updates.back();
    tree_update.has_tree_data = true;
    tree_update.root_id = 1;
    auto& tree_data = tree_update.tree_data;

    // This is a point of inconsistency as the mojo representation allows
    // updates from ultiple trees.
    tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    bundle.tree_id = tree_data.tree_id;
    tree_data.focus_id = 2;
    tree_update.nodes.emplace_back();
    auto& node_data1 = tree_update.nodes.back();
    node_data1.id = 1;
    node_data1.role = ax::mojom::Role::kRootWebArea;
    node_data1.child_ids.push_back(2);
    node_data1.child_ids.push_back(3);
    tree_update.nodes.emplace_back();
    auto& node_data2 = tree_update.nodes.back();
    node_data2.id = 2;
    node_data2.role = ax::mojom::Role::kButton;
    tree_update.nodes.emplace_back();
    auto& node_data3 = tree_update.nodes.back();
    node_data3.id = 3;
    node_data3.role = ax::mojom::Role::kLink;
  }

  // Link up the trees so that the first is a parent of the other two using app
  // ids.
  ui::AXTreeID tree_0_id = bundles[0].updates[0].tree_data.tree_id;
  ui::AXTreeID tree_1_id = bundles[1].updates[0].tree_data.tree_id;
  ui::AXTreeID tree_2_id = bundles[2].updates[0].tree_data.tree_id;
  auto& wrapper0_button_data = bundles[0].updates[0].nodes[1];
  auto& wrapper0_link_data = bundles[0].updates[0].nodes[2];
  auto& wrapper1_link_data = bundles[1].updates[0].nodes[2];
  auto& wrapper2_button_data = bundles[2].updates[0].nodes[1];

  // This construction requires the hosting and client nodes annotate with the
  // same app id.
  wrapper0_button_data.AddStringAttribute(
      ax::mojom::StringAttribute::kChildTreeNodeAppId, "app1");
  wrapper1_link_data.AddStringAttribute(ax::mojom::StringAttribute::kAppId,
                                        "app1");
  wrapper0_link_data.AddStringAttribute(
      ax::mojom::StringAttribute::kChildTreeNodeAppId, "app2");
  wrapper2_button_data.AddStringAttribute(ax::mojom::StringAttribute::kAppId,
                                          "app2");

  for (auto& bundle : bundles)
    SendOnAccessibilityEvents(bundle, true /* active profile */);

  ASSERT_EQ(3U, GetTreeIDToTreeMap().size());

  ui::AutomationAXTreeWrapper* wrapper_0 =
      GetTreeIDToTreeMap()[tree_0_id].get();
  ASSERT_TRUE(wrapper_0);
  ui::AutomationAXTreeWrapper* wrapper_1 =
      GetTreeIDToTreeMap()[tree_1_id].get();
  ASSERT_TRUE(wrapper_1);
  ui::AutomationAXTreeWrapper* wrapper_2 =
      GetTreeIDToTreeMap()[tree_2_id].get();
  ASSERT_TRUE(wrapper_2);

  ui::AutomationAXTreeWrapper* focused_wrapper = nullptr;
  ui::AXNode* focused_node = nullptr;
  CallGetFocusInternal(wrapper_0, &focused_wrapper, &focused_node);
  ASSERT_TRUE(focused_wrapper);
  ASSERT_TRUE(focused_node);
  EXPECT_EQ(wrapper_1, focused_wrapper);
  EXPECT_EQ(tree_1_id, focused_node->tree()->GetAXTreeID());

  // This is an interesting inconsistency as this node is technically not in the
  // app (which starts at the link in wrapper 1).
  EXPECT_EQ(ax::mojom::Role::kButton, focused_node->GetRole());

  // Push an update where we change the focus.
  focused_wrapper = nullptr;
  focused_node = nullptr;

  // The link in wrapper 0 which has a child tree id pointing to wrapper 2.
  bundles[0].updates[0].tree_data.focus_id = 3;
  SendOnAccessibilityEvents(bundles[0], true /* active profile */);
  CallGetFocusInternal(wrapper_0, &focused_wrapper, &focused_node);
  ASSERT_TRUE(focused_wrapper);
  ASSERT_TRUE(focused_node);
  EXPECT_EQ(wrapper_2, focused_wrapper);
  EXPECT_EQ(tree_2_id, focused_node->tree()->GetAXTreeID());
  EXPECT_EQ(ax::mojom::Role::kButton, focused_node->GetRole());
}

TEST_F(AutomationInternalCustomBindingsTest, GetBoundsAppIdConstruction) {
  // two trees each with a button.
  std::vector<ExtensionMsg_AccessibilityEventBundleParams> bundles;
  for (int i = 0; i < 2; i++) {
    bundles.emplace_back();
    auto& bundle = bundles.back();
    bundle.updates.emplace_back();
    auto& tree_update = bundle.updates.back();
    tree_update.has_tree_data = true;
    tree_update.root_id = 1;
    auto& tree_data = tree_update.tree_data;
    tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    bundle.tree_id = tree_data.tree_id;
    tree_update.nodes.emplace_back();
    auto& node_data1 = tree_update.nodes.back();
    node_data1.id = 1;
    node_data1.role =
        i == 0 ? ax::mojom::Role::kDesktop : ax::mojom::Role::kRootWebArea;
    node_data1.child_ids.push_back(2);
    node_data1.relative_bounds.bounds = gfx::RectF(100, 100, 100, 100);
    tree_update.nodes.emplace_back();
    auto& node_data2 = tree_update.nodes.back();
    node_data2.id = 2;
    node_data2.role = ax::mojom::Role::kButton;
    node_data2.relative_bounds.bounds = gfx::RectF(0, 0, 200, 200);
  }

  // Link up the trees by app id.
  ui::AXTreeID tree_0_id = bundles[0].updates[0].tree_data.tree_id;
  ui::AXTreeID tree_1_id = bundles[1].updates[0].tree_data.tree_id;
  auto& wrapper0_button_data = bundles[0].updates[0].nodes[1];
  auto& wrapper1_button_data = bundles[1].updates[0].nodes[1];

  // This construction requires the hosting and client nodes annotate with the
  // same app id.
  wrapper0_button_data.AddStringAttribute(
      ax::mojom::StringAttribute::kChildTreeNodeAppId, "app1");
  wrapper1_button_data.AddStringAttribute(ax::mojom::StringAttribute::kAppId,
                                          "app1");

  wrapper0_button_data.AddFloatAttribute(
      ax::mojom::FloatAttribute::kChildTreeScale, 2.0);

  for (auto& bundle : bundles)
    SendOnAccessibilityEvents(bundle, true /* active profile */);

  ASSERT_EQ(2U, GetTreeIDToTreeMap().size());

  ui::AutomationAXTreeWrapper* wrapper_0 =
      GetTreeIDToTreeMap()[tree_0_id].get();
  ASSERT_TRUE(wrapper_0);
  ui::AutomationAXTreeWrapper* wrapper_1 =
      GetTreeIDToTreeMap()[tree_1_id].get();
  ASSERT_TRUE(wrapper_1);

  ui::AXNode* wrapper1_button = wrapper_1->ax_tree()->GetFromId(2);
  ASSERT_TRUE(wrapper1_button);

  // The button in wrapper 1 is scaled by .5 (200 * .5). It's root is also
  // scaled (100 * .5). In wrapper 0, it is *not* offset by the tree's root
  // bounds.
  EXPECT_EQ(gfx::Rect(50, 50, 100, 100),
            CallComputeGlobalNodeBounds(wrapper_1, wrapper1_button));
}

TEST_F(AutomationInternalCustomBindingsTest, ActionStringMapping) {
  for (uint32_t action = static_cast<uint32_t>(ax::mojom::Action::kNone) + 1;
       action <= static_cast<uint32_t>(ax::mojom::Action::kMaxValue);
       ++action) {
    const char* val = ui::ToString(static_cast<ax::mojom::Action>(action));
    EXPECT_NE(api::automation::ACTION_TYPE_NONE,
              api::automation::ParseActionType(val))
        << "No automation mapping found for ax::mojom::Action::" << val;
  }
}

TEST_F(AutomationInternalCustomBindingsTest, GetBoundsNestedAppIdConstruction) {
  // two trees each with a button and a client node.
  std::vector<ExtensionMsg_AccessibilityEventBundleParams> bundles;
  for (int i = 0; i < 2; i++) {
    bundles.emplace_back();
    auto& bundle = bundles.back();
    bundle.updates.emplace_back();
    auto& tree_update = bundle.updates.back();
    tree_update.has_tree_data = true;
    tree_update.root_id = 1;
    auto& tree_data = tree_update.tree_data;
    tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    bundle.tree_id = tree_data.tree_id;
    tree_update.nodes.emplace_back();
    auto& node_data1 = tree_update.nodes.back();
    node_data1.id = 1;
    node_data1.role =
        i == 0 ? ax::mojom::Role::kDesktop : ax::mojom::Role::kRootWebArea;
    node_data1.child_ids.push_back(2);
    node_data1.child_ids.push_back(3);
    node_data1.relative_bounds.bounds = gfx::RectF(100, 100, 100, 100);
    tree_update.nodes.emplace_back();
    auto& node_data2 = tree_update.nodes.back();
    node_data2.id = 2;
    node_data2.role = ax::mojom::Role::kButton;
    node_data2.relative_bounds.bounds = gfx::RectF(0, 0, 200, 200);
    tree_update.nodes.emplace_back();
    auto& node_data3 = tree_update.nodes.back();
    node_data3.id = 3;
    node_data3.role = ax::mojom::Role::kClient;
    node_data3.relative_bounds.bounds = gfx::RectF(0, 0, 200, 200);
  }

  // Link up the trees by app id. One button -> child button; client -> child
  // root.
  ui::AXTreeID tree_0_id = bundles[0].updates[0].tree_data.tree_id;
  ui::AXTreeID tree_1_id = bundles[1].updates[0].tree_data.tree_id;
  auto& wrapper0_button_data = bundles[0].updates[0].nodes[1];
  auto& wrapper0_client_data = bundles[0].updates[0].nodes[2];
  auto& wrapper1_root_data = bundles[1].updates[0].nodes[0];
  auto& wrapper1_button_data = bundles[1].updates[0].nodes[1];

  // This construction requires the hosting and client nodes annotate with the
  // same app id.
  wrapper0_button_data.AddStringAttribute(
      ax::mojom::StringAttribute::kChildTreeNodeAppId, "app1");
  wrapper1_button_data.AddStringAttribute(ax::mojom::StringAttribute::kAppId,
                                          "app1");

  wrapper0_button_data.AddFloatAttribute(
      ax::mojom::FloatAttribute::kChildTreeScale, 2.0);

  // Adding this app id should not impact the above bounds computation.
  wrapper0_client_data.AddStringAttribute(
      ax::mojom::StringAttribute::kChildTreeNodeAppId, "lacrosHost");
  wrapper1_root_data.AddStringAttribute(ax::mojom::StringAttribute::kAppId,
                                        "lacrosHost");

  for (auto& bundle : bundles)
    SendOnAccessibilityEvents(bundle, true /* active profile */);

  ASSERT_EQ(2U, GetTreeIDToTreeMap().size());

  ui::AutomationAXTreeWrapper* wrapper_0 =
      GetTreeIDToTreeMap()[tree_0_id].get();
  ASSERT_TRUE(wrapper_0);
  ui::AutomationAXTreeWrapper* wrapper_1 =
      GetTreeIDToTreeMap()[tree_1_id].get();
  ASSERT_TRUE(wrapper_1);

  ui::AXNode* wrapper1_button = wrapper_1->ax_tree()->GetFromId(2);
  ASSERT_TRUE(wrapper1_button);

  // The button in wrapper 1 is scaled by .5 (200 * .5). It's root is also
  // scaled (100 * .5). In wrapper 0, it is *not* offset by the tree's root
  // bounds.
  EXPECT_EQ(gfx::Rect(50, 50, 100, 100),
            CallComputeGlobalNodeBounds(wrapper_1, wrapper1_button));

  ui::AXNode* wrapper1_root = wrapper_1->ax_tree()->GetFromId(1);
  ASSERT_TRUE(wrapper1_root);

  // Similar to the button, but not scaled. This does not cross an app id
  // boundary, so is also offset by the parent tree's root (100 + 100).
  EXPECT_EQ(gfx::Rect(200, 200, 100, 100),
            CallComputeGlobalNodeBounds(wrapper_1, wrapper1_root));
}

TEST_F(AutomationInternalCustomBindingsTest, IgnoredAncestorTrees) {
  // Three trees each with a button and link.
  std::vector<ExtensionMsg_AccessibilityEventBundleParams> bundles;
  for (int i = 0; i < 3; i++) {
    bundles.emplace_back();
    auto& bundle = bundles.back();
    bundle.updates.emplace_back();
    auto& tree_update = bundle.updates.back();
    tree_update.has_tree_data = true;
    tree_update.root_id = 1;
    auto& tree_data = tree_update.tree_data;

    // This is a point of inconsistency as the mojo representation allows
    // updates from multiple trees.
    tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    bundle.tree_id = tree_data.tree_id;
    tree_data.focus_id = 2;
    tree_update.nodes.emplace_back();
    auto& node_data1 = tree_update.nodes.back();
    node_data1.id = 1;
    node_data1.role = ax::mojom::Role::kRootWebArea;
    node_data1.child_ids.push_back(2);
    node_data1.child_ids.push_back(3);
    tree_update.nodes.emplace_back();
    auto& node_data2 = tree_update.nodes.back();
    node_data2.id = 2;
    node_data2.role = ax::mojom::Role::kButton;
    tree_update.nodes.emplace_back();
    auto& node_data3 = tree_update.nodes.back();
    node_data3.id = 3;
    node_data3.role = ax::mojom::Role::kLink;
  }

  // Link up the trees so that the first is a parent of the second and the
  // second a parent of the third.
  ui::AXTreeID tree_0_id = bundles[0].updates[0].tree_data.tree_id;
  ui::AXTreeID tree_1_id = bundles[1].updates[0].tree_data.tree_id;
  ui::AXTreeID tree_2_id = bundles[2].updates[0].tree_data.tree_id;
  bundles[0].updates[0].nodes[1].AddChildTreeId(tree_1_id);

  // Make the hosting node ignored.
  bundles[0].updates[0].nodes[1].AddState(ax::mojom::State::kInvisible);

  bundles[1].updates[0].nodes[1].AddChildTreeId(tree_2_id);

  for (auto& bundle : bundles)
    SendOnAccessibilityEvents(bundle, true /* active profile */);

  ASSERT_EQ(3U, GetTreeIDToTreeMap().size());

  ui::AutomationAXTreeWrapper* wrapper_0 =
      GetTreeIDToTreeMap()[tree_0_id].get();
  ASSERT_TRUE(wrapper_0);
  ui::AutomationAXTreeWrapper* wrapper_1 =
      GetTreeIDToTreeMap()[tree_1_id].get();
  ASSERT_TRUE(wrapper_1);
  ui::AutomationAXTreeWrapper* wrapper_2 =
      GetTreeIDToTreeMap()[tree_2_id].get();
  ASSERT_TRUE(wrapper_2);

  // The root tree isn't ignored.
  EXPECT_FALSE(wrapper_0->IsTreeIgnored());

  // However, since the hosting node in |wrapper_0| is ignored, both of the
  // descendant trees should be ignored.
  EXPECT_TRUE(wrapper_1->IsTreeIgnored());
  EXPECT_TRUE(wrapper_2->IsTreeIgnored());

  // No longer invisible.
  ui::AXNode* button = wrapper_0->ax_tree()->GetFromId(2);
  ui::AXNodeData data = button->TakeData();
  data.RemoveState(ax::mojom::State::kInvisible);
  button->SetData(data);

  EXPECT_FALSE(wrapper_0->IsTreeIgnored());
  EXPECT_FALSE(wrapper_1->IsTreeIgnored());
  EXPECT_FALSE(wrapper_2->IsTreeIgnored());
}

TEST_F(AutomationInternalCustomBindingsTest,
       GetMultipleChildRootsAppIdConstruction) {
  // Two trees each with a button and a client node.
  std::vector<ExtensionMsg_AccessibilityEventBundleParams> bundles;
  for (int i = 0; i < 2; i++) {
    bundles.emplace_back();
    auto& bundle = bundles.back();
    bundle.updates.emplace_back();
    auto& tree_update = bundle.updates.back();
    tree_update.has_tree_data = true;
    tree_update.root_id = 1;
    auto& tree_data = tree_update.tree_data;
    tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    bundle.tree_id = tree_data.tree_id;
    tree_update.nodes.emplace_back();
    auto& node_data1 = tree_update.nodes.back();
    node_data1.id = 1;
    node_data1.role =
        i == 0 ? ax::mojom::Role::kDesktop : ax::mojom::Role::kRootWebArea;
    node_data1.child_ids.push_back(2);
    node_data1.child_ids.push_back(3);
    node_data1.relative_bounds.bounds = gfx::RectF(100, 100, 100, 100);
    tree_update.nodes.emplace_back();
    auto& node_data2 = tree_update.nodes.back();
    node_data2.id = 2;
    node_data2.role = ax::mojom::Role::kButton;
    node_data2.relative_bounds.bounds = gfx::RectF(0, 0, 200, 200);
    tree_update.nodes.emplace_back();
    auto& node_data3 = tree_update.nodes.back();
    node_data3.id = 3;
    node_data3.role = ax::mojom::Role::kClient;
    node_data3.relative_bounds.bounds = gfx::RectF(0, 0, 200, 200);
  }

  // Link up the trees by using one app id. Tree 0's client has two children
  // from tree 1.
  ui::AXTreeID tree_0_id = bundles[0].updates[0].tree_data.tree_id;
  ui::AXTreeID tree_1_id = bundles[1].updates[0].tree_data.tree_id;
  auto& wrapper0_client_data = bundles[0].updates[0].nodes[2];
  auto& wrapper1_button_data = bundles[1].updates[0].nodes[1];
  auto& wrapper1_client_data = bundles[1].updates[0].nodes[2];

  // This construction requires the hosting and client nodes annotate with the
  // same app id.
  wrapper0_client_data.AddStringAttribute(
      ax::mojom::StringAttribute::kChildTreeNodeAppId, "app1");
  wrapper1_button_data.AddStringAttribute(ax::mojom::StringAttribute::kAppId,
                                          "app1");
  wrapper1_client_data.AddStringAttribute(ax::mojom::StringAttribute::kAppId,
                                          "app1");

  for (auto& bundle : bundles)
    SendOnAccessibilityEvents(bundle, true /* active profile */);

  ASSERT_EQ(2U, GetTreeIDToTreeMap().size());

  ui::AutomationAXTreeWrapper* wrapper_0 =
      GetTreeIDToTreeMap()[tree_0_id].get();
  ASSERT_TRUE(wrapper_0);

  ui::AXNode* wrapper0_client = wrapper_0->ax_tree()->GetFromId(3);
  ASSERT_TRUE(wrapper0_client);

  std::vector<ui::AXNode*> child_roots =
      CallGetRootsOfChildTree(wrapper0_client);
  EXPECT_EQ(2U, child_roots.size());
  EXPECT_EQ(tree_1_id, child_roots[0]->tree()->GetAXTreeID());
  EXPECT_EQ(tree_1_id, child_roots[1]->tree()->GetAXTreeID());
  EXPECT_EQ(2, child_roots[0]->id());
  EXPECT_EQ(3, child_roots[1]->id());
}

TEST_F(AutomationInternalCustomBindingsTest, FireEventsWithListeners) {
  // A simple tree.
  ExtensionMsg_AccessibilityEventBundleParams bundle;
  bundle.updates.emplace_back();
  auto& tree_update = bundle.updates.back();
  tree_update.has_tree_data = true;
  tree_update.root_id = 1;
  auto& tree_data = tree_update.tree_data;
  tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  bundle.tree_id = tree_data.tree_id;
  tree_update.nodes.emplace_back();
  auto& node_data1 = tree_update.nodes.back();
  node_data1.id = 1;
  node_data1.role = ax::mojom::Role::kRootWebArea;
  node_data1.child_ids.push_back(2);
  node_data1.relative_bounds.bounds = gfx::RectF(100, 100, 100, 100);
  tree_update.nodes.emplace_back();
  auto& node_data2 = tree_update.nodes.back();
  node_data2.id = 2;
  node_data2.role = ax::mojom::Role::kButton;
  node_data2.relative_bounds.bounds = gfx::RectF(0, 0, 200, 200);

  // Add a hook for events from automation.
  std::vector<api::automation::EventType> events;
  AddAutomationEventCallback(base::BindLambdaForTesting(
      [&](api::automation::EventType event) { events.push_back(event); }));

  SendOnAccessibilityEvents(bundle, true /* active profile */);

  // We aren't listening for any events yet, but we should still get one that
  // gets fired on initial tree creation.
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(api::automation::EVENT_TYPE_NONE, events[0]);
  events.clear();

  // Remove the root node data and don't update tree data.
  tree_update.nodes.erase(tree_update.nodes.begin());
  tree_update.has_tree_data = false;

  // Trigger a role change.
  tree_update.nodes[0].role = ax::mojom::Role::kSwitch;
  SendOnAccessibilityEvents(bundle, true /* active profile */);

  // There should be no events since there are no listeners and this isn't the
  // initial tree.
  ASSERT_TRUE(events.empty());

  // Add a role change listener and do trigger the role change again.
  auto* wrapper = GetTreeIDToTreeMap()[tree_data.tree_id].get();
  auto* tree = wrapper->ax_tree();
  // The button is id 2.
  std::tuple<ax::mojom::Event, ui::AXEventGenerator::Event> event_type(
      ax::mojom::Event::kNone, ui::AXEventGenerator::Event::ROLE_CHANGED);
  wrapper->EventListenerAdded(event_type, tree->GetFromId(2));
  EXPECT_EQ(1U, wrapper->EventListenerCount());
  EXPECT_TRUE(wrapper->HasEventListener(event_type, tree->GetFromId(2)));
  tree_update.nodes[0].role = ax::mojom::Role::kButton;
  SendOnAccessibilityEvents(bundle, true /* active profile */);

  // We should now have exactly one event.
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(api::automation::EVENT_TYPE_ROLECHANGED, events[0]);
  events.clear();

  // Now, remove the listener and do the same as above.
  wrapper->EventListenerRemoved(event_type, tree->GetFromId(2));
  // We have to add another listener to ensure we don't shut down (no event
  // listeners means this renderer closes).
  wrapper->EventListenerAdded(
      std::tuple<ax::mojom::Event, ui::AXEventGenerator::Event>(
          ax::mojom::Event::kLoadComplete, ui::AXEventGenerator::Event::NONE),
      tree->GetFromId(1));
  tree_update.nodes[0].role = ax::mojom::Role::kSwitch;
  SendOnAccessibilityEvents(bundle, true /* active profile */);

  // We should have no events.
  ASSERT_TRUE(events.empty());

  // Finally, let's fire a non-generated event on the button, but add the
  // listener on the root. This will test both non-generated events and
  // respecting event listeners on ancestors of the target.

  // First, fire the event without the click listener.
  tree_update.nodes.clear();
  bundle.events.emplace_back();
  auto& event = bundle.events.back();
  event.event_type = ax::mojom::Event::kClicked;
  event.id = 2;
  SendOnAccessibilityEvents(bundle, true /* active profile */);

  // No event.
  ASSERT_TRUE(events.empty());

  // Now, add the click listener to the root, and fire the click event on the
  // button.
  wrapper->EventListenerAdded(
      std::tuple<ax::mojom::Event, ui::AXEventGenerator::Event>(
          ax::mojom::Event::kClicked, ui::AXEventGenerator::Event::NONE),
      tree->GetFromId(1));
  SendOnAccessibilityEvents(bundle, true /* active profile */);

  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(api::automation::EVENT_TYPE_CLICKED, events[0]);
}

}  // namespace extensions
