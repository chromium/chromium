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
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/platform/automation/automation_tree_manager_owner.h"
#include "ui/accessibility/platform/automation/automation_v8_bindings.h"
#include "ui/accessibility/platform/automation/automation_v8_router.h"

struct ExtensionMsg_AccessibilityEventBundleParams;
struct ExtensionMsg_AccessibilityLocationChangeParams;

namespace ui {
class AutomationV8Bindings;
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

  void OnMessageReceived(const IPC::Message& message);

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

  // ui::AutomationTreeManagerOwner:
  ui::AutomationV8Bindings* GetAutomationV8Bindings() const override;
  void NotifyTreeEventListenersChanged() override;

  // ui::AutomationV8Router:
  void ThrowInvalidArgumentsException(bool is_fatal = true) const override;
  v8::Isolate* GetIsolate() const override;
  v8::Local<v8::Context> GetContext() const override;
  void RouteHandlerFunction(const std::string& name,
                            scoped_refptr<ui::V8HandlerFunctionWrapper>
                                handler_function_wrapper) override;
  ui::TreeChangeObserverFilter ParseTreeChangeObserverFilter(
      const std::string& filter) const override;
  std::string GetMarkerTypeString(ax::mojom::MarkerType type) const override;
  std::string GetFocusedStateString() const override;
  std::string GetOffscreenStateString() const override;
  std::string GetLocalizedStringForImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus status) const override;
  std::string GetTreeChangeTypeString(
      ax::mojom::Mutation change_type) const override;
  std::string GetEventTypeString(
      const std::tuple<ax::mojom::Event, ui::AXEventGenerator::Event>&
          event_type) const override;
  // This enables the MessageFilter that allows us to listen to accessibility
  // events forwarded to this process.
  void StartCachingAccessibilityTrees() override;
  // This disables the MessageFilter that allows us to listen to accessibility
  // events forwarded to this process.
  void StopCachingAccessibilityTrees() override;
  void DispatchEvent(const std::string& event_name,
                     const base::Value::List& event_args) const override;

 private:
  // ObjectBackedNativeHandler overrides:
  void Invalidate() override;

  // Returns whether this extension has the "interact" permission set (either
  // explicitly or implicitly after manifest parsing).
  void IsInteractPermitted(
      const v8::FunctionCallbackInfo<v8::Value>& args) const;

  // Handle accessibility events from the browser process sent
  // over IPC.
  void HandleAccessibilityEvents(
      const ExtensionMsg_AccessibilityEventBundleParams& events,
      bool is_active_profile);
  void HandleAccessibilityLocationChange(
      const ExtensionMsg_AccessibilityLocationChangeParams& params);

  scoped_refptr<AutomationMessageFilter> message_filter_;
  NativeExtensionBindingsSystem* bindings_system_;
  bool should_ignore_context_;

  std::unique_ptr<ui::AutomationV8Bindings> automation_v8_bindings_;

  base::WeakPtrFactory<AutomationInternalCustomBindings> weak_ptr_factory_{
      this};
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_
