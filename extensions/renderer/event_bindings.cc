// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/event_bindings.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/constants.h"
#include "extensions/common/event_filter.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_urls.h"
#include "extensions/renderer/event_bookkeeper.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// Returns the routing id to use for matching filtered events.
// Used for routing events to the correct RenderFrame. This doesn't apply to
// Extension Service Worker events as there is no RenderFrame to target an event
// to. This function returns MSG_ROUTING_NONE in that case,
// essentially ignoring routing id for worker events.
int GetRoutingIDForFilteredEvents(ScriptContext* script_context) {
  return script_context->context_type() == Feature::SERVICE_WORKER_CONTEXT
             ? MSG_ROUTING_NONE
             : script_context->GetRenderFrame()->GetRoutingID();
}

// Returns a v8::Array containing the ids of the listeners that match the given
// |event_filter_dict| in the given |script_context|.
v8::Local<v8::Array> GetMatchingListeners(ScriptContext* script_context,
                                          const std::string& event_name,
                                          const EventFilteringInfo& info) {
  const EventFilter& event_filter = EventBookkeeper::Get()->event_filter();
  v8::Isolate* isolate = script_context->isolate();
  v8::Local<v8::Context> context = script_context->v8_context();

  // Only match events routed to this context's RenderFrame or ones that don't
  // have a routingId in their filter.
  std::set<EventFilter::MatcherID> matched_event_filters =
      event_filter.MatchEvent(event_name, info,
                              GetRoutingIDForFilteredEvents(script_context));
  v8::Local<v8::Array> array(
      v8::Array::New(isolate, matched_event_filters.size()));
  int i = 0;
  for (EventFilter::MatcherID id : matched_event_filters) {
    CHECK(array->CreateDataProperty(context, i++, v8::Integer::New(isolate, id))
              .ToChecked());
  }

  return array;
}

bool IsLazyContext(ScriptContext* context) {
  // Note: Check context type first so that ExtensionFrameHelper isn't accessed
  // on a worker thread.
  return context->context_type() == Feature::SERVICE_WORKER_CONTEXT ||
         ExtensionFrameHelper::IsContextForEventPage(context);
}

}  // namespace

EventBindings::EventBindings(ScriptContext* context,
                             IPCMessageSender* ipc_message_sender)
    : ObjectBackedNativeHandler(context),
      ipc_message_sender_(ipc_message_sender) {
  // It's safe to use base::Unretained here because |context| will always
  // outlive us.
  context->AddInvalidationObserver(
      base::BindOnce(&EventBindings::OnInvalidated, base::Unretained(this)));
}

EventBindings::~EventBindings() {}

void EventBindings::AddRoutes() {
  RouteHandlerFunction(
      "AttachEvent",
      base::Bind(&EventBindings::AttachEventHandler, base::Unretained(this)));
  RouteHandlerFunction(
      "DetachEvent",
      base::Bind(&EventBindings::DetachEventHandler, base::Unretained(this)));
  RouteHandlerFunction(
      "AttachFilteredEvent",
      base::Bind(&EventBindings::AttachFilteredEvent, base::Unretained(this)));
  RouteHandlerFunction("DetachFilteredEvent",
                       base::Bind(&EventBindings::DetachFilteredEventHandler,
                                  base::Unretained(this)));
  RouteHandlerFunction(
      "AttachUnmanagedEvent",
      base::Bind(&EventBindings::AttachUnmanagedEvent, base::Unretained(this)));
  RouteHandlerFunction(
      "DetachUnmanagedEvent",
      base::Bind(&EventBindings::DetachUnmanagedEvent, base::Unretained(this)));
}

// static
void EventBindings::DispatchEventInContext(
    const std::string& event_name,
    const base::ListValue* event_args,
    const EventFilteringInfo* filtering_info,
    ScriptContext* context) {
  v8::HandleScope handle_scope(context->isolate());
  v8::Context::Scope context_scope(context->v8_context());

  v8::Local<v8::Array> listener_ids;
  if (filtering_info && !filtering_info->is_empty())
    listener_ids = GetMatchingListeners(context, event_name, *filtering_info);
  else
    listener_ids = v8::Array::New(context->isolate());

  v8::Local<v8::Value> v8_args[] = {
      gin::StringToSymbol(context->isolate(), event_name),
      content::V8ValueConverter::Create()->ToV8Value(event_args,
                                                     context->v8_context()),
      listener_ids,
  };

  context->module_system()->CallModuleMethodSafe(
      kEventBindings, "dispatchEvent", arraysize(v8_args), v8_args);
}

