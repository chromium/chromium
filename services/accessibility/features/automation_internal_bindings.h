// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_AUTOMATION_INTERNAL_BINDINGS_H_
#define SERVICES_ACCESSIBILITY_FEATURES_AUTOMATION_INTERNAL_BINDINGS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
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

namespace gin {
class Arguments;
}  // namespace gin

namespace ax {
class BindingsIsolateHolder;

// AutomationInternalBindings creates the Javascript V8 bindings for the
// Automation API in the Accessibility Service. It runs in a V8 thread.
// The service may have multiple AutomationInternalBindings, one per
// V8 isolate, each owned by a V8Manager.
class AutomationInternalBindings : public mojom::Automation,
                                   public ui::AutomationTreeManagerOwner,
                                   public ui::AutomationV8Router {
 public:
  // AutomationInternalBindings will use the |ax_service_client| on the
  // |main_runner| to bind itself to the main OS process. We assume that
  // |isolate_holder| has a longer lifetime than this class.
  // Specifically, AutomationInternalBindings is owned by
  // V8Manager which implements BindingsIsolateHolder.
  explicit AutomationInternalBindings(
      base::WeakPtr<BindingsIsolateHolder> isolate_holder,
      base::WeakPtr<mojom::AccessibilityServiceClient> ax_service_client,
      scoped_refptr<base::SequencedTaskRunner> main_runner);
  ~AutomationInternalBindings() override;
  AutomationInternalBindings(const AutomationInternalBindings&) = delete;
  AutomationInternalBindings& operator=(const AutomationInternalBindings&) =
      delete;

  // Creates bindings between C++ functions and Javascript by adding
  // V8 bindings to the given |object_template|.
  // Adds V8 bindings for the chrome.automation API.
  void AddAutomationRoutesToTemplate(
      v8::Local<v8::ObjectTemplate>* object_template);
  // Adds V8 bindings for the chrome.automationInternal API.
  void AddAutomationInternalRoutesToTemplate(
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
  void StartCachingAccessibilityTrees() override {}
  void StopCachingAccessibilityTrees() override {}
  void DispatchEvent(const std::string& event_name,
                     const base::Value::List& event_args) const override;

  // Methods to communicate back to the OS main process. These should get bound
  // to V8 JS methods and called from there.
  void Enable(gin::Arguments* args);
  void Disable(gin::Arguments* args);
  void EnableTree(const ui::AXTreeID& tree_id);
  void PerformAction(const ui::AXActionData& action_data);

 private:
  friend class AutomationInternalBindingsTest;

  // Binds to Automation in the OS on the |main_runner|.
  void Bind(base::WeakPtr<mojom::AccessibilityServiceClient> at_controller,
            scoped_refptr<base::SequencedTaskRunner> main_runner);

  // TODO(crbug.com/1355633): Override these from
  // mojom::Automation:
  void DispatchTreeDestroyedEvent(const ui::AXTreeID& tree_id);
  void DispatchActionResult(const ui::AXActionData& data, bool result);
  void DispatchAccessibilityEvents(
      const ui::AXTreeID& tree_id,
      const std::vector<ui::AXTreeUpdate>& updates,
      const gfx::Point& mouse_location,
      const std::vector<ui::AXEvent>& events) override;
  void DispatchAccessibilityLocationChange(
      const ui::AXTreeID& tree_id,
      int node_id,
      const ui::AXRelativeBounds& bounds) override;
  void DispatchGetTextLocationResult(const ax::mojom::AXActionData& data,
                                     gfx::Rect rect);

  // Used during object template creation.
  raw_ptr<v8::Local<v8::ObjectTemplate>, ExperimentalAsh> template_;

  base::WeakPtr<BindingsIsolateHolder> isolate_holder_;

  std::unique_ptr<ui::AutomationV8Bindings> automation_v8_bindings_;

  mojo::Receiver<mojom::Automation> automation_receiver_{this};

  // We can send automation info back to the main Service thread with the
  // automation client interface.
  mojo::Remote<mojom::AutomationClient> automation_client_remote_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AutomationInternalBindings> weak_ptr_factory_{this};
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_AUTOMATION_INTERNAL_BINDINGS_H_
