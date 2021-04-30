// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/automation/automation_internal_custom_bindings.h"

#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"

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

  std::map<ui::AXTreeID, std::unique_ptr<AutomationAXTreeWrapper>>&
  GetTreeIDToTreeMap() {
    return automation_internal_bindings_->tree_id_to_tree_wrapper_map_;
  }

  void SendOnAccessibilityEvents(
      const ExtensionMsg_AccessibilityEventBundleParams& event_bundle,
      bool is_active_profile) {
    automation_internal_bindings_->OnAccessibilityEvents(event_bundle,
                                                         is_active_profile);
  }

 private:
  AutomationInternalCustomBindings* automation_internal_bindings_ = nullptr;
};

TEST_F(AutomationInternalCustomBindingsTest, TestGetDesktop) {
  EXPECT_TRUE(GetTreeIDToTreeMap().empty());

  // Send a tree with one node having role desktop.
  ExtensionMsg_AccessibilityEventBundleParams bundle;
  bundle.updates.emplace_back();
  auto& tree_update = bundle.updates.back();
  tree_update.nodes.emplace_back();
  auto& node_data = tree_update.nodes.back();
  node_data.role = ax::mojom::Role::kDesktop;
  SendOnAccessibilityEvents(bundle, true /* active profile */);

  ASSERT_EQ(1U, GetTreeIDToTreeMap().size());

  AutomationAXTreeWrapper* desktop = GetTreeIDToTreeMap().begin()->second.get();
  ASSERT_TRUE(desktop);
  EXPECT_TRUE(desktop->IsDesktopTree());
}

}  // namespace extensions
