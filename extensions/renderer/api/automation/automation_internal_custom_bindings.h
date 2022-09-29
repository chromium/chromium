// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_
#define EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "extensions/common/api/automation.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "ipc/ipc_message.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/platform/automation/automation_ax_tree_wrapper.h"
#include "ui/accessibility/platform/automation/automation_tree_manager_owner.h"
#include "ui/accessibility/platform/automation/automation_v8_router.h"

struct ExtensionMsg_AccessibilityEventBundleParams;
struct ExtensionMsg_AccessibilityLocationChangeParams;

namespace ui {
class AutomationV8Bindings;
struct AXEvent;
}

namespace extensions {

class AutomationInternalCustomBindings;
class AutomationMessageFilter;
class NativeExtensionBindingsSystem;

// The native component of custom bindings for the chrome.automationInternal
// API.
class AutomationInternalCustomBindings : public ObjectBackedNativeHandler,
                                         public ui::AutomationTreeManagerOwner,
                                         public ui::AutomationV8Router {
 public:
  AutomationInternalCustomBindings(
      ScriptContext* context,
      NativeExtensionBindingsSystem* bindings_system);

  AutomationInternalCustomBindings(const AutomationInternalCustomBindings&) =
      delete;
  AutomationInternalCustomBindings& operator=(
      const AutomationInternalCustomBindings&) = delete;

  ~AutomationInternalCustomBindings() override;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

  void OnMessageReceived(const IPC::Message& message);

  // ui::AutomationTreeManagerOwner:
  void SendNodesRemovedEvent(ui::AXTree* tree,
                             const std::vector<int>& ids) override;
  bool SendTreeChangeEvent(ax::mojom::Mutation change_type,
                           ui::AXTree* tree,
                           ui::AXNode* node) override;
  void SendAutomationEvent(
      ui::AXTreeID tree_id,
      const gfx::Point& mouse_location,
      const ui::AXEvent& event,
      absl::optional<ui::AXEventGenerator::Event> generated_event_type =
          absl::optional<ui::AXEventGenerator::Event>()) override;
  void NotifyTreeEventListenersChanged() override;

  // ui::AutomationV8Router:
  void ThrowInvalidArgumentsException(bool is_fatal = true) const override;
  v8::Isolate* GetIsolate() const override;
  v8::Local<v8::Context> GetContext() const override;
  void RouteHandlerFunction(
      const std::string& name,
      AutomationV8Router::HandlerFunction handler_function) override;
  void RouteHandlerFunction(
      const std::string& name,
      const std::string& api_name,
      AutomationV8Router::HandlerFunction handler_function) override;
  std::tuple<ax::mojom::Event, ui::AXEventGenerator::Event> ParseEventType(
      const std::string& event_type) const override;
  ui::TreeChangeObserverFilter ParseTreeChangeObserverFilter(
      const std::string& filter) const override;
  std::string GetMarkerTypeString(ax::mojom::MarkerType type) const override;
  void DispatchEvent(const std::string& event_name,
                     const base::Value::List& event_args) const override;

 private:
  friend class AutomationInternalCustomBindingsTest;

  // ObjectBackedNativeHandler overrides:
  void Invalidate() override;

  // Returns whether this extension has the "interact" permission set (either
  // explicitly or implicitly after manifest parsing).
  void IsInteractPermitted(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Returns an object with bindings that will be added to the
  // chrome.automation namespace.
  void GetSchemaAdditions(const v8::FunctionCallbackInfo<v8::Value>& args);

  // This is called by automation_internal_custom_bindings.js to indicate
  // that an API was called that needs access to accessibility trees. This
  // enables the MessageFilter that allows us to listen to accessibility
  // events forwarded to this process.
  void StartCachingAccessibilityTrees(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // This is called by automation_internal_custom_bindings.js to indicate
  // that an API was called that turns off accessibility trees. This
  // disables the MessageFilter that allows us to listen to accessibility
  // events forwarded to this process and clears all existing tree state.
  void StopCachingAccessibilityTrees(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: string ax_tree_id, int node_id
  // Returns: JS object with a string key for each state flag that's set.
  void GetState(const v8::FunctionCallbackInfo<v8::Value>& args);

  void GetImageAnnotation(v8::Isolate* isolate,
                          v8::ReturnValue<v8::Value> result,
                          ui::AutomationAXTreeWrapper* tree_wrapper,
                          ui::AXNode* node);

  //
  // Helper functions.
  //

  // Handle accessibility events from the browser process.
  void OnAccessibilityEvents(
      const ExtensionMsg_AccessibilityEventBundleParams& events,
      bool is_active_profile);

  void OnAccessibilityLocationChange(
      const ExtensionMsg_AccessibilityLocationChangeParams& params);

  void SendChildTreeIDEvent(ui::AXTreeID child_tree_id);

  std::string GetLocalizedStringForImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus status) const;

  void MaybeSendOnAllAutomationEventListenersRemoved();

  scoped_refptr<AutomationMessageFilter> message_filter_;
  bool is_active_profile_;
  NativeExtensionBindingsSystem* bindings_system_;
  bool should_ignore_context_;

  std::unique_ptr<ui::AutomationV8Bindings> automation_v8_bindings_;

  base::RepeatingCallback<void(api::automation::EventType)>
      notify_event_for_testing_;

  base::WeakPtrFactory<AutomationInternalCustomBindings> weak_ptr_factory_{
      this};
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_
