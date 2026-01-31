// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/renderer/api/automation/automation_internal_custom_bindings.h"

#include "base/test/bind.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"
#include "ui/accessibility/ax_enum_util.h"

namespace extensions {

class AutomationInternalCustomBindingsTest
    : public NativeExtensionBindingsSystemUnittest {
 public:
  void SetUp() override {
    NativeExtensionBindingsSystemUnittest::SetUp();

    // Bootstrap a simple extension with desktop automation permissions.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("testExtension")
            .SetManifestPath("automation.desktop", true)
            .SetLocation(mojom::ManifestLocation::kComponent)
            .Build();
    RegisterExtension(extension);
    v8::HandleScope handle_scope(isolate());
    v8::Local<v8::Context> context = MainContext();
    ScriptContext* script_context = CreateScriptContext(
        context, extension.get(), mojom::ContextType::kPrivilegedExtension);
    script_context->set_url(extension->url());
    bindings_system()->UpdateBindingsForContext(script_context);

    // TODO(crbug.com/40283128) Add tests for service worker.
    auto automation_internal_bindings =
        std::make_unique<AutomationInternalCustomBindings>(script_context,
                                                           bindings_system());
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
};

TEST_F(AutomationInternalCustomBindingsTest, ActionStringMapping) {
  for (uint32_t action = static_cast<uint32_t>(ax::mojom::Action::kNone) + 1;
       action <= static_cast<uint32_t>(ax::mojom::Action::kMaxValue);
       ++action) {
    const char* val = ui::ToString(static_cast<ax::mojom::Action>(action));
    EXPECT_NE(api::automation::ActionType::kNone,
              api::automation::ParseActionType(val))
        << "No automation mapping found for ax::mojom::Action::" << val;
  }
}

}  // namespace extensions
