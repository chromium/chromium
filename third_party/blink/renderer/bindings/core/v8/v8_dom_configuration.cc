/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"

#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

template <class Configuration>
bool WorldConfigurationApplies(const Configuration& config,
                               const DOMWrapperWorld& world) {
  const auto current_world_config = world.IsMainWorld()
                                        ? V8DOMConfiguration::kMainWorld
                                        : V8DOMConfiguration::kNonMainWorlds;
  return config.world_configuration & current_world_config;
}

template <>
bool WorldConfigurationApplies(
    const V8DOMConfiguration::SymbolKeyedMethodConfiguration&,
    const DOMWrapperWorld&) {
  return true;
}

void DoInstallAttribute(
    v8::Local<v8::ObjectTemplate> object_template,
    v8::Local<v8::Name> name,
    const V8DOMConfiguration::AttributeConfiguration& attribute) {
  v8::SideEffectType getter_side_effect_type =
      attribute.getter_side_effect_type == V8DOMConfiguration::kHasNoSideEffect
          ? v8::SideEffectType::kHasNoSideEffect
          : v8::SideEffectType::kHasSideEffect;
  switch (static_cast<V8DOMConfiguration::AttributeGetterBehavior>(
      attribute.getter_behavior)) {
    case V8DOMConfiguration::kAlwaysCallGetter:
      object_template->SetNativeDataProperty(
          name, attribute.getter, attribute.setter, v8::Local<v8::Value>(),
          static_cast<v8::PropertyAttribute>(attribute.attribute),
          v8::Local<v8::AccessorSignature>(), v8::AccessControl::DEFAULT,
          getter_side_effect_type);
      break;
    case V8DOMConfiguration::kReplaceWithDataProperty:
      DCHECK(!attribute.setter);
      DCHECK_EQ(attribute.world_configuration, V8DOMConfiguration::kAllWorlds);
      object_template->SetLazyDataProperty(
          name, attribute.getter, v8::Local<v8::Value>(),
          static_cast<v8::PropertyAttribute>(attribute.attribute),
          getter_side_effect_type);
      break;
  }
}

void DoInstallAttribute(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> object,
    v8::Local<v8::Name> name,
    const V8DOMConfiguration::AttributeConfiguration& attribute) {
  v8::SideEffectType getter_side_effect_type =
      attribute.getter_side_effect_type == V8DOMConfiguration::kHasNoSideEffect
          ? v8::SideEffectType::kHasNoSideEffect
          : v8::SideEffectType::kHasSideEffect;
  switch (static_cast<V8DOMConfiguration::AttributeGetterBehavior>(
      attribute.getter_behavior)) {
    case V8DOMConfiguration::kAlwaysCallGetter:
      object
          ->SetNativeDataProperty(
              context, name, attribute.getter, attribute.setter,
              v8::Local<v8::Value>(),
              static_cast<v8::PropertyAttribute>(attribute.attribute),
              getter_side_effect_type)
          .ToChecked();
      break;
    case V8DOMConfiguration::kReplaceWithDataProperty:
      DCHECK(!attribute.setter);
      DCHECK_EQ(attribute.world_configuration, V8DOMConfiguration::kAllWorlds);
      object
          ->SetLazyDataProperty(
              context, name, attribute.getter, v8::Local<v8::Value>(),
              static_cast<v8::PropertyAttribute>(attribute.attribute),
              getter_side_effect_type)
          .ToChecked();
      break;
  }
}

void InstallAttributeInternal(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate> instance_template,
    v8::Local<v8::ObjectTemplate> prototype_template,
    const V8DOMConfiguration::AttributeConfiguration& attribute,
    const DOMWrapperWorld& world) {
  if (!WorldConfigurationApplies(attribute, world))
    return;

  v8::Local<v8::Name> name = V8AtomicString(isolate, attribute.name);
  const unsigned location = attribute.property_location_configuration;
  DCHECK(location);
  if (location & V8DOMConfiguration::kOnInstance)
    DoInstallAttribute(instance_template, name, attribute);
  if (location & V8DOMConfiguration::kOnPrototype)
    DoInstallAttribute(prototype_template, name, attribute);
  if (location & V8DOMConfiguration::kOnInterface)
    NOTREACHED();
}

