// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_constants.h"

#include <algorithm>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo v8_test_constants_wrapper_type_info = {
    gin::kEmbedderBlink,
    V8TestConstants::DomTemplate,
    nullptr,
    "TestConstants",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestConstants.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestConstants::wrapper_type_info_ = v8_test_constants_wrapper_type_info;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestConstants>::value,
    "TestConstants inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestConstants::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestConstants is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_constants_v8_internal {

}  // namespace test_constants_v8_internal

void V8TestConstants::DEPRECATEDCONSTANTConstantGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestConstants_DEPRECATED_CONSTANT_ConstantGetter");

  Deprecation::CountDeprecation(CurrentExecutionContext(info.GetIsolate()), WebFeature::kConstant);
  V8SetReturnValueInt(info, 1);
}

void V8TestConstants::MEASUREDCONSTANTConstantGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestConstants_MEASURED_CONSTANT_ConstantGetter");

  ExecutionContext* execution_context_for_measurement = CurrentExecutionContext(info.GetIsolate());
  UseCounter::Count(execution_context_for_measurement, WebFeature::kConstant);
  V8SetReturnValueInt(info, 1);
}

static void InstallV8TestConstantsTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interface_template) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interface_template, V8TestConstants::GetWrapperTypeInfo()->interface_name, v8::Local<v8::FunctionTemplate>(), V8TestConstants::kInternalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instance_template = interface_template->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instance_template);
  v8::Local<v8::ObjectTemplate> prototype_template = interface_template->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototype_template);

  // Register IDL constants, attributes and operations.
  {
    static constexpr V8DOMConfiguration::ConstantConfiguration kConstants[] = {
        {"CONST_VALUE_ZERO", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(0)},
        {"CONST_VALUE_ONE", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(1)},
        {"CONST_VALUE_TWO", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(2)},
        {"CONST_VALUE_NEGATIVE", V8DOMConfiguration::kConstantTypeShort, static_cast<int>(-1)},
        {"CONST_VALUE_32_BITS", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(0xffffffff)},
        {"CONST_VALUE_HEX", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(0x01)},
        {"CONST_VALUE_HEX2", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(0X20)},
        {"CONST_VALUE_HEX3", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(0x1abc)},
        {"CONST_VALUE_OCT", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(010)},
        {"CONST_VALUE_NEGATIVE_OCT", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(-010)},
        {"CONST_VALUE_NEGATIVE_HEX", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(-0x1A)},
        {"CONST_VALUE_NEGATIVE_HEX2", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(-0X1a)},
        {"CONST_VALUE_DECIMAL", V8DOMConfiguration::kConstantTypeDouble, static_cast<double>(0.123)},
        {"CONST_VALUE_DECIMAL2", V8DOMConfiguration::kConstantTypeDouble, static_cast<double>(4e9)},
        {"CONST_VALUE_DECIMAL3", V8DOMConfiguration::kConstantTypeDouble, static_cast<double>(3.4e5)},
        {"CONST_VALUE_DECIMAL4", V8DOMConfiguration::kConstantTypeDouble, static_cast<double>(.123)},
        {"CONST_VALUE_DECIMAL5", V8DOMConfiguration::kConstantTypeDouble, static_cast<double>(5E+4)},
        {"CONST_VALUE_NEGATIVE_DECIMAL", V8DOMConfiguration::kConstantTypeDouble, static_cast<double>(-1.3)},
        {"CONST_VALUE_NEGATIVE_DECIMAL2", V8DOMConfiguration::kConstantTypeDouble, static_cast<double>(-4e-9)},
        {"CONST_VALUE_FLOAT", V8DOMConfiguration::kConstantTypeFloat, static_cast<double>(1)},
        {"CONST_JAVASCRIPT", V8DOMConfiguration::kConstantTypeShort, static_cast<int>(1)},
    };
    V8DOMConfiguration::InstallConstants(
        isolate, interface_template, prototype_template,
        kConstants, base::size(kConstants));
  }
  V8DOMConfiguration::InstallConstantWithGetter(
      isolate, interface_template, prototype_template,
      "DEPRECATED_CONSTANT", V8TestConstants::DEPRECATEDCONSTANTConstantGetterCallback);
  V8DOMConfiguration::InstallConstantWithGetter(
      isolate, interface_template, prototype_template,
      "MEASURED_CONSTANT", V8TestConstants::MEASUREDCONSTANTConstantGetterCallback);
  static_assert(0 == TestConstants::kConstValueZero, "the value of TestConstants_kConstValueZero does not match with implementation");
  static_assert(1 == TestConstants::kConstValueOne, "the value of TestConstants_kConstValueOne does not match with implementation");
  static_assert(2 == TestConstants::kConstValueTwo, "the value of TestConstants_kConstValueTwo does not match with implementation");
  static_assert(-1 == TestConstants::kConstValueNegative, "the value of TestConstants_kConstValueNegative does not match with implementation");
  static_assert(0xffffffff == TestConstants::kConstValue32Bits, "the value of TestConstants_kConstValue32Bits does not match with implementation");
  static_assert(0x01 == TestConstants::kConstValueHex, "the value of TestConstants_kConstValueHex does not match with implementation");
  static_assert(0X20 == TestConstants::kConstValueHex2, "the value of TestConstants_kConstValueHex2 does not match with implementation");
  static_assert(0x1abc == TestConstants::kConstValueHex3, "the value of TestConstants_kConstValueHex3 does not match with implementation");
  static_assert(010 == TestConstants::kConstValueOct, "the value of TestConstants_kConstValueOct does not match with implementation");
  static_assert(-010 == TestConstants::kConstValueNegativeOct, "the value of TestConstants_kConstValueNegativeOct does not match with implementation");
  static_assert(-0x1A == TestConstants::kConstValueNegativeHex, "the value of TestConstants_kConstValueNegativeHex does not match with implementation");
  static_assert(-0X1a == TestConstants::kConstValueNegativeHex2, "the value of TestConstants_kConstValueNegativeHex2 does not match with implementation");
  static_assert(1 == TestConstants::kDeprecatedConstant, "the value of TestConstants_kDeprecatedConstant does not match with implementation");
  static_assert(1 == TestConstants::kMeasuredConstant, "the value of TestConstants_kMeasuredConstant does not match with implementation");
  static_assert(1 == TestConstants::kFeature1EnabledConst1, "the value of TestConstants_kFeature1EnabledConst1 does not match with implementation");
  static_assert(2 == TestConstants::kFeature1EnabledConst2, "the value of TestConstants_kFeature1EnabledConst2 does not match with implementation");
  static_assert(3 == TestConstants::kFeature2EnabledConst1, "the value of TestConstants_kFeature2EnabledConst1 does not match with implementation");
  static_assert(4 == TestConstants::kFeature2EnabledConst2, "the value of TestConstants_kFeature2EnabledConst2 does not match with implementation");
  static_assert(6 == TestConstants::kFeature1OriginTrialEnabledConst1, "the value of TestConstants_kFeature1OriginTrialEnabledConst1 does not match with implementation");
  static_assert(7 == TestConstants::kFeature1OriginTrialEnabledConst2, "the value of TestConstants_kFeature1OriginTrialEnabledConst2 does not match with implementation");
  static_assert(8 == TestConstants::kFeature2OriginTrialEnabledConst1, "the value of TestConstants_kFeature2OriginTrialEnabledConst1 does not match with implementation");
  static_assert(9 == TestConstants::kFeature2OriginTrialEnabledConst2, "the value of TestConstants_kFeature2OriginTrialEnabledConst2 does not match with implementation");
  static_assert(1 == TestConstants::CONST_IMPL, "the value of TestConstants_CONST_IMPL does not match with implementation");

  // Custom signature

  V8TestConstants::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interface_template);
}

