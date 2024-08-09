// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/idl_member_installer.h"

#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"

namespace blink {

namespace bindings {

namespace {

template <typename Config>
bool DoesWorldMatch(const Config& config, const DOMWrapperWorld& world) {
  const unsigned world_bit = static_cast<unsigned>(
      world.IsMainWorld() ? IDLMemberInstaller::FlagWorld::kMainWorld
                          : IDLMemberInstaller::FlagWorld::kNonMainWorlds);
  return config.world & world_bit;
}

template <v8::ExceptionContext kind, typename Config>
v8::FunctionCallback GetConfigCallback(const Config& config);
template <>
v8::FunctionCallback GetConfigCallback<v8::ExceptionContext::kAttributeGet>(
    const IDLMemberInstaller::AttributeConfig& config) {
  return config.callback_for_get;
}
template <>
v8::FunctionCallback GetConfigCallback<v8::ExceptionContext::kAttributeSet>(
    const IDLMemberInstaller::AttributeConfig& config) {
  return config.callback_for_set;
}
template <>
v8::FunctionCallback GetConfigCallback<v8::ExceptionContext::kOperation>(
    const IDLMemberInstaller::OperationConfig& config) {
  return config.callback;
}

template <v8::ExceptionContext kind, typename Config>
int GetConfigLength(const Config& config);
template <>
int GetConfigLength<v8::ExceptionContext::kAttributeGet>(
    const IDLMemberInstaller::AttributeConfig& config) {
  return 0;
}
template <>
int GetConfigLength<v8::ExceptionContext::kAttributeSet>(
    const IDLMemberInstaller::AttributeConfig& config) {
  return 1;
}
template <>
int GetConfigLength<v8::ExceptionContext::kOperation>(
    const IDLMemberInstaller::OperationConfig& config) {
  return config.length;
}

template <v8::ExceptionContext kind, typename Config>
IDLMemberInstaller::FlagCrossOriginCheck GetConfigCrossOriginCheck(
    const Config& config);
template <>
IDLMemberInstaller::FlagCrossOriginCheck
GetConfigCrossOriginCheck<v8::ExceptionContext::kAttributeGet>(
    const IDLMemberInstaller::AttributeConfig& config) {
  return static_cast<IDLMemberInstaller::FlagCrossOriginCheck>(
      config.cross_origin_check_for_get);
}
template <>
IDLMemberInstaller::FlagCrossOriginCheck
GetConfigCrossOriginCheck<v8::ExceptionContext::kAttributeSet>(
    const IDLMemberInstaller::AttributeConfig& config) {
  return static_cast<IDLMemberInstaller::FlagCrossOriginCheck>(
      config.cross_origin_check_for_set);
}
template <>
IDLMemberInstaller::FlagCrossOriginCheck
GetConfigCrossOriginCheck<v8::ExceptionContext::kOperation>(
    const IDLMemberInstaller::OperationConfig& config) {
  return static_cast<IDLMemberInstaller::FlagCrossOriginCheck>(
      config.cross_origin_check);
}

template <v8::ExceptionContext kind, typename Config>
v8::SideEffectType GetConfigSideEffect(const Config& config);
template <>
v8::SideEffectType GetConfigSideEffect<v8::ExceptionContext::kAttributeGet>(
    const IDLMemberInstaller::AttributeConfig& config) {
  return static_cast<v8::SideEffectType>(config.v8_side_effect);
}
template <>
v8::SideEffectType GetConfigSideEffect<v8::ExceptionContext::kAttributeSet>(
    const IDLMemberInstaller::AttributeConfig& config) {
  return v8::SideEffectType::kHasSideEffect;
}
template <>
v8::SideEffectType GetConfigSideEffect<v8::ExceptionContext::kOperation>(
    const IDLMemberInstaller::OperationConfig& config) {
  return static_cast<v8::SideEffectType>(config.v8_side_effect);
}

template <v8::ExceptionContext kind, typename Config>
V8PrivateProperty::CachedAccessor GetConfigV8CachedAccessor(
    const Config& config);
template <>
V8PrivateProperty::CachedAccessor
GetConfigV8CachedAccessor<v8::ExceptionContext::kAttributeGet>(
    const IDLMemberInstaller::AttributeConfig& config) {
  return static_cast<V8PrivateProperty::CachedAccessor>(
      config.v8_cached_accessor);
}
template <>
V8PrivateProperty::CachedAccessor
GetConfigV8CachedAccessor<v8::ExceptionContext::kAttributeSet>(
    const IDLMemberInstaller::AttributeConfig& config) {
  return V8PrivateProperty::CachedAccessor::kNone;
}
template <>
V8PrivateProperty::CachedAccessor
GetConfigV8CachedAccessor<v8::ExceptionContext::kOperation>(
    const IDLMemberInstaller::OperationConfig& config) {
  return V8PrivateProperty::CachedAccessor::kNone;
}

template <v8::ExceptionContext kind, typename Config>
v8::Local<v8::FunctionTemplate> CreateFunctionTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Signature> signature,
    v8::Local<v8::String> property_name,
    v8::Local<v8::String> interface_name,
    v8::ExceptionContext exception_context,
    const Config& config,
    const v8::CFunction* v8_cfunction_table_data = nullptr,
    uint32_t v8_cfunction_table_size = 0) {
  v8::FunctionCallback callback = GetConfigCallback<kind>(config);
  if (!callback)
    return v8::Local<v8::FunctionTemplate>();

  int length = GetConfigLength<kind>(config);
  v8::SideEffectType v8_side_effect = GetConfigSideEffect<kind>(config);
  V8PrivateProperty::CachedAccessor v8_cached_accessor =
      GetConfigV8CachedAccessor<kind>(config);

  v8::Local<v8::FunctionTemplate> function_template;
  if (v8_cached_accessor == V8PrivateProperty::CachedAccessor::kNone ||
      (v8_cached_accessor ==
           V8PrivateProperty::CachedAccessor::kWindowDocument &&
       !world.IsMainWorld())) {
    function_template = v8::FunctionTemplate::NewWithCFunctionOverloads(
        isolate, callback, v8::Local<v8::Value>(), signature, length,
        v8::ConstructorBehavior::kThrow, v8_side_effect,
        {v8_cfunction_table_data, v8_cfunction_table_size});
  } else {
    DCHECK(!v8_cfunction_table_data);
    DCHECK_EQ(v8_cfunction_table_size, 0u);
    function_template = v8::FunctionTemplate::NewWithCache(
        isolate, callback,
        V8PrivateProperty::GetCachedAccessor(isolate, v8_cached_accessor)
            .GetPrivate(),
        v8::Local<v8::Value>(), signature, length, v8_side_effect);
    function_template->RemovePrototype();
  }

  function_template->SetClassName(property_name);
  function_template->SetInterfaceName(interface_name);
  function_template->SetExceptionContext(kind);

  function_template->SetAcceptAnyReceiver(
      GetConfigCrossOriginCheck<kind>(config) ==
      IDLMemberInstaller::FlagCrossOriginCheck::kDoNotCheck);

  return function_template;
}

template <v8::ExceptionContext kind, typename Config>
v8::Local<v8::Function> CreateFunction(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    const DOMWrapperWorld& world,
    v8::Local<v8::Signature> signature,
    v8::Local<v8::String> property_name,
    v8::Local<v8::String> interface_name,
    v8::ExceptionContext exception_context,
    const Config& config,
    const v8::CFunction* v8_cfunction_table_data = nullptr,
    uint32_t v8_cfunction_table_size = 0) {
  if (!GetConfigCallback<kind>(config))
    return v8::Local<v8::Function>();

  return CreateFunctionTemplate<kind>(isolate, world, signature, property_name,
                                      interface_name, exception_context, config,
                                      v8_cfunction_table_data,
                                      v8_cfunction_table_size)
      ->GetFunction(context)
      .ToLocalChecked();
}

void InstallAttribute(v8::Isolate* isolate,
                      const DOMWrapperWorld& world,
                      v8::Local<v8::Template> instance_template,
                      v8::Local<v8::Template> prototype_template,
                      v8::Local<v8::Template> interface_template,
                      v8::Local<v8::Signature> signature,
                      const IDLMemberInstaller::AttributeConfig& config) {
  if (!DoesWorldMatch(config, world))
    return;

  IDLMemberInstaller::FlagLocation location =
      static_cast<IDLMemberInstaller::FlagLocation>(config.location);
  if (static_cast<IDLMemberInstaller::FlagReceiverCheck>(
          config.receiver_check) ==
          IDLMemberInstaller::FlagReceiverCheck::kDoNotCheck ||
      location == IDLMemberInstaller::FlagLocation::kInterface)
    signature = v8::Local<v8::Signature>();

  StringView property_name_as_view(config.property_name);
  v8::Local<v8::String> property_name =
      V8AtomicString(isolate, property_name_as_view);
  v8::Local<v8::String> interface_name =
      V8AtomicString(isolate, config.interface_name);
  v8::Local<v8::String> get_name = V8AtomicString(
      isolate,
      static_cast<String>(StringView("get ", 4) + property_name_as_view));
  v8::Local<v8::String> set_name = V8AtomicString(
      isolate,
      static_cast<String>(StringView("set ", 4) + property_name_as_view));
  v8::Local<v8::FunctionTemplate> get_func =
      CreateFunctionTemplate<v8::ExceptionContext::kAttributeGet>(
          isolate, world, signature, get_name, interface_name,
          v8::ExceptionContext::kAttributeGet, config);
  v8::Local<v8::FunctionTemplate> set_func =
      CreateFunctionTemplate<v8::ExceptionContext::kAttributeSet>(
          isolate, world, signature, set_name, interface_name,
          v8::ExceptionContext::kAttributeSet, config);

  v8::Local<v8::Template> target_template;
  switch (location) {
    case IDLMemberInstaller::FlagLocation::kInstance:
      target_template = instance_template;
      break;
    case IDLMemberInstaller::FlagLocation::kPrototype:
      target_template = prototype_template;
      break;
    case IDLMemberInstaller::FlagLocation::kInterface:
      target_template = interface_template;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  target_template->SetAccessorProperty(
      property_name, get_func, set_func,
      static_cast<v8::PropertyAttribute>(config.v8_property_attribute));
}

void InstallAttribute(v8::Isolate* isolate,
                      v8::Local<v8::Context> context,
                      const DOMWrapperWorld& world,
                      v8::Local<v8::Object> instance_object,
                      v8::Local<v8::Object> prototype_object,
                      v8::Local<v8::Object> interface_object,
                      v8::Local<v8::Signature> signature,
                      const IDLMemberInstaller::AttributeConfig& config) {
  if (!DoesWorldMatch(config, world))
    return;

  IDLMemberInstaller::FlagLocation location =
      static_cast<IDLMemberInstaller::FlagLocation>(config.location);
  if (static_cast<IDLMemberInstaller::FlagReceiverCheck>(
          config.receiver_check) ==
          IDLMemberInstaller::FlagReceiverCheck::kDoNotCheck ||
      location == IDLMemberInstaller::FlagLocation::kInterface)
    signature = v8::Local<v8::Signature>();

  StringView name_as_view(config.property_name);
  v8::Local<v8::String> property_name = V8AtomicString(isolate, name_as_view);
  v8::Local<v8::String> interface_name =
      V8AtomicString(isolate, config.interface_name);
  v8::Local<v8::String> get_name = V8AtomicString(
      isolate, static_cast<String>(StringView("get ", 4) + name_as_view));
  v8::Local<v8::String> set_name = V8AtomicString(
      isolate, static_cast<String>(StringView("set ", 4) + name_as_view));
  v8::Local<v8::Function> get_func =
      CreateFunction<v8::ExceptionContext::kAttributeGet>(
          isolate, context, world, signature, get_name, interface_name,
          v8::ExceptionContext::kAttributeGet, config);
  v8::Local<v8::Function> set_func =
      CreateFunction<v8::ExceptionContext::kAttributeSet>(
          isolate, context, world, signature, set_name, interface_name,
          v8::ExceptionContext::kAttributeSet, config);

  v8::Local<v8::Object> target_object;
  switch (location) {
    case IDLMemberInstaller::FlagLocation::kInstance:
      target_object = instance_object;
      break;
    case IDLMemberInstaller::FlagLocation::kPrototype:
      target_object = prototype_object;
      break;
    case IDLMemberInstaller::FlagLocation::kInterface:
      target_object = interface_object;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  target_object->SetAccessorProperty(
      property_name, get_func, set_func,
      static_cast<v8::PropertyAttribute>(config.v8_property_attribute));
}

void InstallOperation(v8::Isolate* isolate,
                      const DOMWrapperWorld& world,
                      v8::Local<v8::Template> instance_template,
                      v8::Local<v8::Template> prototype_template,
                      v8::Local<v8::Template> interface_template,
                      v8::Local<v8::Signature> signature,
                      const IDLMemberInstaller::OperationConfig& config,
                      const v8::CFunction* v8_cfunction_table_data = nullptr,
                      uint32_t v8_cfunction_table_size = 0) {
  if (!DoesWorldMatch(config, world))
    return;

  IDLMemberInstaller::FlagLocation location =
      static_cast<IDLMemberInstaller::FlagLocation>(config.location);
  if (static_cast<IDLMemberInstaller::FlagReceiverCheck>(
          config.receiver_check) ==
          IDLMemberInstaller::FlagReceiverCheck::kDoNotCheck ||
      location == IDLMemberInstaller::FlagLocation::kInterface)
    signature = v8::Local<v8::Signature>();

  v8::Local<v8::String> property_name =
      V8AtomicString(isolate, config.property_name);
  v8::Local<v8::String> interface_name =
      V8AtomicString(isolate, config.interface_name);
  v8::Local<v8::FunctionTemplate> func =
      CreateFunctionTemplate<v8::ExceptionContext::kOperation>(
          isolate, world, signature, property_name, interface_name,
          v8::ExceptionContext::kOperation, config, v8_cfunction_table_data,
          v8_cfunction_table_size);

  v8::Local<v8::Template> target_template;
  switch (location) {
    case IDLMemberInstaller::FlagLocation::kInstance:
      target_template = instance_template;
      break;
    case IDLMemberInstaller::FlagLocation::kPrototype:
      target_template = prototype_template;
      break;
    case IDLMemberInstaller::FlagLocation::kInterface:
      target_template = interface_template;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  target_template->Set(
      property_name, func,
      static_cast<v8::PropertyAttribute>(config.v8_property_attribute));
}

void InstallOperation(v8::Isolate* isolate,
                      v8::Local<v8::Context> context,
                      const DOMWrapperWorld& world,
                      v8::Local<v8::Object> instance_object,
                      v8::Local<v8::Object> prototype_object,
                      v8::Local<v8::Object> interface_object,
                      v8::Local<v8::Signature> signature,
                      const IDLMemberInstaller::OperationConfig& config,
                      const v8::CFunction* v8_cfunction_table_data = nullptr,
                      uint32_t v8_cfunction_table_size = 0) {
  if (!DoesWorldMatch(config, world))
    return;

  IDLMemberInstaller::FlagLocation location =
      static_cast<IDLMemberInstaller::FlagLocation>(config.location);
  if (static_cast<IDLMemberInstaller::FlagReceiverCheck>(
          config.receiver_check) ==
          IDLMemberInstaller::FlagReceiverCheck::kDoNotCheck ||
      location == IDLMemberInstaller::FlagLocation::kInterface)
    signature = v8::Local<v8::Signature>();

  v8::Local<v8::String> property_name =
      V8AtomicString(isolate, config.property_name);
  v8::Local<v8::String> interface_name =
      V8AtomicString(isolate, config.interface_name);
  v8::Local<v8::Function> func =
      CreateFunction<v8::ExceptionContext::kOperation>(
          isolate, context, world, signature, property_name, interface_name,
          v8::ExceptionContext::kOperation, config, v8_cfunction_table_data,
          v8_cfunction_table_size);

  v8::Local<v8::Object> target_object;
  switch (location) {
    case IDLMemberInstaller::FlagLocation::kInstance:
      target_object = instance_object;
      break;
    case IDLMemberInstaller::FlagLocation::kPrototype:
      target_object = prototype_object;
      break;
    case IDLMemberInstaller::FlagLocation::kInterface:
      target_object = interface_object;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  target_object
      ->DefineOwnProperty(
          context, property_name, func,
          static_cast<v8::PropertyAttribute>(config.v8_property_attribute))
      .ToChecked();
}

}  // namespace

// static
void IDLMemberInstaller::InstallAttributes(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Template> instance_template,
    v8::Local<v8::Template> prototype_template,
    v8::Local<v8::Template> interface_template,
    v8::Local<v8::Signature> signature,
    base::span<const AttributeConfig> configs) {
  for (const auto& config : configs) {
    InstallAttribute(isolate, world, instance_template, prototype_template,
                     interface_template, signature, config);
  }
}

// static
void IDLMemberInstaller::InstallAttributes(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Object> instance_object,
    v8::Local<v8::Object> prototype_object,
    v8::Local<v8::Object> interface_object,
    v8::Local<v8::Signature> signature,
    base::span<const AttributeConfig> configs) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  for (const auto& config : configs) {
    InstallAttribute(isolate, context, world, instance_object, prototype_object,
                     interface_object, signature, config);
  }
}

// static
void IDLMemberInstaller::InstallConstants(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Template> instance_template,
    v8::Local<v8::Template> prototype_template,
    v8::Local<v8::Template> interface_template,
    v8::Local<v8::Signature> signature,
    base::span<const ConstantCallbackConfig> configs) {
  const bool has_prototype_template = !prototype_template.IsEmpty();
  const v8::PropertyAttribute v8_property_attribute =
      static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
  for (const auto& config : configs) {
    v8::Local<v8::String> name = V8AtomicString(isolate, config.name);
    if (has_prototype_template) {
      prototype_template->SetLazyDataProperty(
          name, config.callback, v8::Local<v8::Value>(), v8_property_attribute,
          v8::SideEffectType::kHasNoSideEffect);
    }
    interface_template->SetLazyDataProperty(
        name, config.callback, v8::Local<v8::Value>(), v8_property_attribute,
        v8::SideEffectType::kHasNoSideEffect);
  }
}

// static
void IDLMemberInstaller::InstallConstants(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Template> instance_template,
    v8::Local<v8::Template> prototype_template,
    v8::Local<v8::Template> interface_template,
    v8::Local<v8::Signature> signature,
    base::span<const ConstantValueConfig> configs) {
  const bool has_prototype_template = !prototype_template.IsEmpty();
  const v8::PropertyAttribute v8_property_attribute =
      static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
  for (const auto& config : configs) {
    v8::Local<v8::String> name = V8AtomicString(isolate, config.name);
    v8::Local<v8::Integer> value;
    if (config.value < 0) {
      int32_t i32_value = static_cast<int32_t>(config.value);
      DCHECK_EQ(static_cast<int64_t>(i32_value), config.value);
      value = v8::Integer::New(isolate, i32_value);
    } else {
      uint32_t u32_value = static_cast<uint32_t>(config.value);
      DCHECK_EQ(static_cast<int64_t>(u32_value), config.value);
      value = v8::Integer::NewFromUnsigned(isolate, u32_value);
    }
    if (has_prototype_template) {
      prototype_template->Set(name, value, v8_property_attribute);
    }
    interface_template->Set(name, value, v8_property_attribute);
  }
}

// static
void IDLMemberInstaller::InstallOperations(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Template> instance_template,
    v8::Local<v8::Template> prototype_template,
    v8::Local<v8::Template> interface_template,
    v8::Local<v8::Signature> signature,
    base::span<const OperationConfig> configs) {
  for (const auto& config : configs) {
    InstallOperation(isolate, world, instance_template, prototype_template,
                     interface_template, signature, config);
  }
}

// static
void IDLMemberInstaller::InstallOperations(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Object> instance_object,
    v8::Local<v8::Object> prototype_object,
    v8::Local<v8::Object> interface_object,
    v8::Local<v8::Signature> signature,
    base::span<const OperationConfig> configs) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  for (const auto& config : configs) {
    InstallOperation(isolate, context, world, instance_object, prototype_object,
                     interface_object, signature, config);
  }
}

// static
void IDLMemberInstaller::InstallOperations(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Template> instance_template,
    v8::Local<v8::Template> prototype_template,
    v8::Local<v8::Template> interface_template,
    v8::Local<v8::Signature> signature,
    base::span<const NoAllocDirectCallOperationConfig> configs) {
  for (const auto& config : configs) {
    InstallOperation(isolate, world, instance_template, prototype_template,
                     interface_template, signature, config.operation_config,
                     config.v8_cfunction_table_data,
                     config.v8_cfunction_table_size);
  }
}

// static
void IDLMemberInstaller::InstallOperations(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Object> instance_object,
    v8::Local<v8::Object> prototype_object,
    v8::Local<v8::Object> interface_object,
    v8::Local<v8::Signature> signature,
    base::span<const NoAllocDirectCallOperationConfig> configs) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  for (const auto& config : configs) {
    InstallOperation(isolate, context, world, instance_object, prototype_object,
                     interface_object, signature, config.operation_config,
                     config.v8_cfunction_table_data,
                     config.v8_cfunction_table_size);
  }
}

// static
void IDLMemberInstaller::InstallExposedConstructs(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Template> instance_template,
    v8::Local<v8::Template> prototype_template,
    v8::Local<v8::Template> interface_template,
    v8::Local<v8::Signature> signature,
    base::span<const ExposedConstructConfig> configs) {
  for (const auto& config : configs) {
    v8::Local<v8::String> name = V8AtomicString(isolate, config.name);
    instance_template->SetLazyDataProperty(
        name, config.callback, v8::Local<v8::Value>(), v8::DontEnum,
        v8::SideEffectType::kHasNoSideEffect);
  }
}

// static
void IDLMemberInstaller::InstallExposedConstructs(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Object> instance_object,
    v8::Local<v8::Object> prototype_object,
    v8::Local<v8::Object> interface_object,
    v8::Local<v8::Signature> signature,
    base::span<const ExposedConstructConfig> configs) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  for (const auto& config : configs) {
    instance_object
        ->SetLazyDataProperty(context, V8AtomicString(isolate, config.name),
                              config.callback, v8::Local<v8::Value>(),
                              v8::DontEnum,
                              v8::SideEffectType::kHasNoSideEffect)
        .ToChecked();
  }
}

}  // namespace bindings

}  // namespace blink
