// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_
#define EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_

#include <map>
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
#include "v8/include/v8.h"

struct ExtensionMsg_AccessibilityEventBundleParams;
struct ExtensionMsg_AccessibilityLocationChangeParams;

namespace ui {
struct AXEvent;
}

namespace extensions {

class AutomationInternalCustomBindings;
class AutomationMessageFilter;
class NativeExtensionBindingsSystem;

struct TreeChangeObserver {
  int id;
  api::automation::TreeChangeObserverFilter filter;
};

// The native component of custom bindings for the chrome.automationInternal
// API.
class AutomationInternalCustomBindings : public ObjectBackedNativeHandler,
                                         public ui::AutomationTreeManagerOwner {
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

  ScriptContext* context() const {
    return ObjectBackedNativeHandler::context();
  }

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

  // Called when an accessibility tree is destroyed and needs to be
  // removed from our cache.
  // Args: string ax_tree_id
  void DestroyAccessibilityTree(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  void AddTreeChangeObserver(const v8::FunctionCallbackInfo<v8::Value>& args);

  void RemoveTreeChangeObserver(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  void GetFocus(const v8::FunctionCallbackInfo<v8::Value>& args);

  void RouteTreeIDFunction(
      const std::string& name,
      void (*callback)(v8::Isolate* isolate,
                       v8::ReturnValue<v8::Value> result,
                       ui::AutomationAXTreeWrapper* tree_wrapper));

  void RouteNodeIDFunction(
      const std::string& name,
      std::function<void(v8::Isolate* isolate,
                         v8::ReturnValue<v8::Value> result,
                         ui::AutomationAXTreeWrapper* tree_wrapper,
                         ui::AXNode* node)> callback);
  void RouteNodeIDPlusAttributeFunction(
      const std::string& name,
      void (*callback)(v8::Isolate* isolate,
                       v8::ReturnValue<v8::Value> result,
                       ui::AXTree* tree,
                       ui::AXNode* node,
                       const std::string& attribute_name));
  void RouteNodeIDPlusRangeFunction(
      const std::string& name,
      std::function<void(v8::Isolate* isolate,
                         v8::ReturnValue<v8::Value> result,
                         ui::AutomationAXTreeWrapper* tree_wrapper,
                         ui::AXNode* node,
                         int start,
                         int end,
                         bool clipped)> callback);
  void RouteNodeIDPlusStringBoolFunction(
      const std::string& name,
      std::function<void(v8::Isolate* isolate,
                         v8::ReturnValue<v8::Value> result,
                         ui::AutomationAXTreeWrapper* tree_wrapper,
                         ui::AXNode* node,
                         const std::string& strVal,
                         bool boolVal)> callback);
  void RouteNodeIDPlusDimensionsFunction(
      const std::string& name,
      std::function<void(v8::Isolate* isolate,
                         v8::ReturnValue<v8::Value> result,
                         ui::AutomationAXTreeWrapper* tree_wrapper,
                         ui::AXNode* node,
                         int start,
                         int end,
                         int width,
                         int height)> callback);
  void RouteNodeIDPlusEventFunction(
      const std::string& name,
      std::function<void(v8::Isolate* isolate,
                         v8::ReturnValue<v8::Value> result,
                         ui::AutomationAXTreeWrapper* tree_wrapper,
                         ui::AXNode* node,
                         api::automation::EventType event_type)> callback);

  //
  // Access the cached accessibility trees and properties of their nodes.
  //

  // Args: string ax_tree_id, int node_id, Returns: int child_id.
  void GetChildIDAtIndex(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Returns: string tree_id and int node_id of a node which has global
  // accessibility focus.
  void GetAccessibilityFocus(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: string ax_tree_id.
  void SetDesktopID(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: string ax_tree_id, int node_id
  // Returns: JS object with a map from html attribute key to value.
  void GetHtmlAttributes(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: string ax_tree_id, int node_id
  // Returns: JS object with a string key for each state flag that's set.
  void GetState(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Creates the backing AutomationPosition native object given a request from
  // javascript.
  // Args: string ax_tree_id, int node_id, int offset, bool is_downstream
  // Returns: JS object with bindings back to the native AutomationPosition.
  void CreateAutomationPosition(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  //
  // Helper functions.
  //

  // Handle accessibility events from the browser process.
  void OnAccessibilityEvents(
      const ExtensionMsg_AccessibilityEventBundleParams& events,
      bool is_active_profile);

  void OnAccessibilityLocationChange(
      const ExtensionMsg_AccessibilityLocationChangeParams& params);

  void UpdateOverallTreeChangeObserverFilter();

  void SendChildTreeIDEvent(ui::AXTreeID child_tree_id);

  std::string GetLocalizedStringForImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus status) const;

  void TreeEventListenersChanged(ui::AutomationAXTreeWrapper* tree_wrapper);

  void MaybeSendOnAllAutomationEventListenersRemoved();

  scoped_refptr<AutomationMessageFilter> message_filter_;
  bool is_active_profile_;
  std::vector<TreeChangeObserver> tree_change_observers_;
  // A bit-map of api::automation::TreeChangeObserverFilter.
  int tree_change_observer_overall_filter_;
  NativeExtensionBindingsSystem* bindings_system_;
  bool should_ignore_context_;

  // Keeps track of all trees with event listeners.
  std::set<ui::AXTreeID> trees_with_event_listeners_;

  base::RepeatingCallback<void(api::automation::EventType)>
      notify_event_for_testing_;

  base::WeakPtrFactory<AutomationInternalCustomBindings> weak_ptr_factory_{
      this};
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_