void V8TestConstants::InstallRuntimeEnabledFeaturesOnTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interface_template) {
  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instance_template = interface_template->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instance_template);
  v8::Local<v8::ObjectTemplate> prototype_template = interface_template->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototype_template);

  // Register IDL constants, attributes and operations.
  if (RuntimeEnabledFeatures::RuntimeFeature1Enabled()) {
    static constexpr V8DOMConfiguration::ConstantConfiguration kConfigurations[] = {
        {"FEATURE1_ENABLED_CONST1", V8DOMConfiguration::kConstantTypeShort, static_cast<int>(1)},
        {"FEATURE1_ENABLED_CONST2", V8DOMConfiguration::kConstantTypeShort, static_cast<int>(2)},
    };
    V8DOMConfiguration::InstallConstants(
        isolate, interface_template, prototype_template,
        kConfigurations, base::size(kConfigurations));
  }
  if (RuntimeEnabledFeatures::RuntimeFeature2Enabled()) {
    static constexpr V8DOMConfiguration::ConstantConfiguration kConfigurations[] = {
        {"FEATURE2_ENABLED_CONST1", V8DOMConfiguration::kConstantTypeShort, static_cast<int>(3)},
        {"FEATURE2_ENABLED_CONST2", V8DOMConfiguration::kConstantTypeShort, static_cast<int>(4)},
    };
    V8DOMConfiguration::InstallConstants(
        isolate, interface_template, prototype_template,
        kConfigurations, base::size(kConfigurations));
  }

  // Custom signature
}

