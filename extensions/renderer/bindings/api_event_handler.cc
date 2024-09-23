// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_event_handler.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/supports_user_data.h"
#include "base/values.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/renderer/bindings/api_response_validator.h"
#include "extensions/renderer/bindings/event_emitter.h"
#include "extensions/renderer/bindings/get_per_context_data.h"
#include "extensions/renderer/bindings/js_runner.h"
#include "gin/converter.h"
#include "gin/data_object_builder.h"
#include "gin/handle.h"
#include "gin/per_context_data.h"

namespace extensions {

namespace {

constexpr char kFilterIdKey[] = "filterId";
constexpr char kEventNameKey[] = "eventName";

struct APIEventPerContextData : public base::SupportsUserData::Data {
  static constexpr char kPerContextDataKey[] = "extension_api_events";

  APIEventPerContextData(v8::Isolate* isolate) : isolate(isolate) {}
  ~APIEventPerContextData() override {
    DCHECK(emitters.empty())
        << "|emitters| should have been cleared by InvalidateContext()";
    DCHECK(massagers.empty())
        << "|massagers| should have been cleared by InvalidateContext()";
    DCHECK(anonymous_emitters.empty())
        << "|anonymous_emitters| should have been cleared by "
        << "InvalidateContext()";
  }

  // The associated v8::Isolate. Since this object is cleaned up at context
  // destruction, this should always be valid.
  raw_ptr<v8::Isolate> isolate;

  // A map from event name -> event emitter.
  std::map<std::string, v8::Global<v8::Object>> emitters;

  // A map from event name -> argument massager.
  std::map<std::string, v8::Global<v8::Function>> massagers;

  // The collection of anonymous events.
  std::vector<v8::Global<v8::Object>> anonymous_emitters;

