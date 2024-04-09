// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/automation_internal_bindings.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "gin/function_template.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "services/accessibility/assistive_technology_controller_impl.h"
#include "services/accessibility/automation_impl.h"
#include "services/accessibility/features/bindings_isolate_holder.h"
#include "services/accessibility/features/v8_utils.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/mojom/ax_event.mojom.h"
#include "ui/accessibility/platform/automation/automation_api_util.h"
#include "ui/accessibility/platform/automation/automation_v8_router.h"
#include "v8-function.h"
#include "v8-value.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-template.h"

namespace ax {
namespace {

static const char kJSAutomationInternalV8Listeners[] =
    "automationInternalV8Listeners";
static const char kJSCallListeners[] = "callListeners";

}  // namespace
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
  // This task is posted because we need to wait for any pending mutations
  // to be processed before sending the event.
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  auto& main_runner = base::SequencedTaskRunner::GetCurrentDefault();

  // `this` is safe here because this object outlives
  // AutomationTreeManagerOwner, which in turn generates this kind of event.
  auto task = base::BindOnce(&AutomationInternalBindings::
                                 MaybeSendOnAllAutomationEventListenersRemoved,
                             base::Unretained(this));
  main_runner->PostTask(FROM_HERE, std::move(task));
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
  // TODO(b:327258691): Share const strings between c++ and js for event names.
  if (filter == "none") {
    return ui::TreeChangeObserverFilter::kNone;
  }
  if (filter == "noTreeChanges") {
    return ui::TreeChangeObserverFilter::kNoTreeChanges;
  }
  if (filter == "liveRegionTreeChanges") {
    return ui::TreeChangeObserverFilter::kLiveRegionTreeChanges;
  }
  if (filter == "textMarkerChanges") {
    return ui::TreeChangeObserverFilter::kTextMarkerChanges;
  }
  if (filter == "allTreeChanges") {
    return ui::TreeChangeObserverFilter::kAllTreeChanges;
  }

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
  // TODO(b:327258691): Share const strings between c++ and js for event names.
  switch (change_type) {
    case ax::mojom::Mutation::kNone:
      return "none";
    case ax::mojom::Mutation::kNodeCreated:
      return "nodeCreated";
    case ax::mojom::Mutation::kSubtreeCreated:
      return "subtreeCreated";
    case ax::mojom::Mutation::kNodeChanged:
      return "nodeChanged";
    case ax::mojom::Mutation::kTextChanged:
      return "textChanged";
    case ax::mojom::Mutation::kNodeRemoved:
      return "nodeRemoved";
    case ax::mojom::Mutation::kSubtreeUpdateEnd:
      return "subtreeUpdateEnd";
  }
}

std::string AutomationInternalBindings::GetEventTypeString(
    const std::tuple<ax::mojom::Event, ui::AXEventGenerator::Event>& event_type)
    const {
  // TODO(b:327258691): Share const strings between c++ and js for event names.
  const ui::AXEventGenerator::Event& generated_event = std::get<1>(event_type);

  // Resolve the proper event based on generated or non-generated event sources.
  if (generated_event != ui::AXEventGenerator::Event::NONE &&
      !ui::ShouldIgnoreGeneratedEventForAutomation(generated_event)) {
    return ui::ToString(generated_event);
  }

  const ax::mojom::Event& event = std::get<0>(event_type);
  if (event != ax::mojom::Event::kNone &&
      !ui::ShouldIgnoreAXEventForAutomation(event)) {
    return ui::ToString(event);
  }
  return "";
}

void AutomationInternalBindings::DispatchEvent(
    const std::string& event_name,
    const base::Value::List& event_args) const {
  v8::HandleScope handle_scope(GetIsolate());
  v8::Local<v8::Context> context = GetContext();
  v8::Context::Scope context_scope(context);

  // Here, a helper function defined in js is invoked to retrieve the event
  // object that contains all the listeners. Then, we dispatch the event to all
  // listeners.
  v8::Local<v8::String> func_name =
      v8::String::NewFromUtf8(GetIsolate(), kJSAutomationInternalV8Listeners)
          .ToLocalChecked();
  v8::Local<v8::Value> func_value =
      context->Global()->Get(context, func_name).ToLocalChecked();
  v8::Local<v8::Function> get_event_listeners_from_name_function =
      v8::Local<v8::Function>::Cast(func_value);

  // Prepare the argument for the 'getEventListenersFromName' call
  v8::Local<v8::Value> args[] = {
      v8::String::NewFromUtf8(GetIsolate(), event_name.c_str())
          .ToLocalChecked()};

  // Call the 'getEventListenersFromName' function using the event that needs to
  // be dispatched. The return value is an object containing all listeners to
  // that event.
  v8::Local<v8::Value> result = get_event_listeners_from_name_function
                                    ->Call(context, context->Global(), 1, args)
                                    .ToLocalChecked();
  CHECK(result->IsObject()) << "Unknown event type: " << event_name;

  v8::Local<v8::Object> event_listeners_object =
      v8::Local<v8::Object>::Cast(result);

  v8::Local<v8::String> method_name =
      v8::String::NewFromUtf8(GetIsolate(), kJSCallListeners).ToLocalChecked();
  v8::Local<v8::Value> method_value =
      event_listeners_object->Get(context, method_name).ToLocalChecked();
  v8::Local<v8::Function> method = v8::Local<v8::Function>::Cast(method_value);

  std::vector<v8::Local<v8::Value>> js_event_args;
  for (const base::Value& value : event_args) {
    v8::Local<v8::Value> js_event_arg =
        V8ValueConverter ::GetInstance()->ConvertToV8Value(value, context);
    js_event_args.push_back(js_event_arg);
  }

  v8::MaybeLocal<v8::Value> unused_result =
      method->Call(context, event_listeners_object, js_event_args.size(),
                   js_event_args.data());
  CHECK(!unused_result.IsEmpty());
}

}  // namespace ax