void InstallAttributeInternal(
    v8::Isolate* isolate,
    v8::Local<v8::Object> instance,
    v8::Local<v8::Object> prototype,
    const V8DOMConfiguration::AttributeConfiguration& config,
    const DOMWrapperWorld& world) {
  DCHECK(!instance.IsEmpty() || !prototype.IsEmpty());
  if (!WorldConfigurationApplies(config, world))
    return;

  v8::Local<v8::Name> name = V8AtomicString(isolate, config.name);
  const unsigned location = config.property_location_configuration;
  DCHECK(location);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (location & V8DOMConfiguration::kOnInstance && !instance.IsEmpty())
    DoInstallAttribute(context, instance, name, config);
  if (location & V8DOMConfiguration::kOnPrototype && !prototype.IsEmpty())
    DoInstallAttribute(context, prototype, name, config);
  if (location & V8DOMConfiguration::kOnInterface)
    NOTREACHED();
}

enum class AccessorType {
  Getter,
  Setter,
};

template <class FunctionOrTemplate>
v8::Local<FunctionOrTemplate> CreateAccessorFunctionOrTemplate(
    v8::Isolate*,
    v8::FunctionCallback,
    V8PrivateProperty::CachedAccessor,
    v8::Local<v8::Value> data,
    v8::Local<v8::Signature>,
    const char* name,
    AccessorType,
    V8DOMConfiguration::AccessCheckConfiguration access_check_configuration,
    v8::SideEffectType side_effect_type = v8::SideEffectType::kHasSideEffect);

template <>
v8::Local<v8::FunctionTemplate>
CreateAccessorFunctionOrTemplate<v8::FunctionTemplate>(
    v8::Isolate* isolate,
    v8::FunctionCallback callback,
    V8PrivateProperty::CachedAccessor cached_property_key,
    v8::Local<v8::Value> data,
    v8::Local<v8::Signature> signature,
    const char* name,
    AccessorType type,
    V8DOMConfiguration::AccessCheckConfiguration access_check_configuration,
    v8::SideEffectType side_effect_type) {
  v8::Local<v8::FunctionTemplate> function_template;
  if (callback) {
    // https://heycam.github.io/webidl/#dfn-attribute-getter has:
    //
    //  5. Perform ! SetFunctionLength(|F|, 0).
    //
    // https://heycam.github.io/webidl/#dfn-attribute-setter has:
    //
    //  7. Perform ! SetFunctionLength(|F|, 1).
    int length = type == AccessorType::Getter ? 0 : 1;

    if (cached_property_key != V8PrivateProperty::CachedAccessor::kNone) {
      function_template = v8::FunctionTemplate::NewWithCache(
          isolate, callback,
          V8PrivateProperty::GetCachedAccessor(isolate, cached_property_key)
              .GetPrivate(),
          data, signature, length, side_effect_type);
    } else {
      function_template = v8::FunctionTemplate::New(
          isolate, callback, data, signature, length,
          v8::ConstructorBehavior::kAllow, side_effect_type);
    }

    if (!function_template.IsEmpty()) {
      function_template->RemovePrototype();
      function_template->SetAcceptAnyReceiver(
          access_check_configuration == V8DOMConfiguration::kDoNotCheckAccess);

      // https://heycam.github.io/webidl/#dfn-attribute-getter has:
      //
      //  3. Let |name| be the string "get " prepended to |attribute|’s
      //  identifier.
      //
      //  4. Perform ! SetFunctionName(|F|, |name|).
      //
      // https://heycam.github.io/webidl/#dfn-attribute-setter has:
      //
      //  5. Let |name| be the string "set " prepended to |id|.
      //
      //  6. Perform ! SetFunctionName(|F|, |name|).
      //
      // SetClassName on a FunctionTemplate that doesn't have a prototype just
      // sets the .name property of the generated function.
      WTF::StringBuilder full_name_builder;
      full_name_builder.Append(type == AccessorType::Getter ? "get " : "set ",
                               4);
      full_name_builder.Append(name);
      function_template->SetClassName(
          V8AtomicString(isolate, full_name_builder.ToString()));
    }
  }
  return function_template;
}

template <>
v8::Local<v8::Function> CreateAccessorFunctionOrTemplate<v8::Function>(
    v8::Isolate* isolate,
    v8::FunctionCallback callback,
    V8PrivateProperty::CachedAccessor,
    v8::Local<v8::Value> data,
    v8::Local<v8::Signature> signature,
    const char* name,
    AccessorType type,
    V8DOMConfiguration::AccessCheckConfiguration access_check_configuration,
    v8::SideEffectType side_effect_type) {
  if (!callback)
    return v8::Local<v8::Function>();

  v8::Local<v8::FunctionTemplate> function_template =
      CreateAccessorFunctionOrTemplate<v8::FunctionTemplate>(
          isolate, callback, V8PrivateProperty::CachedAccessor::kNone, data,
          signature, name, type, access_check_configuration, side_effect_type);
  if (function_template.IsEmpty())
    return v8::Local<v8::Function>();

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Function> function;
  if (!function_template->GetFunction(context).ToLocal(&function))
    return v8::Local<v8::Function>();
  return function;
}