  static APIEventPerContextData* GetFrom(v8::Local<v8::Context> context,
                                         CreatePerContextData should_create) {
    return GetPerContextData<APIEventPerContextData>(context, should_create,
                                                     context->GetIsolate());
  }
};

constexpr char APIEventPerContextData::kPerContextDataKey[];

void DispatchEvent(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  if (info.Length() != 1 || !info[0]->IsArray()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  APIEventPerContextData* data =
      APIEventPerContextData::GetFrom(context, kDontCreateIfMissing);
  DCHECK(data);

  v8::Local<v8::Object> dispatch_data = info.Data().As<v8::Object>();
  v8::Local<v8::Value> filter_id_value =
      dispatch_data->Get(context, gin::StringToSymbol(isolate, kFilterIdKey))
          .ToLocalChecked();
  CHECK(filter_id_value->IsInt32());
  int filter_id = filter_id_value.As<v8::Int32>()->Value();

  v8::Local<v8::Value> event_name_value =
      dispatch_data->Get(context, gin::StringToSymbol(isolate, kEventNameKey))
          .ToLocalChecked();
  CHECK(event_name_value->IsString());
  v8::Local<v8::String> event_name_string = event_name_value.As<v8::String>();
  std::string event_name;
  gin::ConvertFromV8(isolate, event_name_string, &event_name);

  auto iter = data->emitters.find(event_name);
  if (iter == data->emitters.end()) {
    return;
  }
  v8::Global<v8::Object>& v8_emitter = iter->second;

  v8::LocalVector<v8::Value> args(isolate);
  CHECK(gin::Converter<v8::LocalVector<v8::Value>>::FromV8(isolate, info[0],
                                                           &args));

  EventEmitter* emitter = nullptr;
  gin::Converter<EventEmitter*>::FromV8(isolate, v8_emitter.Get(isolate),
                                        &emitter);
  CHECK(emitter);
  // Note: It's safe to use EventEmitter::FireSync() here because this should
  // only be triggered from a JS call, so we know JS is running.
  // TODO(devlin): It looks like the return result that requires this to be sync
  // is only used by the InputIME custom bindings; it would be kind of nice to
  // remove the dependency.
  mojom::EventFilteringInfoPtr filter = emitter->PopFilter(filter_id);
  info.GetReturnValue().Set(
      emitter->FireSync(context, &args, std::move(filter)));
}

}  // namespace

APIEventHandler::APIEventHandler(
    const APIEventListeners::ListenersUpdated& listeners_changed,
    const APIEventListeners::ContextOwnerIdGetter& context_owner_id_getter,
    ExceptionHandler* exception_handler)
    : listeners_changed_(listeners_changed),
      context_owner_id_getter_(context_owner_id_getter),
      exception_handler_(exception_handler) {}
APIEventHandler::~APIEventHandler() = default;

void APIEventHandler::SetResponseValidator(
    std::unique_ptr<APIResponseValidator> validator) {
  api_response_validator_ = std::move(validator);
}

v8::Local<v8::Object> APIEventHandler::CreateEventInstance(
    const std::string& event_name,
    bool supports_filters,
    bool supports_lazy_listeners,
    int max_listeners,
    bool notify_on_change,
    v8::Local<v8::Context> context) {
  // We need a context scope since gin::CreateHandle only takes the isolate
  // and infers the context from that.
  // TODO(devlin): This could be avoided if gin::CreateHandle could take a
  // context directly.
  v8::Context::Scope context_scope(context);

  APIEventPerContextData* data =
      APIEventPerContextData::GetFrom(context, kCreateIfMissing);
  DCHECK(data->emitters.find(event_name) == data->emitters.end());

  APIEventListeners::ListenersUpdated updated =
      notify_on_change ? listeners_changed_ : base::DoNothing();
  std::unique_ptr<APIEventListeners> listeners;
  if (supports_filters) {
    listeners = std::make_unique<FilteredEventListeners>(
        updated, event_name, context_owner_id_getter_, max_listeners,
        supports_lazy_listeners, &listener_tracker_);
  } else {
    listeners = std::make_unique<UnfilteredEventListeners>(
        updated, event_name, context_owner_id_getter_, max_listeners,
        supports_lazy_listeners, &listener_tracker_);
  }

  gin::Handle<EventEmitter> emitter_handle =
      gin::CreateHandle(context->GetIsolate(),
                        new EventEmitter(supports_filters, std::move(listeners),
                                         exception_handler_));
  CHECK(!emitter_handle.IsEmpty());
  v8::Local<v8::Value> emitter_value = emitter_handle.ToV8();
  CHECK(emitter_value->IsObject());
  v8::Local<v8::Object> emitter_object =
      v8::Local<v8::Object>::Cast(emitter_value);
  data->emitters[event_name] =
      v8::Global<v8::Object>(context->GetIsolate(), emitter_object);

  return emitter_object;
}

v8::Local<v8::Object> APIEventHandler::CreateAnonymousEventInstance(
    v8::Local<v8::Context> context) {
  v8::Context::Scope context_scope(context);
  APIEventPerContextData* data =
      APIEventPerContextData::GetFrom(context, kCreateIfMissing);
  bool supports_filters = false;

  // Anonymous events are not tracked, and thus don't need a name or a context
  // owner.
  std::string empty_event_name;
  ListenerTracker* anonymous_listener_tracker = nullptr;
  std::unique_ptr<APIEventListeners> listeners =
      std::make_unique<UnfilteredEventListeners>(
          base::DoNothing(), empty_event_name,
          APIEventListeners::ContextOwnerIdGetter(), binding::kNoListenerMax,
          false, anonymous_listener_tracker);
  gin::Handle<EventEmitter> emitter_handle =
      gin::CreateHandle(context->GetIsolate(),
                        new EventEmitter(supports_filters, std::move(listeners),
                                         exception_handler_));
  CHECK(!emitter_handle.IsEmpty());
  v8::Local<v8::Object> emitter_object = emitter_handle.ToV8().As<v8::Object>();
  data->anonymous_emitters.push_back(
      v8::Global<v8::Object>(context->GetIsolate(), emitter_object));
  return emitter_object;
}

void APIEventHandler::InvalidateCustomEvent(v8::Local<v8::Context> context,
                                            v8::Local<v8::Object> event) {
  EventEmitter* emitter = nullptr;
  APIEventPerContextData* data =
      APIEventPerContextData::GetFrom(context, kDontCreateIfMissing);
  // This could happen if a port (or JS) invalidates an event following
  // context destruction.
  // TODO(devlin): Is it better to fail gracefully here, or track all these
  // down for determinism?
  if (!data) {
    return;
  }

  if (!gin::Converter<EventEmitter*>::FromV8(context->GetIsolate(), event,
                                             &emitter)) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  emitter->Invalidate(context);
  auto emitter_entry = base::ranges::find(data->anonymous_emitters, event);
  if (emitter_entry == data->anonymous_emitters.end()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  data->anonymous_emitters.erase(emitter_entry);
}

void APIEventHandler::FireEventInContext(const std::string& event_name,
                                         v8::Local<v8::Context> context,
                                         const base::Value::List& args,
                                         mojom::EventFilteringInfoPtr filter) {
  // Don't bother converting arguments if there are no listeners.
  // NOTE(devlin): This causes a double data and EventEmitter lookup, since
  // the v8 version below also checks for listeners. This should be very cheap,
  // but if we were really worried we could refactor.
  if (!HasListenerForEvent(event_name, context)) {
    return;
  }

  // Note: since we only convert the arguments once, if a listener modifies an
  // object (including an array), other listeners will see that modification.
  // TODO(devlin): This is how it's always been, but should it be?
  std::unique_ptr<content::V8ValueConverter> converter =
      content::V8ValueConverter::Create();

  v8::LocalVector<v8::Value> v8_args(context->GetIsolate());
  v8_args.reserve(args.size());
  for (const auto& arg : args) {
    v8_args.push_back(converter->ToV8Value(arg, context));
  }

  FireEventInContext(event_name, context, &v8_args, std::move(filter),
                     JSRunner::ResultCallback());
}

void APIEventHandler::FireEventInContext(const std::string& event_name,
                                         v8::Local<v8::Context> context,
                                         v8::LocalVector<v8::Value>* arguments,
                                         mojom::EventFilteringInfoPtr filter,
                                         JSRunner::ResultCallback callback) {
  APIEventPerContextData* data =
      APIEventPerContextData::GetFrom(context, kDontCreateIfMissing);
  if (!data) {
    return;
  }

  auto iter = data->emitters.find(event_name);
  if (iter == data->emitters.end()) {
    return;
  }
  EventEmitter* emitter = nullptr;
  gin::Converter<EventEmitter*>::FromV8(
      context->GetIsolate(), iter->second.Get(context->GetIsolate()), &emitter);
  CHECK(emitter);

  auto massager_iter = data->massagers.find(event_name);
  if (massager_iter == data->massagers.end()) {
    // Validate the event arguments if there are no massagers (and validation is
    // enabled). Unfortunately, massagers both transform the event args from
    // unexpected -> expected and (badly!) from expected -> unexpected. As such,
    // we simply don't validate if there's a massager attached to the event.
    // TODO(crbug.com/40226845): Ideally, we'd be able to validate the response
    // after the massagers run. This requires fixing our schema for at least
    // chrome.permissions events.
    if (api_response_validator_) {
      api_response_validator_->ValidateEvent(context, event_name, *arguments);
    }

    emitter->Fire(context, arguments, std::move(filter), std::move(callback));
  } else {
    DCHECK(!callback) << "Can't use an event callback with argument massagers.";

    v8::Isolate* isolate = context->GetIsolate();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Function> massager = massager_iter->second.Get(isolate);

    v8::Local<v8::Array> args_array =
        v8::Array::New(isolate, arguments->size());
    {
      // Massagers expect an array of v8 values. Since this is a newly-
      // constructed array and we're assigning data properties, this shouldn't
      // be able to fail or be visible by other script.
      for (size_t i = 0; i < arguments->size(); ++i) {
        v8::Maybe<bool> success = args_array->CreateDataProperty(
            context, static_cast<uint32_t>(i), arguments->at(i));
        CHECK(success.ToChecked());
      }
    }

    // Curry in the native dispatch function. Some argument massagers take
    // extra liberties and call this asynchronously, so we can't just have the
    // massager return a modified array of arguments.
    // We don't store this in a template because the Data (event name) is
    // different for each instance. Luckily, this is called during dispatching
    // an event, rather than e.g. at initialization time.

    int filter_id = emitter->PushFilter(std::move(filter));

    v8::Local<v8::Object> dispatch_data =
        gin::DataObjectBuilder(isolate)
            .Set(kFilterIdKey, gin::ConvertToV8(isolate, filter_id))
            .Set(kEventNameKey, gin::ConvertToV8(isolate, event_name))
            .Build();

    v8::Local<v8::Function> dispatch_event =
        v8::Function::New(context, &DispatchEvent, std::move(dispatch_data))
            .ToLocalChecked();

    v8::Local<v8::Value> massager_args[] = {args_array, dispatch_event};
    JSRunner::Get(context)->RunJSFunction(
        massager, context, std::size(massager_args), massager_args);
  }
}

void APIEventHandler::RegisterArgumentMassager(
    v8::Local<v8::Context> context,
    const std::string& event_name,
    v8::Local<v8::Function> massager) {
  APIEventPerContextData* data =
      APIEventPerContextData::GetFrom(context, kCreateIfMissing);
  DCHECK(!base::Contains(data->massagers, event_name));
  data->massagers[event_name].Reset(context->GetIsolate(), massager);
}

bool APIEventHandler::HasListenerForEvent(const std::string& event_name,
                                          v8::Local<v8::Context> context) {
  APIEventPerContextData* data =
      APIEventPerContextData::GetFrom(context, kDontCreateIfMissing);
  if (!data) {
    return false;
  }

  auto iter = data->emitters.find(event_name);
  if (iter == data->emitters.end()) {
    return false;
  }
  EventEmitter* emitter = nullptr;
  gin::Converter<EventEmitter*>::FromV8(
      context->GetIsolate(), iter->second.Get(context->GetIsolate()), &emitter);
  CHECK(emitter);
  return emitter->GetNumListeners() > 0;
}

void APIEventHandler::InvalidateContext(v8::Local<v8::Context> context) {
  DCHECK(gin::PerContextData::From(context))
      << "Trying to invalidate an already-invalid context.";
  APIEventPerContextData* data =
      APIEventPerContextData::GetFrom(context, kDontCreateIfMissing);
  if (!data) {
    return;
  }

  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope scope(isolate);

  // This loop *shouldn't* allow any self-modification (i.e., no listeners
  // should be added or removed as a result of the iteration). If that changes,
  // we'll need to cache the listeners elsewhere before iterating.
  for (const auto& pair : data->emitters) {
    EventEmitter* emitter = nullptr;
    gin::Converter<EventEmitter*>::FromV8(isolate, pair.second.Get(isolate),
                                          &emitter);
    CHECK(emitter);
    emitter->Invalidate(context);
  }
  for (const auto& global : data->anonymous_emitters) {
    EventEmitter* emitter = nullptr;
    gin::Converter<EventEmitter*>::FromV8(isolate, global.Get(isolate),
                                          &emitter);
    CHECK(emitter);
    emitter->Invalidate(context);
  }

  data->emitters.clear();
  data->massagers.clear();
  data->anonymous_emitters.clear();

  // InvalidateContext() is called shortly (and, theoretically, synchronously)
  // before the PerContextData is deleted. We have a check that guarantees that
  // no new EventEmitters are created after the PerContextData is deleted, so
  // no new emitters should be created after this point.
}

size_t APIEventHandler::GetNumEventListenersForTesting(
    const std::string& event_name,
    v8::Local<v8::Context> context) {
  APIEventPerContextData* data =
      APIEventPerContextData::GetFrom(context, kDontCreateIfMissing);
  DCHECK(data);

  auto iter = data->emitters.find(event_name);
  CHECK(iter != data->emitters.end());
  EventEmitter* emitter = nullptr;
  gin::Converter<EventEmitter*>::FromV8(
      context->GetIsolate(), iter->second.Get(context->GetIsolate()), &emitter);
  CHECK(emitter);
  return emitter->GetNumListeners();
}

}  // namespace extensions