void V8TestConstants::InstallFeatureName1(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Object> instance,
    v8::Local<v8::Object> prototype,
    v8::Local<v8::Function> interface) {
  static constexpr V8DOMConfiguration::ConstantConfiguration
  kFEATURE1ORIGINTRIALENABLEDCONST1Configuration = {"FEATURE1_ORIGIN_TRIAL_ENABLED_CONST1", V8DOMConfiguration::kConstantTypeShort, static_cast<int>(6)};
  V8DOMConfiguration::InstallConstant(
      isolate, interface, prototype, kFEATURE1ORIGINTRIALENABLEDCONST1Configuration);
  static constexpr V8DOMConfiguration::ConstantConfiguration
  kFEATURE1ORIGINTRIALENABLEDCONST2Configuration = {"FEATURE1_ORIGIN_TRIAL_ENABLED_CONST2", V8DOMConfiguration::kConstantTypeShort, static_cast<int>(7)};
  V8DOMConfiguration::InstallConstant(
      isolate, interface, prototype, kFEATURE1ORIGINTRIALENABLEDCONST2Configuration);
}

void V8TestConstants::InstallFeatureName1(
    ScriptState* script_state, v8::Local<v8::Object> instance) {
  V8PerContextData* per_context_data = script_state->PerContextData();
  v8::Local<v8::Object> prototype = per_context_data->PrototypeForType(
      V8TestConstants::GetWrapperTypeInfo());
  v8::Local<v8::Function> interface = per_context_data->ConstructorForType(
      V8TestConstants::GetWrapperTypeInfo());
  ALLOW_UNUSED_LOCAL(interface);
  InstallFeatureName1(script_state->GetIsolate(), script_state->World(), instance, prototype, interface);
}

void V8TestConstants::InstallFeatureName1(ScriptState* script_state) {
  InstallFeatureName1(script_state, v8::Local<v8::Object>());
}

void V8TestConstants::InstallFeatureName2(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Object> instance,
    v8::Local<v8::Object> prototype,
    v8::Local<v8::Function> interface) {
  static constexpr V8DOMConfiguration::ConstantConfiguration
  kFEATURE2ORIGINTRIALENABLEDCONST1Configuration = {"FEATURE2_ORIGIN_TRIAL_ENABLED_CONST1", V8DOMConfiguration::kConstantTypeShort, static_cast<int>(8)};
  V8DOMConfiguration::InstallConstant(
      isolate, interface, prototype, kFEATURE2ORIGINTRIALENABLEDCONST1Configuration);
  static constexpr V8DOMConfiguration::ConstantConfiguration
  kFEATURE2ORIGINTRIALENABLEDCONST2Configuration = {"FEATURE2_ORIGIN_TRIAL_ENABLED_CONST2", V8DOMConfiguration::kConstantTypeShort, static_cast<int>(9)};
  V8DOMConfiguration::InstallConstant(
      isolate, interface, prototype, kFEATURE2ORIGINTRIALENABLEDCONST2Configuration);
}

void V8TestConstants::InstallFeatureName2(
    ScriptState* script_state, v8::Local<v8::Object> instance) {
  V8PerContextData* per_context_data = script_state->PerContextData();
  v8::Local<v8::Object> prototype = per_context_data->PrototypeForType(
      V8TestConstants::GetWrapperTypeInfo());
  v8::Local<v8::Function> interface = per_context_data->ConstructorForType(
      V8TestConstants::GetWrapperTypeInfo());
  ALLOW_UNUSED_LOCAL(interface);
  InstallFeatureName2(script_state->GetIsolate(), script_state->World(), instance, prototype, interface);
}

void V8TestConstants::InstallFeatureName2(ScriptState* script_state) {
  InstallFeatureName2(script_state, v8::Local<v8::Object>());
}

v8::Local<v8::FunctionTemplate> V8TestConstants::DomTemplate(
    v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(
      isolate, world, const_cast<WrapperTypeInfo*>(V8TestConstants::GetWrapperTypeInfo()),
      InstallV8TestConstantsTemplate);
}

bool V8TestConstants::HasInstance(v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(V8TestConstants::GetWrapperTypeInfo(), v8_value);
}

v8::Local<v8::Object> V8TestConstants::FindInstanceInPrototypeChain(
    v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(
      V8TestConstants::GetWrapperTypeInfo(), v8_value);
}

TestConstants* V8TestConstants::ToImplWithTypeCheck(
    v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return HasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestConstants* NativeValueTraits<TestConstants>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  TestConstants* native_value = V8TestConstants::ToImplWithTypeCheck(isolate, value);
  if (!native_value) {
    exception_state.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestConstants"));
  }
  return native_value;
}

}  // namespace blink