// Returns true iff |target| is the empty handle to v8::Object.
// Returns false especially when |target| is a handle to v8::ObjectTemplate.
bool IsObjectAndEmpty(v8::Local<v8::Object> target) {
  return target.IsEmpty();
}

bool IsObjectAndEmpty(v8::Local<v8::Function> target) {
  return target.IsEmpty();
}

bool IsObjectAndEmpty(v8::Local<v8::ObjectTemplate> target) {
  return false;
}

bool IsObjectAndEmpty(v8::Local<v8::FunctionTemplate> target) {
  return false;
}

template <class ObjectOrTemplate, class FunctionOrTemplate>
void InstallAccessorInternal(
    v8::Isolate* isolate,
    v8::Local<ObjectOrTemplate> instance_or_template,
    v8::Local<ObjectOrTemplate> prototype_or_template,
    v8::Local<FunctionOrTemplate> interface_or_template,
    v8::Local<v8::Signature> signature,
    const V8DOMConfiguration::AccessorConfiguration& config,
    const DOMWrapperWorld& world) {
  DCHECK(!IsObjectAndEmpty(instance_or_template) ||
         !IsObjectAndEmpty(prototype_or_template) ||
         !IsObjectAndEmpty(interface_or_template));
  if (!WorldConfigurationApplies(config, world))
    return;

  v8::Local<v8::String> name = V8AtomicString(isolate, config.name);
  v8::FunctionCallback getter_callback = config.getter;
  v8::FunctionCallback setter_callback = config.setter;
  auto cached_property_key = V8PrivateProperty::CachedAccessor::kNone;
  bool is_window_document = static_cast<V8PrivateProperty::CachedAccessor>(
                                config.cached_property_key) ==
                            V8PrivateProperty::CachedAccessor::kWindowDocument;
  if (!is_window_document || world.IsMainWorld()) {
    cached_property_key = static_cast<V8PrivateProperty::CachedAccessor>(
        config.cached_property_key);
  }

  // Support [LegacyLenientThis] and attributes with Promise types by not
  // specifying the signature. V8 does not do the type checking against holder
  // if no signature is specified. Note that info.Holder() passed to callbacks
  // will be *unsafe*.
  if (config.holder_check_configuration ==
      V8DOMConfiguration::kDoNotCheckHolder)
    signature = v8::Local<v8::Signature>();

  V8DOMConfiguration::AccessCheckConfiguration getter_access_check =
      static_cast<V8DOMConfiguration::AccessCheckConfiguration>(
          config.getter_access_check_configuration);
  V8DOMConfiguration::AccessCheckConfiguration setter_access_check =
      static_cast<V8DOMConfiguration::AccessCheckConfiguration>(
          config.setter_access_check_configuration);

  const unsigned location = config.property_location_configuration;
  v8::SideEffectType getter_side_effect_type =
      config.getter_side_effect_type == V8DOMConfiguration::kHasNoSideEffect
          ? v8::SideEffectType::kHasNoSideEffect
          : v8::SideEffectType::kHasSideEffect;
  DCHECK(location);
  if (location &
      (V8DOMConfiguration::kOnInstance | V8DOMConfiguration::kOnPrototype)) {
    v8::Local<FunctionOrTemplate> getter =
        CreateAccessorFunctionOrTemplate<FunctionOrTemplate>(
            isolate, getter_callback, cached_property_key,
            v8::Local<v8::Value>(), signature, config.name,
            AccessorType::Getter, getter_access_check, getter_side_effect_type);
    v8::Local<FunctionOrTemplate> setter =
        CreateAccessorFunctionOrTemplate<FunctionOrTemplate>(
            isolate, setter_callback, V8PrivateProperty::CachedAccessor::kNone,
            v8::Local<v8::Value>(), signature, config.name,
            AccessorType::Setter, setter_access_check);
    if (location & V8DOMConfiguration::kOnInstance &&
        !IsObjectAndEmpty(instance_or_template)) {
      instance_or_template->SetAccessorProperty(
          name, getter, setter,
          static_cast<v8::PropertyAttribute>(config.attribute));
    }
    if (location & V8DOMConfiguration::kOnPrototype &&
        !IsObjectAndEmpty(prototype_or_template)) {
      prototype_or_template->SetAccessorProperty(
          name, getter, setter,
          static_cast<v8::PropertyAttribute>(config.attribute));
    }
  }
  if (location & V8DOMConfiguration::kOnInterface &&
      !IsObjectAndEmpty(interface_or_template)) {
    // Attributes installed on the interface object must be static
    // attributes, so no need to specify a signature, i.e. no need to do
    // type check against a holder.
    v8::Local<FunctionOrTemplate> getter =
        CreateAccessorFunctionOrTemplate<FunctionOrTemplate>(
            isolate, getter_callback, V8PrivateProperty::CachedAccessor::kNone,
            v8::Local<v8::Value>(), v8::Local<v8::Signature>(), config.name,
            AccessorType::Getter, getter_access_check, getter_side_effect_type);
    v8::Local<FunctionOrTemplate> setter =
        CreateAccessorFunctionOrTemplate<FunctionOrTemplate>(
            isolate, setter_callback, V8PrivateProperty::CachedAccessor::kNone,
            v8::Local<v8::Value>(), v8::Local<v8::Signature>(), config.name,
            AccessorType::Setter, setter_access_check);
    interface_or_template->SetAccessorProperty(
        name, getter, setter,
        static_cast<v8::PropertyAttribute>(config.attribute));
  }
}