void EventBindings::AttachEventHandler(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsBoolean());
  AttachEvent(*v8::String::Utf8Value(args.GetIsolate(), args[0]),
              args[1].As<v8::Boolean>()->Value());
}

void EventBindings::AttachEvent(const std::string& event_name,
                                bool supports_lazy_listeners) {
  if (!context()->HasAccessOrThrowError(event_name))
    return;

  // Record the attachment for this context so that events can be detached when
  // the context is destroyed.
  //
  // Ideally we'd CHECK that it's not already attached, however that's not
  // possible because extensions can create and attach events themselves. Very
  // silly, but that's the way it is. For an example of this, see
  // chrome/test/data/extensions/api_test/events/background.js.
  attached_event_names_.insert(event_name);

  EventBookkeeper* bookkeeper = EventBookkeeper::Get();
  DCHECK(bookkeeper);
  if (bookkeeper->IncrementEventListenerCount(context(), event_name) == 1) {
    ipc_message_sender_->SendAddUnfilteredEventListenerIPC(context(),
                                                           event_name);
  }

  // This is called the first time the page has added a listener. Since
  // the background page is the only lazy page, we know this is the first
  // time this listener has been registered.
  if (IsLazyContext(context()) && supports_lazy_listeners) {
    ipc_message_sender_->SendAddUnfilteredLazyEventListenerIPC(context(),
                                                               event_name);
  }
}

void EventBindings::DetachEventHandler(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(3, args.Length());
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsBoolean());
  CHECK(args[2]->IsBoolean());
  bool was_manual = args[1].As<v8::Boolean>()->Value();
  bool supports_lazy_listeners = args[2].As<v8::Boolean>()->Value();
  DetachEvent(*v8::String::Utf8Value(args.GetIsolate(), args[0]),
              was_manual && supports_lazy_listeners);
}

void EventBindings::DetachEvent(const std::string& event_name,
                                bool remove_lazy_listener) {
  // See comment in AttachEvent().
  attached_event_names_.erase(event_name);

  EventBookkeeper* bookkeeper = EventBookkeeper::Get();
  DCHECK(bookkeeper);
  if (bookkeeper->DecrementEventListenerCount(context(), event_name) == 0) {
    ipc_message_sender_->SendRemoveUnfilteredEventListenerIPC(context(),
                                                              event_name);
  }

  // DetachEvent is called when the last listener for the context is
  // removed. If the context is the background page or service worker, and it
  // removes the last listener manually, then we assume that it is no longer
  // interested in being awakened for this event.
  if (remove_lazy_listener && IsLazyContext(context())) {
    ipc_message_sender_->SendRemoveUnfilteredLazyEventListenerIPC(context(),
                                                                  event_name);
  }
}

