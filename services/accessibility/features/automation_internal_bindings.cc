// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/automation_internal_bindings.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "gin/function_template.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "services/accessibility/assistive_technology_controller_impl.h"
#include "services/accessibility/automation_impl.h"
#include "services/accessibility/features/bindings_isolate_holder.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/platform/automation/automation_api_util.h"
#include "ui/accessibility/platform/automation/automation_v8_router.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-template.h"

namespace ax {

AutomationInternalBindings::AutomationInternalBindings(
    BindingsIsolateHolder* isolate_holder,
    mojo::PendingAssociatedReceiver<mojom::Automation> automation,
    mojo::PendingRemote<mojom::AutomationClient> automation_client)
    : isolate_holder_(isolate_holder),
      automation_v8_bindings_(std::make_unique<ui::AutomationV8Bindings>(
          /*AutomationTreeManagerOwner=*/this,
          /*AutomationV8Router=*/this)),
      automation_client_remote_(std::move(automation_client)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receiver_.Bind(std::move(automation));
}

AutomationInternalBindings::~AutomationInternalBindings() = default;

void AutomationInternalBindings::AddAutomationRoutesToTemplate(
    v8::Local<v8::ObjectTemplate>* object_template) {
  template_ = object_template;
  automation_v8_bindings_->AddV8Routes();
  template_ = nullptr;
}

void AutomationInternalBindings::AddAutomationInternalRoutesToTemplate(
    v8::Local<v8::ObjectTemplate>* object_template) {
  // Adds V8 route for "automationInternal.enableDesktop" and "disableDesktop".
  (*object_template)
      ->Set(GetIsolate(), "enableDesktop",
            gin::CreateFunctionTemplate(
                GetIsolate(),
                base::BindRepeating(&AutomationInternalBindings::Enable,
                                    weak_ptr_factory_.GetWeakPtr())));
  (*object_template)
      ->Set(GetIsolate(), "disableDesktop",
            gin::CreateFunctionTemplate(
                GetIsolate(),
                base::BindRepeating(&AutomationInternalBindings::Disable,
                                    weak_ptr_factory_.GetWeakPtr())));

  // TODO(crbug.com/1357889): Add bindings for additional AutomationInternalAPI
  // functions, like automationInternal.performAction, etc, paralleling the
  // implementation in
  // extensions/browser/api/automation_internal/automation_internal_api.h.
}

ui::AutomationV8Bindings* AutomationInternalBindings::GetAutomationV8Bindings()
    const {
  DCHECK(automation_v8_bindings_);
  return automation_v8_bindings_.get();
}

void AutomationInternalBindings::NotifyTreeEventListenersChanged() {
  // TODO(crbug.com/1357889): Implement.
}

void AutomationInternalBindings::ThrowInvalidArgumentsException(
    bool is_fatal) const {
  GetIsolate()->ThrowException(v8::String::NewFromUtf8Literal(
      GetIsolate(),
      "Invalid arguments to AutomationInternalBindings function"));
  if (!is_fatal)
    return;
  LOG(FATAL) << "Invalid arguments to AutomationInternalBindings function";
  // TODO(crbug.com/1357889): Could print a stack trace from JS, paralleling
  // AutomationInternalCustomBindings.
}

v8::Isolate* AutomationInternalBindings::GetIsolate() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(isolate_holder_);
  return isolate_holder_->GetIsolate();
}

v8::Local<v8::Context> AutomationInternalBindings::GetContext() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(isolate_holder_);
  return isolate_holder_->GetContext();
}

void AutomationInternalBindings::RouteHandlerFunction(
    const std::string& name,
    scoped_refptr<ui::V8HandlerFunctionWrapper> handler_function_wrapper) {
  DCHECK(template_);
  (*template_)
      ->Set(GetIsolate(), name.c_str(),
            gin::CreateFunctionTemplate(
                GetIsolate(),
                base::BindRepeating(&ui::V8HandlerFunctionWrapper::Run,
                                    handler_function_wrapper)));
}

ui::TreeChangeObserverFilter
AutomationInternalBindings::ParseTreeChangeObserverFilter(
    const std::string& filter) const {
  return ui::TreeChangeObserverFilter::kAllTreeChanges;
}

std::string AutomationInternalBindings::GetMarkerTypeString(
    ax::mojom::MarkerType type) const {
  // TODO(crbug.com/1357889): Implement based on Automation API.
  return ui::ToString(type);
}

std::string AutomationInternalBindings::GetFocusedStateString() const {
  // TODO(crbug.com/1357889): Implement based on Automation API.
  return "focused";
}

std::string AutomationInternalBindings::GetOffscreenStateString() const {
  // TODO(crbug.com/1357889): Implement based on Automation API.
  return "offscreen";
}

std::string
AutomationInternalBindings::GetLocalizedStringForImageAnnotationStatus(
    ax::mojom::ImageAnnotationStatus status) const {
  // TODO(crbug.com/1357889): Implement based on Automation API.
  return ui::ToString(status);
}

std::string AutomationInternalBindings::GetTreeChangeTypeString(
    ax::mojom::Mutation change_type) const {
  // TODO(crbug.com/1357889): Implement based on Automation API.
  return ui::ToString(change_type);
}

std::string AutomationInternalBindings::GetEventTypeString(
    const std::tuple<ax::mojom::Event, ui::AXEventGenerator::Event>& event_type)
    const {
  // TODO(crbug.com/1357889): Implement based on Automation API.
  return "";
}

void AutomationInternalBindings::DispatchEvent(
    const std::string& event_name,
    const base::Value::List& event_args) const {
  // TODO(crbug.com/1357889): Send the event to V8.
}

void AutomationInternalBindings::Enable(gin::Arguments* args) {
  // TODO(crbug.com/1357889): Send to the OS AutomationClient.
  // automation_client_remote_->Enable(
  //     base::BindOnce(&AutomationInternalBindings::OnTreeID,
  //     weak_ptr_factory_.GetWeakPtr()));

  // TODO(crbug.com/1357889): Need to figure out how to RespondLater to these
  // args in order to send the tree ID back to the JS caller as a callback.
}

void AutomationInternalBindings::Disable(gin::Arguments* args) {
  // TODO(crbug.com/1357889): Send to the OS AutomationClient.
  // automation_client_remote_->Disable();
  // Execute the return value on the args so that the JS callback
  // will execute.
  args->Return(true);
}

void AutomationInternalBindings::EnableTree(const ui::AXTreeID& tree_id) {
  // TODO(crbug.com/1357889): Send to the OS AutomationClient.
  // automation_client_remote_->EnableTree(tree_id);
}

void AutomationInternalBindings::PerformAction(
    const ui::AXActionData& action_data) {
  // TODO(crbug.com/1357889): Send to the OS AutomationClient.
  // automation_client_remote_->PerformAction(action_data);
}

}  // namespace ax