v8::Local<v8::Primitive> ValueForConstant(
    v8::Isolate* isolate,
    const V8DOMConfiguration::ConstantConfiguration& constant) {
  v8::Local<v8::Primitive> value;
  switch (constant.type) {
    case V8DOMConfiguration::kConstantTypeShort:
    case V8DOMConfiguration::kConstantTypeLong:
    case V8DOMConfiguration::kConstantTypeUnsignedShort:
      value = v8::Integer::New(isolate, constant.ivalue);
      break;
    case V8DOMConfiguration::kConstantTypeUnsignedLong:
      value = v8::Integer::NewFromUnsigned(isolate, constant.ivalue);
      break;
    case V8DOMConfiguration::kConstantTypeFloat:
    case V8DOMConfiguration::kConstantTypeDouble:
      value = v8::Number::New(isolate, constant.dvalue);
      break;
    default:
      NOTREACHED();
  }
  return value;
}

void InstallConstantInternal(
    v8::Isolate* isolate,
    v8::Local<v8::FunctionTemplate> interface_template,
    v8::Local<v8::ObjectTemplate> prototype_template,
    const V8DOMConfiguration::ConstantConfiguration& constant) {
  v8::Local<v8::String> constant_name = V8AtomicString(isolate, constant.name);
  v8::PropertyAttribute attributes =
      static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
  v8::Local<v8::Primitive> value = ValueForConstant(isolate, constant);
  interface_template->Set(constant_name, value, attributes);
  prototype_template->Set(constant_name, value, attributes);
}

void InstallConstantInternal(
    v8::Isolate* isolate,
    v8::Local<v8::Function> interface,
    v8::Local<v8::Object> prototype,
    const V8DOMConfiguration::ConstantConfiguration& constant) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Name> name = V8AtomicString(isolate, constant.name);
  v8::PropertyAttribute attributes =
      static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
  v8::Local<v8::Primitive> value = ValueForConstant(isolate, constant);
  interface->DefineOwnProperty(context, name, value, attributes).ToChecked();
  prototype->DefineOwnProperty(context, name, value, attributes).ToChecked();
}

template <class Configuration>
void AddMethodToTemplate(v8::Isolate* isolate,
                         v8::Local<v8::Template> v8_template,
                         v8::Local<v8::FunctionTemplate> function_template,
                         const Configuration& method) {
  v8_template->Set(method.MethodName(isolate), function_template,
                   static_cast<v8::PropertyAttribute>(method.attribute));
}

template <>
void AddMethodToTemplate(
    v8::Isolate* isolate,
    v8::Local<v8::Template> v8_template,
    v8::Local<v8::FunctionTemplate> function_template,
    const V8DOMConfiguration::SymbolKeyedMethodConfiguration& method) {
  // The order matters here: if the Symbol is added first, the Function object
  // will have no associated name. For example, WebIDL states, among other
  // things, that a pair iterator's @@iterator Function object's name must be
  // set to "entries".
  if (method.symbol_alias) {
    v8_template->Set(V8AtomicString(isolate, method.symbol_alias),
                     function_template);
  }
  v8_template->Set(method.MethodName(isolate), function_template,
                   static_cast<v8::PropertyAttribute>(method.attribute));
}

