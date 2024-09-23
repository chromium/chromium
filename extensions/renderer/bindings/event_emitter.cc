// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/event_emitter.h"

#include <algorithm>
#include <utility>

#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/api_event_listeners.h"
#include "extensions/renderer/bindings/exception_handler.h"
#include "gin/data_object_builder.h"
#include "gin/object_template_builder.h"
#include "gin/per_context_data.h"

namespace extensions {

namespace {

constexpr const char kEmitterKey[] = "emitter";
constexpr const char kArgumentsKey[] = "arguments";
constexpr const char kFilterKey[] = "filter";
constexpr const char kEventEmitterTypeName[] = "Event";

}  // namespace

gin::WrapperInfo EventEmitter::kWrapperInfo = {gin::kEmbedderNativeGin};

EventEmitter::EventEmitter(bool supports_filters,
                           std::unique_ptr<APIEventListeners> listeners,
                           ExceptionHandler* exception_handler)
    : supports_filters_(supports_filters),
      listeners_(std::move(listeners)),
      exception_handler_(exception_handler) {}

EventEmitter::~EventEmitter() = default;

gin::ObjectTemplateBuilder EventEmitter::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<EventEmitter>::GetObjectTemplateBuilder(isolate)
      .SetMethod("addListener", &EventEmitter::AddListener)
      .SetMethod("removeListener", &EventEmitter::RemoveListener)
      .SetMethod("hasListener", &EventEmitter::HasListener)
      .SetMethod("hasListeners", &EventEmitter::HasListeners)
      // The following methods aren't part of the public API, but are used
      // by our custom bindings and exposed on the public event object. :(
      // TODO(devlin): Once we convert all custom bindings that use these,
      // they can be removed.
      .SetMethod("dispatch", &EventEmitter::Dispatch);
}

const char* EventEmitter::GetTypeName() {
  return kEventEmitterTypeName;
}

void EventEmitter::Fire(v8::Local<v8::Context> context,
                        v8::LocalVector<v8::Value>* args,
                        mojom::EventFilteringInfoPtr filter,
                        JSRunner::ResultCallback callback) {
  DispatchAsync(context, args, std::move(filter), std::move(callback));
}

v8::Local<v8::Value> EventEmitter::FireSync(
    v8::Local<v8::Context> context,
    v8::LocalVector<v8::Value>* args,
    mojom::EventFilteringInfoPtr filter) {
  DCHECK(context == context->GetIsolate()->GetCurrentContext());
  return DispatchSync(context, args, std::move(filter));
}

void EventEmitter::Invalidate(v8::Local<v8::Context> context) {
  valid_ = false;
  listeners_->Invalidate(context);
}

size_t EventEmitter::GetNumListeners() const {
  return listeners_->GetNumListeners();
}

int EventEmitter::PushFilter(mojom::EventFilteringInfoPtr filter) {
  // In order to dispatch (potentially) asynchronously (such as when script is
  // suspended), use a helper function to run once JS is allowed to run,
  // currying in the necessary information about the arguments and filter.
  // We do this (rather than simply queuing up each listener and running them
  // asynchronously) for a few reasons:
  // - It allows us to catch exceptions when the listener is running.
  // - Listeners could be removed between the time the event is received and the
  //   listeners are notified.
  // - It allows us to group the listeners responses.

  // We always set a filter id (rather than leaving filter undefined in the
  // case of no filter being present) to avoid ever hitting the Object prototype
  // chain when checking for it on the data value in DispatchAsyncHelper().
  int filter_id = kInvalidFilterId;

  if (filter) {
    filter_id = next_filter_id_++;
    pending_filters_[filter_id] = std::move(filter);
  }

  return filter_id;
}

mojom::EventFilteringInfoPtr EventEmitter::PopFilter(int filter_id) {
  mojom::EventFilteringInfoPtr filter;

  if (filter_id == kInvalidFilterId) {
    return filter;
  }

  auto filter_iter = pending_filters_.find(filter_id);
  CHECK(filter_iter != pending_filters_.end());
  filter = std::move(filter_iter->second);
  pending_filters_.erase(filter_iter);

  return filter;
}

