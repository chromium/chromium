// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_bindings_system.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/values.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/renderer/bindings/api_binding_hooks.h"
#include "extensions/renderer/bindings/api_binding_hooks_delegate.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/api_response_validator.h"
#include "extensions/renderer/bindings/interaction_provider.h"

namespace extensions {

APIBindingsSystem::APIBindingsSystem(
    GetAPISchemaMethod get_api_schema,
    BindingAccessChecker::APIAvailabilityCallback api_available,
    BindingAccessChecker::PromiseAvailabilityCallback promises_available,
    APIRequestHandler::SendRequestMethod send_request,
    std::unique_ptr<InteractionProvider> interaction_provider,
    APIEventListeners::ListenersUpdated event_listeners_changed,
    APIEventHandler::ContextOwnerIdGetter context_owner_getter,
    APIBinding::OnSilentRequest on_silent_request,
    binding::AddConsoleError add_console_error,
    APILastError last_error)
    : type_reference_map_(
          base::BindRepeating(&APIBindingsSystem::InitializeType,
                              base::Unretained(this))),
      exception_handler_(std::move(add_console_error)),
      interaction_provider_(std::move(interaction_provider)),
      request_handler_(std::move(send_request),
                       std::move(last_error),
                       &exception_handler_,
                       interaction_provider_.get()),
      event_handler_(std::move(event_listeners_changed),
                     std::move(context_owner_getter),
                     &exception_handler_),
      access_checker_(std::move(api_available), std::move(promises_available)),
      get_api_schema_(std::move(get_api_schema)),
      on_silent_request_(std::move(on_silent_request)) {
  if (binding::IsResponseValidationEnabled()) {
    request_handler_.SetResponseValidator(
        std::make_unique<APIResponseValidator>(&type_reference_map_));
    event_handler_.SetResponseValidator(
        std::make_unique<APIResponseValidator>(&type_reference_map_));
  }
}

APIBindingsSystem::~APIBindingsSystem() = default;

v8::Local<v8::Object> APIBindingsSystem::CreateAPIInstance(
    const std::string& api_name,
    v8::Local<v8::Context> context,
    APIBindingHooks** hooks_out) {
  std::unique_ptr<APIBinding>& binding = api_bindings_[api_name];
  if (!binding)
    binding = CreateNewAPIBinding(api_name);
  if (hooks_out)
    *hooks_out = binding->hooks();
  return binding->CreateInstance(context);
}

std::unique_ptr<APIBinding> APIBindingsSystem::CreateNewAPIBinding(
    const std::string& api_name) {
  const base::Value::Dict& api_schema = get_api_schema_.Run(api_name);

  const base::Value::List* function_definitions =
      api_schema.FindList("functions");
  const base::Value::List* type_definitions = api_schema.FindList("types");
  const base::Value::List* event_definitions = api_schema.FindList("events");
  const base::Value::Dict* property_definitions =
      api_schema.FindDict("properties");

  // Find the hooks for the API. If none exist, an empty set will be created so
  // we can use JS custom bindings.
  // TODO(devlin): Once all legacy custom bindings are converted, we don't have
  // to unconditionally pass in binding hooks.
  std::unique_ptr<APIBindingHooks> hooks;
  auto iter = binding_hooks_.find(api_name);
  if (iter != binding_hooks_.end()) {
    hooks = std::move(iter->second);
    binding_hooks_.erase(iter);
  } else {
    hooks = std::make_unique<APIBindingHooks>(api_name, &request_handler_);
  }

  return std::make_unique<APIBinding>(
      api_name, function_definitions, type_definitions, event_definitions,
      property_definitions,
      base::BindRepeating(&APIBindingsSystem::CreateCustomType,
                          base::Unretained(this)),
      on_silent_request_, std::move(hooks), &type_reference_map_,
      &request_handler_, &event_handler_, &access_checker_);
}

void APIBindingsSystem::InitializeType(const std::string& type_name) {
  // In order to initialize the type, we just initialize the full binding. This
  // seems like a lot of work, but in practice, trying to extract out only the
  // types from the schema, and then update the reference map based on that, is
  // close enough to the same cost. Additionally, this happens lazily on API
  // use, and relatively few APIs specify types from another API. Finally, this
  // will also go away if/when we generate all these specifications.
  std::string::size_type dot = type_name.rfind('.');
  // The type name should be fully qualified (include the API name).
  DCHECK_NE(std::string::npos, dot) << type_name;
  DCHECK_LT(dot, type_name.size() - 1);
  std::string api_name = type_name.substr(0, dot);
  // If we've already instantiated the binding, the type should have been in
  // there.
  DCHECK(!base::Contains(api_bindings_, api_name)) << api_name;

  api_bindings_[api_name] = CreateNewAPIBinding(api_name);
}

void APIBindingsSystem::CompleteRequest(
    int request_id,
    const base::Value::List& response,
    const std::string& error,
    mojom::ExtraResponseDataPtr extra_data) {
  request_handler_.CompleteRequest(request_id, response, error,
                                   std::move(extra_data));
}

void APIBindingsSystem::FireEventInContext(
    const std::string& event_name,
    v8::Local<v8::Context> context,
    const base::Value::List& response,
    mojom::EventFilteringInfoPtr filter) {
  event_handler_.FireEventInContext(event_name, context, response,
                                    std::move(filter));
}

void APIBindingsSystem::RegisterHooksDelegate(
    const std::string& api_name,
    std::unique_ptr<APIBindingHooksDelegate> delegate) {
  DCHECK(api_bindings_.empty())
      << "Hook registration must happen before creating any binding instances.";
  std::unique_ptr<APIBindingHooks>& hooks = binding_hooks_[api_name];
  if (!hooks) {
    hooks = std::make_unique<APIBindingHooks>(api_name, &request_handler_);
  }
  hooks->SetDelegate(std::move(delegate));
}

void APIBindingsSystem::RegisterCustomType(const std::string& type_name,
                                           CustomTypeHandler function) {
  DCHECK(!base::Contains(custom_types_, type_name))
      << "Custom type already registered: " << type_name;
  custom_types_[type_name] = std::move(function);
}

void APIBindingsSystem::WillReleaseContext(v8::Local<v8::Context> context) {
  binding::InvalidateContext(context);
  request_handler_.InvalidateContext(context);
  event_handler_.InvalidateContext(context);
}

v8::Local<v8::Object> APIBindingsSystem::CreateCustomType(
    v8::Isolate* isolate,
    const std::string& type_name,
    const std::string& property_name,
    const base::Value::List* property_values) {
  auto iter = custom_types_.find(type_name);
  CHECK(iter != custom_types_.end(), base::NotFatalUntil::M130)
      << "Custom type not found: " << type_name;
  return iter->second.Run(isolate, property_name, property_values,
                          &request_handler_, &event_handler_,
                          &type_reference_map_, &access_checker_);
}

}  // namespace extensions