template <class Configuration>
void InstallMethodInternal(v8::Isolate* isolate,
                           v8::Local<v8::ObjectTemplate> instance_template,
                           v8::Local<v8::ObjectTemplate> prototype_template,
                           v8::Local<v8::FunctionTemplate> interface_template,
                           v8::Local<v8::Signature> signature,
                           const Configuration& method,
                           const DOMWrapperWorld& world,
                           const v8::CFunction* v8_c_function = nullptr) {
  if (!WorldConfigurationApplies(method, world))
    return;

  v8::FunctionCallback callback = method.callback;
  // Promise-returning functions need to return a reject promise when
  // an exception occurs.  This includes a case that the receiver object is not
  // of the type.  So, we disable the type check of the receiver object on V8
  // side so that V8 won't throw.  Instead, we do the check on Blink side and
  // convert an exception to a reject promise.
  if (method.holder_check_configuration ==
      V8DOMConfiguration::kDoNotCheckHolder)
    signature = v8::Local<v8::Signature>();

  DCHECK(method.property_location_configuration);
  v8::SideEffectType side_effect_type =
      method.side_effect_type == V8DOMConfiguration::kHasNoSideEffect
          ? v8::SideEffectType::kHasNoSideEffect
          : v8::SideEffectType::kHasSideEffect;
  if (method.property_location_configuration &
      (V8DOMConfiguration::kOnInstance | V8DOMConfiguration::kOnPrototype)) {
    // TODO(luoe): use ConstructorBehavior::kThrow for non-constructor methods.
    v8::Local<v8::FunctionTemplate> function_template =
        v8::FunctionTemplate::New(
            isolate, callback, v8::Local<v8::Value>(), signature, method.length,
            v8::ConstructorBehavior::kAllow, side_effect_type, v8_c_function);
    function_template->RemovePrototype();
    function_template->SetAcceptAnyReceiver(
        method.access_check_configuration ==
        V8DOMConfiguration::kDoNotCheckAccess);
    if (method.property_location_configuration &
        V8DOMConfiguration::kOnInstance) {
      AddMethodToTemplate(isolate, instance_template, function_template,
                          method);
    }
    if (method.property_location_configuration &
        V8DOMConfiguration::kOnPrototype) {
      AddMethodToTemplate(isolate, prototype_template, function_template,
                          method);
    }
  }
  if (method.property_location_configuration &
      V8DOMConfiguration::kOnInterface) {
    // Operations installed on the interface object must be static methods, so
    // no need to specify a signature, i.e. no need to do type check against a
    // holder.
    // TODO(luoe): use ConstructorBehavior::kThrow for non-constructor methods.
    v8::Local<v8::FunctionTemplate> function_template =
        v8::FunctionTemplate::New(isolate, callback, v8::Local<v8::Value>(),
                                  v8::Local<v8::Signature>(), method.length,
                                  v8::ConstructorBehavior::kAllow,
                                  side_effect_type);
    function_template->RemovePrototype();
    // Similarly, there is no need to do an access check for static methods, as
    // there is no holder to check against.
    AddMethodToTemplate(isolate, interface_template, function_template, method);
  }
}

void InstallMethodInternal(
    v8::Isolate* isolate,
    v8::Local<v8::Object> instance,
    v8::Local<v8::Object> prototype,
    v8::Local<v8::Function> interface,
    v8::Local<v8::Signature> signature,
    const V8DOMConfiguration::MethodConfiguration& config,
    const DOMWrapperWorld& world) {
  DCHECK(!instance.IsEmpty() || !prototype.IsEmpty() || !interface.IsEmpty());
  if (!WorldConfigurationApplies(config, world))
    return;

  v8::Local<v8::String> name = config.MethodName(isolate);
  v8::FunctionCallback callback = config.callback;
  // Promise-returning functions need to return a reject promise when
  // an exception occurs.  This includes a case that the receiver object is not
  // of the type.  So, we disable the type check of the receiver object on V8
  // side so that V8 won't throw.  Instead, we do the check on Blink side and
  // convert an exception to a reject promise.
  if (config.holder_check_configuration ==
      V8DOMConfiguration::kDoNotCheckHolder)
    signature = v8::Local<v8::Signature>();

  const unsigned location = config.property_location_configuration;
  v8::SideEffectType side_effect_type =
      config.side_effect_type == V8DOMConfiguration::kHasNoSideEffect
          ? v8::SideEffectType::kHasNoSideEffect
          : v8::SideEffectType::kHasSideEffect;
  DCHECK(location);
  if (location &
      (V8DOMConfiguration::kOnInstance | V8DOMConfiguration::kOnPrototype)) {
    // TODO(luoe): use ConstructorBehavior::kThrow for non-constructor methods.
    v8::Local<v8::FunctionTemplate> function_template =
        v8::FunctionTemplate::New(
            isolate, callback, v8::Local<v8::Value>(), signature, config.length,
            v8::ConstructorBehavior::kAllow, side_effect_type);
    function_template->RemovePrototype();
    function_template->SetAcceptAnyReceiver(
        config.access_check_configuration ==
        V8DOMConfiguration::kDoNotCheckAccess);
    v8::Local<v8::Function> function =
        function_template->GetFunction(isolate->GetCurrentContext())
            .ToLocalChecked();
    function->SetName(name);
    if (location & V8DOMConfiguration::kOnInstance && !instance.IsEmpty()) {
      instance
          ->DefineOwnProperty(
              isolate->GetCurrentContext(), name, function,
              static_cast<v8::PropertyAttribute>(config.attribute))
          .ToChecked();
    }
    if (location & V8DOMConfiguration::kOnPrototype && !prototype.IsEmpty()) {
      prototype
          ->DefineOwnProperty(
              isolate->GetCurrentContext(), name, function,
              static_cast<v8::PropertyAttribute>(config.attribute))
          .ToChecked();
    }
  }
  if (location & V8DOMConfiguration::kOnInterface && !interface.IsEmpty()) {
    // Operations installed on the interface object must be static
    // operations, so no need to specify a signature, i.e. no need to do
    // type check against a holder.
    // TODO(luoe): use ConstructorBehavior::kThrow for non-constructor methods.
    v8::Local<v8::FunctionTemplate> function_template =
        v8::FunctionTemplate::New(isolate, callback, v8::Local<v8::Value>(),
                                  v8::Local<v8::Signature>(), config.length,
                                  v8::ConstructorBehavior::kAllow,
                                  side_effect_type);
    function_template->RemovePrototype();
    v8::Local<v8::Function> function =
        function_template->GetFunction(isolate->GetCurrentContext())
            .ToLocalChecked();
    function->SetName(name);
    interface->DefineOwnProperty(isolate->GetCurrentContext(), name, function, static_cast<v8::PropertyAttribute>(config.attribute)).ToChecked();
  }
}

}  // namespace

