// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/automation_internal_bindings.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
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
    mojo::PendingAssociatedReceiver<mojom::Automation> automation)
    : isolate_holder_(isolate_holder),
      automation_v8_bindings_(std::make_unique<ui::AutomationV8Bindings>(
          /*AutomationTreeManagerOwner=*/this,
          /*AutomationV8Router=*/this)) {
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

}  // namespace ax