// MatcherID AttachFilteredEvent(string event_name, object filter)
// event_name - Name of the event to attach.
// filter - Which instances of the named event are we interested in.
// returns the id assigned to the listener, which will be provided to calls to
// dispatchEvent().
void EventBindings::AttachFilteredEvent(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(3, args.Length());
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsObject());
  CHECK(args[2]->IsBoolean());

  std::string event_name = *v8::String::Utf8Value(args.GetIsolate(), args[0]);
  if (!context()->HasAccessOrThrowError(event_name))
    return;

  std::unique_ptr<base::DictionaryValue> filter;
  {
    std::unique_ptr<base::Value> filter_value =
        content::V8ValueConverter::Create()->FromV8Value(
            v8::Local<v8::Object>::Cast(args[1]), context()->v8_context());
    if (!filter_value || !filter_value->is_dict()) {
      args.GetReturnValue().Set(static_cast<int32_t>(-1));
      return;
    }
    filter = base::DictionaryValue::From(std::move(filter_value));
  }

  bool supports_lazy_listeners = args[2].As<v8::Boolean>()->Value();

  EventBookkeeper* bookkeeper = EventBookkeeper::Get();
  DCHECK(bookkeeper);
  EventFilter& event_filter = bookkeeper->event_filter();
  int id = event_filter.AddEventMatcher(
      event_name,
      std::make_unique<EventMatcher>(std::move(filter),
                                     GetRoutingIDForFilteredEvents(context())));
  if (id == -1) {
    args.GetReturnValue().Set(static_cast<int32_t>(-1));
    return;
  }
  attached_matcher_ids_.insert(id);

  // Only send IPCs the first time a filter gets added.
  const EventMatcher* matcher = event_filter.GetEventMatcher(id);
  DCHECK(matcher);
  base::DictionaryValue* filter_weak = matcher->value();
  const ExtensionId& extension_id = context()->GetExtensionID();
  if (bookkeeper->AddFilter(event_name, extension_id, *filter_weak)) {
    bool lazy = supports_lazy_listeners && IsLazyContext(context());
    ipc_message_sender_->SendAddFilteredEventListenerIPC(context(), event_name,
                                                         *filter_weak, lazy);
  }

  args.GetReturnValue().Set(static_cast<int32_t>(id));
}

void EventBindings::DetachFilteredEventHandler(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(3, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsBoolean());
  CHECK(args[2]->IsBoolean());
  bool was_manual = args[1].As<v8::Boolean>()->Value();
  bool supports_lazy_listeners = args[2].As<v8::Boolean>()->Value();
  DetachFilteredEvent(args[0].As<v8::Int32>()->Value(),
                      was_manual && supports_lazy_listeners);
}

void EventBindings::DetachFilteredEvent(int matcher_id,
                                        bool remove_lazy_event) {
  EventBookkeeper* bookkeeper = EventBookkeeper::Get();
  DCHECK(bookkeeper);
  EventFilter& event_filter = bookkeeper->event_filter();
  EventMatcher* event_matcher = event_filter.GetEventMatcher(matcher_id);

  const std::string& event_name = event_filter.GetEventName(matcher_id);

  // Only send IPCs the last time a filter gets removed.
  const ExtensionId& extension_id = context()->GetExtensionID();
  if (bookkeeper->RemoveFilter(event_name, extension_id,
                               event_matcher->value())) {
    bool remove_lazy = remove_lazy_event && IsLazyContext(context());
    ipc_message_sender_->SendRemoveFilteredEventListenerIPC(
        context(), event_name, *event_matcher->value(), remove_lazy);
  }

  event_filter.RemoveEventMatcher(matcher_id);
  attached_matcher_ids_.erase(matcher_id);
}

void EventBindings::AttachUnmanagedEvent(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsString());
  std::string event_name = gin::V8ToString(isolate, args[0]);
  EventBookkeeper::Get()->AddUnmanagedEvent(context(), event_name);
}

void EventBindings::DetachUnmanagedEvent(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsString());
  std::string event_name = gin::V8ToString(isolate, args[0]);
  EventBookkeeper::Get()->RemoveUnmanagedEvent(context(), event_name);
}

void EventBindings::OnInvalidated() {
  // Detach all attached events that weren't attached. Iterate over a copy
  // because it will be mutated.
  std::set<std::string> attached_event_names_safe = attached_event_names_;
  for (const std::string& event_name : attached_event_names_safe) {
    DetachEvent(event_name, false /* is_manual */);
  }
  DCHECK(attached_event_names_.empty())
      << "Events cannot be attached during invalidation";

  // Same for filtered events.
  std::set<int> attached_matcher_ids_safe = attached_matcher_ids_;
  for (int matcher_id : attached_matcher_ids_safe) {
    DetachFilteredEvent(matcher_id, false /* is_manual */);
  }
  DCHECK(attached_matcher_ids_.empty())
      << "Filtered events cannot be attached during invalidation";

  EventBookkeeper::Get()->RemoveAllUnmanagedListeners(context());
}

}  // namespace extensions