void V8DOMConfiguration::InstallAttributes(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::ObjectTemplate> instance_template,
    v8::Local<v8::ObjectTemplate> prototype_template,
    const AttributeConfiguration* attributes,
    size_t attribute_count) {
  for (size_t i = 0; i < attribute_count; ++i)
    InstallAttributeInternal(isolate, instance_template, prototype_template,
                             attributes[i], world);
}

void V8DOMConfiguration::InstallAttributes(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Object> instance,
    v8::Local<v8::Object> prototype,
    const AttributeConfiguration* attributes,
    size_t attribute_count) {
  for (size_t i = 0; i < attribute_count; ++i) {
    InstallAttributeInternal(isolate, instance, prototype, attributes[i],
                             world);
  }
}

void V8DOMConfiguration::InstallAttribute(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::ObjectTemplate> instance_template,
    v8::Local<v8::ObjectTemplate> prototype_template,
    const AttributeConfiguration& attribute) {
  InstallAttributeInternal(isolate, instance_template, prototype_template,
                           attribute, world);
}

void V8DOMConfiguration::InstallAttribute(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Object> instance,
    v8::Local<v8::Object> prototype,
    const AttributeConfiguration& attribute) {
  InstallAttributeInternal(isolate, instance, prototype, attribute, world);
}

void V8DOMConfiguration::InstallAccessors(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::ObjectTemplate> instance_template,
    v8::Local<v8::ObjectTemplate> prototype_template,
    v8::Local<v8::FunctionTemplate> interface_template,
    v8::Local<v8::Signature> signature,
    const AccessorConfiguration* accessors,
    size_t accessor_count) {
  for (size_t i = 0; i < accessor_count; ++i)
    InstallAccessorInternal(isolate, instance_template, prototype_template,
                            interface_template, signature, accessors[i], world);
}

void V8DOMConfiguration::InstallAccessors(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Object> instance,
    v8::Local<v8::Object> prototype,
    v8::Local<v8::Function> interface,
    v8::Local<v8::Signature> signature,
    const AccessorConfiguration* accessors,
    size_t accessor_count) {
  for (size_t i = 0; i < accessor_count; ++i) {
    InstallAccessorInternal(isolate, instance, prototype, interface, signature,
                            accessors[i], world);
  }
}

void V8DOMConfiguration::InstallAccessor(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::ObjectTemplate> instance_template,
    v8::Local<v8::ObjectTemplate> prototype_template,
    v8::Local<v8::FunctionTemplate> interface_template,
    v8::Local<v8::Signature> signature,
    const AccessorConfiguration& accessor) {
  InstallAccessorInternal(isolate, instance_template, prototype_template,
                          interface_template, signature, accessor, world);
}

void V8DOMConfiguration::InstallAccessor(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Object> instance,
    v8::Local<v8::Object> prototype,
    v8::Local<v8::Function> interface,
    v8::Local<v8::Signature> signature,
    const AccessorConfiguration& accessor) {
  InstallAccessorInternal(isolate, instance, prototype, interface, signature,
                          accessor, world);
}