void EventEmitter::AddListener(gin::Arguments* arguments) {
  // If script from another context maintains a reference to this object, it's
  // possible that functions can be called after this object's owning context
  // is torn down and released by blink. We don't support this behavior, but
  // we need to make sure nothing crashes, so early out of methods.
  if (!valid_) {
    return;
  }

  v8::Local<v8::Function> listener;
  // TODO(devlin): For some reason, we don't throw an error when someone calls
  // add/removeListener with no argument. We probably should. For now, keep
  // the status quo, but we should revisit this.
  if (!arguments->GetNext(&listener)) {
    return;
  }

  if (!arguments->PeekNext().IsEmpty() && !supports_filters_) {
    arguments->ThrowTypeError("This event does not support filters");
    return;
  }

  v8::Local<v8::Object> filter;
  if (!arguments->PeekNext().IsEmpty() && !arguments->GetNext(&filter)) {
    arguments->ThrowTypeError("Invalid invocation");
    return;
  }

  v8::Local<v8::Context> context = arguments->GetHolderCreationContext();
  if (!gin::PerContextData::From(context)) {
    return;
  }

  std::string error;
  if (!listeners_->AddListener(listener, filter, context, &error) &&
      !error.empty()) {
    arguments->ThrowTypeError(error);
  }
}

void EventEmitter::RemoveListener(gin::Arguments* arguments) {
  // See comment in AddListener().
  if (!valid_) {
    return;
  }

  v8::Local<v8::Function> listener;
  // See comment in AddListener().
  if (!arguments->GetNext(&listener)) {
    return;
  }

  listeners_->RemoveListener(listener, arguments->GetHolderCreationContext());
}

bool EventEmitter::HasListener(v8::Local<v8::Function> listener) {
  return listeners_->HasListener(listener);
}

bool EventEmitter::HasListeners() {
  return listeners_->GetNumListeners() != 0;
}

void EventEmitter::Dispatch(gin::Arguments* arguments) {
  if (!valid_) {
    return;
  }

  if (listeners_->GetNumListeners() == 0) {
    return;
  }

  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::LocalVector<v8::Value> v8_args = arguments->GetAll();

  // Since this is directly from JS, we know it should be safe to call
  // synchronously and use the return result, so we don't use Fire().
  arguments->Return(DispatchSync(context, &v8_args, nullptr));
}

v8::Local<v8::Value> EventEmitter::DispatchSync(
    v8::Local<v8::Context> context,
    v8::LocalVector<v8::Value>* args,
    mojom::EventFilteringInfoPtr filter) {
  // Note that |listeners_| can be modified during handling.
  v8::LocalVector<v8::Function> listeners =
      listeners_->GetListeners(std::move(filter), context);

  JSRunner* js_runner = JSRunner::Get(context);
  v8::Isolate* isolate = context->GetIsolate();
  DCHECK(context == isolate->GetCurrentContext());

  // Gather results from each listener as we go along. This should only be
  // called when running synchronous script is allowed, and some callers
  // expect a return value of an array with entries for each of the results of
  // the listeners.
  // TODO(devlin): It'd be nice to refactor anything expecting a result here so
  // we don't have to have this special logic, especially since script could
  // potentially tweak the result object through prototype manipulation (which
  // also means we should never use this for security decisions).
  v8::Local<v8::Array> results = v8::Array::New(isolate);
  uint32_t results_index = 0;

  v8::TryCatch try_catch(isolate);
  for (const auto& listener : listeners) {
    // NOTE(devlin): Technically, any listener here could suspend JS execution
    // (through e.g. calling alert() or print()). That should suspend this
    // message loop as well (though a nested message loop will run). This is a
    // bit ugly, but should hopefully be safe.
    v8::MaybeLocal<v8::Value> maybe_result = js_runner->RunJSFunctionSync(
        listener, context, args->size(), args->data());

    // Any of the listeners could invalidate the context. If that happens,
    // bail out.
    if (!binding::IsContextValid(context)) {
      return v8::Undefined(isolate);
    }

    v8::Local<v8::Value> listener_result;
    if (maybe_result.ToLocal(&listener_result)) {
      if (!listener_result->IsUndefined()) {
        CHECK(
            results
                ->CreateDataProperty(context, results_index++, listener_result)
                .ToChecked());
      }
    } else {
      DCHECK(try_catch.HasCaught());
      exception_handler_->HandleException(context, "Error in event handler",
                                          &try_catch);
      try_catch.Reset();
    }
  }

  // Only return a value if there's at least one response. This is the behavior
  // of the current JS implementation.
  v8::Local<v8::Value> return_value;
  if (results_index > 0) {
    return_value = gin::DataObjectBuilder(isolate)
                       .Set("results", results.As<v8::Value>())
                       .Build();
  } else {
    return_value = v8::Undefined(isolate);
  }

  return return_value;
}

