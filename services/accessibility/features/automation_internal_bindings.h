// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_AUTOMATION_INTERNAL_BINDINGS_H_
#define SERVICES_ACCESSIBILITY_FEATURES_AUTOMATION_INTERNAL_BINDINGS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/accessibility/assistive_technology_controller_impl.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_relative_bounds.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/platform/automation/automation_tree_manager_owner.h"
#include "ui/accessibility/platform/automation/automation_v8_bindings.h"
#include "ui/accessibility/platform/automation/automation_v8_router.h"
#include "v8-value.h"

namespace v8 {
template <typename T>
class Local;
class ObjectTemplate;
}  // namespace v8

namespace ax {
class BindingsIsolateHolder;

// AutomationInternalBindings creates the Javascript V8 bindings for the
// Automation API in the Accessibility Service. It runs in a V8 thread.
// The service may have multiple AutomationInternalBindings, one per
// V8 isolate, each owned by a V8Manager.
class AutomationInternalBindings : public ui::AutomationTreeManagerOwner,
                                   public ui::AutomationV8Router {
 public:
  // AutomationInternalBindings will use the |ax_service_client| on the
  // |main_runner| to bind itself to the main OS thread. We assume that
  // |isolate_holder| has a longer lifetime than this class.
  // Specifically, AutomationInternalBindings is owned by
  // V8Manager which implements BindingsIsolateHolder.
  explicit AutomationInternalBindings(
      BindingsIsolateHolder* isolate_holder,
      mojo::PendingAssociatedReceiver<mojom::Automation> automation);
  ~AutomationInternalBindings() override;
  AutomationInternalBindings(const AutomationInternalBindings&) = delete;
  AutomationInternalBindings& operator=(const AutomationInternalBindings&) =
      delete;

  // Creates bindings between C++ functions and Javascript by adding
  // V8 bindings to the given |object_template|.
  // Adds V8 bindings for the chrome.automation API.
  void AddAutomationRoutesToTemplate(
      v8::Local<v8::ObjectTemplate>* object_template);

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

  // These two functions have no effect on ATP because the automation client
  // pipe connects directly from js where in the extensions implementation it
  // needs to be sent over mojo from renderer to main browser process.
  void StartCachingAccessibilityTrees() override {}
  void StopCachingAccessibilityTrees() override {}
  void DispatchEvent(const std::string& event_name,
                     const base::Value::List& event_args) const override;

 private:
  // Helper function to convert base::Value into v8::Value.
  v8::Local<v8::Value> ConvertToV8Value(base::ValueView value,
                                        v8::Local<v8::Context> context) const;
  v8::Local<v8::Value> ToV8Value(v8::Isolate* isolate,
                                 v8::Local<v8::Object> creation_context,
                                 base::ValueView value) const;
  v8::Local<v8::Value> ToArrayBuffer(
      v8::Isolate* isolate,
      v8::Local<v8::Object> creation_context,
      const base::Value::BlobStorage& value) const;
  v8::Local<v8::Value> ToV8Object(v8::Isolate* isolate,
                                  v8::Local<v8::Object> creation_context,
                                  const base::Value::Dict& val) const;
  v8::Local<v8::Value> ToV8Array(v8::Isolate* isolate,
                                 v8::Local<v8::Object> creation_context,
                                 const base::Value::List& val) const;

  friend class AutomationInternalBindingsTest;

  // Used during object template creation.
  raw_ptr<v8::Local<v8::ObjectTemplate>> template_;

  // Owns `this`.
  raw_ptr<BindingsIsolateHolder> isolate_holder_;

  std::unique_ptr<ui::AutomationV8Bindings> automation_v8_bindings_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AutomationInternalBindings> weak_ptr_factory_{this};
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_AUTOMATION_INTERNAL_BINDINGS_H_