void V8DOMConfiguration::InstallConstants(
    v8::Isolate* isolate,
    v8::Local<v8::FunctionTemplate> interface_template,
    v8::Local<v8::ObjectTemplate> prototype_template,
    const ConstantConfiguration* constants,
    size_t constant_count) {
  for (size_t i = 0; i < constant_count; ++i) {
    InstallConstantInternal(isolate, interface_template, prototype_template,
                            constants[i]);
  }
}

void V8DOMConfiguration::InstallConstant(
    v8::Isolate* isolate,
    v8::Local<v8::FunctionTemplate> interface_template,
    v8::Local<v8::ObjectTemplate> prototype_template,
    const ConstantConfiguration& constant) {
  InstallConstantInternal(isolate, interface_template, prototype_template,
                          constant);
}

void V8DOMConfiguration::InstallConstant(
    v8::Isolate* isolate,
    v8::Local<v8::Function> interface,
    v8::Local<v8::Object> prototype,
    const ConstantConfiguration& constant) {
  InstallConstantInternal(isolate, interface, prototype, constant);
}

void V8DOMConfiguration::InstallConstants(
    v8::Isolate* isolate,
    v8::Local<v8::FunctionTemplate> interface_template,
    v8::Local<v8::ObjectTemplate> prototype_template,
    const ConstantCallbackConfiguration* constants,
    size_t constant_count) {
  for (size_t i = 0; i < constant_count; ++i) {
    v8::Local<v8::String> name = V8AtomicString(isolate, constants[i].name);
    interface_template->SetNativeDataProperty(
        name, constants[i].getter, nullptr, v8::Local<v8::Value>(),
        static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete),
        v8::Local<v8::AccessorSignature>(), v8::DEFAULT,
        v8::SideEffectType::kHasNoSideEffect,
        v8::SideEffectType::kHasSideEffect);
    prototype_template->SetNativeDataProperty(
        name, constants[i].getter, nullptr, v8::Local<v8::Value>(),
        static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete),
        v8::Local<v8::AccessorSignature>(), v8::DEFAULT,
        v8::SideEffectType::kHasNoSideEffect,
        v8::SideEffectType::kHasSideEffect);
  }
}

void V8DOMConfiguration::InstallConstants(
    v8::Isolate* isolate,
    v8::Local<v8::Function> interface_object,
    v8::Local<v8::Object> prototype_object,
    const V8DOMConfiguration::ConstantConfiguration* constants,
    size_t constant_count) {
  for (size_t i = 0; i < constant_count; ++i) {
    InstallConstantInternal(isolate, interface_object, prototype_object,
                            constants[i]);
  }
}

void V8DOMConfiguration::InstallConstantWithGetter(
    v8::Isolate* isolate,
    v8::Local<v8::FunctionTemplate> interface_template,
    v8::Local<v8::ObjectTemplate> prototype_template,
    const char* name,
    v8::AccessorNameGetterCallback getter) {
  v8::Local<v8::String> constant_name = V8AtomicString(isolate, name);
  v8::PropertyAttribute attributes =
      static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
  interface_template->SetNativeDataProperty(constant_name, getter, nullptr,
                                            v8::Local<v8::Value>(), attributes);
  prototype_template->SetNativeDataProperty(constant_name, getter, nullptr,
                                            v8::Local<v8::Value>(), attributes);
}

void V8DOMConfiguration::InstallMethods(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::ObjectTemplate> instance_template,
    v8::Local<v8::ObjectTemplate> prototype_template,
    v8::Local<v8::FunctionTemplate> interface_template,
    v8::Local<v8::Signature> signature,
    const MethodConfiguration* methods,
    size_t method_count) {
  for (size_t i = 0; i < method_count; ++i)
    InstallMethodInternal(isolate, instance_template, prototype_template,
                          interface_template, signature, methods[i], world);
}

void V8DOMConfiguration::InstallMethods(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::ObjectTemplate> instance_template,
    v8::Local<v8::ObjectTemplate> prototype_template,
    v8::Local<v8::FunctionTemplate> interface_template,
    v8::Local<v8::Signature> signature,
    const NoAllocDirectCallMethodConfiguration* methods,
    size_t method_count) {
  for (size_t i = 0; i < method_count; ++i) {
    InstallMethodInternal(
        isolate, instance_template, prototype_template, interface_template,
        signature, methods[i].method_config, world, &methods[i].v8_c_function);
  }
}

void V8DOMConfiguration::InstallMethod(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::ObjectTemplate> instance_template,
    v8::Local<v8::ObjectTemplate> prototype_template,
    v8::Local<v8::FunctionTemplate> interface_template,
    v8::Local<v8::Signature> signature,
    const MethodConfiguration& method) {
  InstallMethodInternal(isolate, instance_template, prototype_template,
                        interface_template, signature, method, world);
}