void EventEmitter::DispatchAsync(v8::Local<v8::Context> context,
                                 v8::LocalVector<v8::Value>* args,
                                 mojom::EventFilteringInfoPtr filter,
                                 JSRunner::ResultCallback callback) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context);

  int filter_id = PushFilter(std::move(filter));

  v8::Local<v8::Array> args_array = v8::Array::New(isolate, args->size());
  for (size_t i = 0; i < args->size(); ++i) {
    CHECK(args_array->CreateDataProperty(context, i, args->at(i)).ToChecked());
  }

  v8::Local<v8::Object> data =
      gin::DataObjectBuilder(isolate)
          .Set(kEmitterKey, GetWrapper(isolate).ToLocalChecked())
          .Set(kArgumentsKey, args_array.As<v8::Value>())
          .Set(kFilterKey, gin::ConvertToV8(isolate, filter_id))
          .Build();
  v8::Local<v8::Function> function;
  // TODO(devlin): Function construction can fail in some weird cases (looking
  // up the "prototype" property on parents, failing to instantiate properties
  // on the function, etc). In *theory*, none of those apply here. Leave this as
  // a CHECK for now to flush out any cases.
  CHECK(v8::Function::New(context, &DispatchAsyncHelper, data)
            .ToLocal(&function));

  JSRunner::Get(context)->RunJSFunction(function, context, 0, nullptr,
                                        std::move(callback));
}

// static
void EventEmitter::DispatchAsyncHelper(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (!binding::IsContextValid(context)) {
    return;
  }

  v8::Local<v8::Object> data = info.Data().As<v8::Object>();

  v8::Local<v8::Value> emitter_value =
      data->Get(context, gin::StringToSymbol(isolate, kEmitterKey))
          .ToLocalChecked();
  EventEmitter* emitter = nullptr;
  gin::Converter<EventEmitter*>::FromV8(isolate, emitter_value, &emitter);
  DCHECK(emitter);

  v8::Local<v8::Value> filter_id_value =
      data->Get(context, gin::StringToSymbol(isolate, kFilterKey))
          .ToLocalChecked();
  int filter_id = filter_id_value.As<v8::Int32>()->Value();
  mojom::EventFilteringInfoPtr filter = emitter->PopFilter(filter_id);

  v8::Local<v8::Value> arguments_value =
      data->Get(context, gin::StringToSymbol(isolate, kArgumentsKey))
          .ToLocalChecked();
  DCHECK(arguments_value->IsArray());
  v8::Local<v8::Array> arguments_array = arguments_value.As<v8::Array>();
  v8::LocalVector<v8::Value> arguments(isolate);
  uint32_t arguments_count = arguments_array->Length();
  arguments.reserve(arguments_count);
  for (uint32_t i = 0; i < arguments_count; ++i) {
    arguments.push_back(arguments_array->Get(context, i).ToLocalChecked());
  }

  // We know that dispatching synchronously should be safe because this function
  // was triggered by JS execution.
  info.GetReturnValue().Set(
      emitter->DispatchSync(context, &arguments, std::move(filter)));
}

}  // namespace extensions