void V8DOMConfiguration::InstallMethods(v8::Isolate* isolate,
                                        const DOMWrapperWorld& world,
                                        v8::Local<v8::Object> instance,
                                        v8::Local<v8::Object> prototype,
                                        v8::Local<v8::Function> interface,
                                        v8::Local<v8::Signature> signature,
                                        const MethodConfiguration* methods,
                                        size_t method_count) {
  for (size_t i = 0; i < method_count; ++i) {
    InstallMethodInternal(isolate, instance, prototype, interface, signature,
                          methods[i], world);
  }
}

void V8DOMConfiguration::InstallMethod(v8::Isolate* isolate,
                                       const DOMWrapperWorld& world,
                                       v8::Local<v8::Object> instance,
                                       v8::Local<v8::Object> prototype,
                                       v8::Local<v8::Function> interface,
                                       v8::Local<v8::Signature> signature,
                                       const MethodConfiguration& method) {
  InstallMethodInternal(isolate, instance, prototype, interface, signature,
                        method, world);
}

void V8DOMConfiguration::InstallMethod(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::ObjectTemplate> prototype_template,
    v8::Local<v8::Signature> signature,
    const SymbolKeyedMethodConfiguration& method) {
  InstallMethodInternal(isolate, v8::Local<v8::ObjectTemplate>(),
                        prototype_template, v8::Local<v8::FunctionTemplate>(),
                        signature, method, world);
}

void V8DOMConfiguration::InitializeDOMInterfaceTemplate(
    v8::Isolate* isolate,
    v8::Local<v8::FunctionTemplate> interface_template,
    const char* interface_name,
    v8::Local<v8::FunctionTemplate> parent_interface_template,
    uint32_t v8_internal_field_count) {
  interface_template->SetClassName(V8AtomicString(isolate, interface_name));
  interface_template->ReadOnlyPrototype();
  v8::Local<v8::ObjectTemplate> instance_template =
      interface_template->InstanceTemplate();
  v8::Local<v8::ObjectTemplate> prototype_template =
      interface_template->PrototypeTemplate();
  instance_template->SetInternalFieldCount(v8_internal_field_count);

  // We intentionally don't set the class string to the platform object
  // (|instanceTemplate|), and set the class string "InterfaceName", without
  // "Prototype", to the prototype object (|prototypeTemplate|) despite a fact
  // that the current WebIDL spec (as of Feb 2017) requires to set the class
  // string "InterfaceName" for the platform objects and
  // "InterfaceNamePrototype" for the interface prototype object, because we
  // think it's more consistent with ECMAScript 2016.
  // See also https://crbug.com/643712
  // https://heycam.github.io/webidl/#es-platform-objects
  // https://heycam.github.io/webidl/#interface-prototype-object
  //
  // Note that V8 minor GC does not collect an object which has an own property.
  // So, if we set the class string to the platform object as an own property,
  // it prevents V8 minor GC to collect the object (V8 minor GC only collects
  // an empty object).  If set, a web test fast/dom/minor-dom-gc.html fails.
  SetClassString(isolate, prototype_template, interface_name);

  if (!parent_interface_template.IsEmpty()) {
    interface_template->Inherit(parent_interface_template);
    // Marks the prototype object as one of native-backed objects.
    // This is needed since bug 110436 asks WebKit to tell native-initiated
    // prototypes from pure-JS ones.  This doesn't mark kinds "root" classes
    // like Node, where setting this changes prototype chain structure.
    // The value of this field is not used, only the count.
    prototype_template->SetInternalFieldCount(kV8PrototypeInternalFieldcount);
  }
}

v8::Local<v8::FunctionTemplate> V8DOMConfiguration::DomClassTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    WrapperTypeInfo* wrapper_type_info,
    InstallTemplateFunction configure_dom_class_template) {
  V8PerIsolateData* data = V8PerIsolateData::From(isolate);
  v8::Local<v8::FunctionTemplate> interface_template =
      data->FindV8Template(world, wrapper_type_info).As<v8::FunctionTemplate>();
  if (!interface_template.IsEmpty())
    return interface_template;

  // We assume all constructors have no JS-observable side effect.
  interface_template = v8::FunctionTemplate::New(
      isolate, V8ObjectConstructor::IsValidConstructorMode);
  configure_dom_class_template(isolate, world, interface_template);
  data->AddV8Template(world, wrapper_type_info, interface_template);
  return interface_template;
}

void V8DOMConfiguration::SetClassString(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate> object_template,
    const char* class_string) {
  object_template->Set(
      v8::Symbol::GetToStringTag(isolate),
      V8AtomicString(isolate, class_string),
      static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontEnum));
}

}  // namespace blink
