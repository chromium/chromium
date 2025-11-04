/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_WRAPPER_TYPE_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_WRAPPER_TYPE_INFO_H_

#include <type_traits>

#include "base/check_op.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "gin/public/wrapper_info.h"
#include "third_party/blink/renderer/platform/bindings/v8_interface_bridge_base.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/v8.h"

namespace blink {

class DOMWrapperWorld;
class ScriptWrappable;

// LINT.IfChange(ScriptWrappableStartTag)
constexpr std::underlying_type_t<v8::CppHeapPointerTag>
    kScriptWrappableStartTag = 256;
// LINT.ThenChange(third_party/blink/renderer/bindings/scripts/web_idl/idl_compiler.py:ScriptWrappableStartTag)

static constexpr std::underlying_type_t<v8::CppHeapPointerTag>
    kLastGeneratedScriptWrappableTag = 1500;

enum class ScriptWrappableArrayTag : std::underlying_type_t<
    v8::CppHeapPointerTag> {
  kFirst = kLastGeneratedScriptWrappableTag,
  kDOMArrayBufferTag,
  // Start of DOMArrayBufferView subclasses
  kDOMArrayBufferViewTag,
  kDOMDataViewTag,
  kDOMBigInt64ArrayTag,
  kDOMBigUint64ArrayTag,
  kDOMInt8ArrayTag,
  kDOMInt16ArrayTag,
  kDOMInt32ArrayTag,
  kDOMUint8ArrayTag,
  kDOMUint8ClampedArrayTag,
  kDOMUint16ArrayTag,
  kDOMUint32ArrayTag,
  kDOMFloat16ArrayTag,
  kDOMFloat32ArrayTag,
  kDOMFloat64ArrayTag,
  // End of DOMArrayBufferView subclasses
  kDOMSharedArrayBufferTag,
  kFrozenArrayTag,
  kScriptFunctionHolderTag,
  // Start of ObservableArrayExoticObject subclasses
  kObservableArrayExoticObjectTag,
  kV8ObservableArrayCSSStyleSheetTag,
  kV8ObservableArraySpeechRecognitionPhraseTag,
  // End of ObservableArrayExoticObject subclasses
  kLastTag,
};

// `kLastScriptWrappableTag` is an upper bound on the number of ScriptWrappable
// sub-types. If more sub-types are added, the number can be increased
// accordingly. Ideally this upper bound would be generated automatically, but
// that may be difficult.
static constexpr v8::CppHeapPointerTag kLastScriptWrappableTag =
    static_cast<v8::CppHeapPointerTag>(ScriptWrappableArrayTag::kLastTag);

static_assert(static_cast<uint16_t>(kLastScriptWrappableTag) <
                  static_cast<uint16_t>(gin::kFirstPointerTag),
              "The tag range of ScriptWrappable and gin::Wrappable should be "
              "disjoint. If they overlap, then the gin:Wrappable range should "
              "be moved backwards");

constexpr v8::CppHeapPointerTagRange kScriptWrappableTagRange(
    static_cast<v8::CppHeapPointerTag>(kScriptWrappableStartTag),
    v8::CppHeapPointerTag::kLastTag);

constexpr v8::CppHeapPointerTagRange kScriptWrappableOrGinWrappableTagRange(
    static_cast<v8::CppHeapPointerTag>(kScriptWrappableStartTag),
    static_cast<v8::CppHeapPointerTag>(gin::kLastPointerTag));

enum class CppHeapExternalTag : std::underlying_type_t<v8::CppHeapPointerTag> {
  kFirst = 1,
  kTaskAttributionTaskStateTag = kFirst,

  kLastTag = kTaskAttributionTaskStateTag
};

static_assert(static_cast<std::underlying_type_t<v8::CppHeapPointerTag>>(
                  CppHeapExternalTag::kLastTag) < kScriptWrappableStartTag);

// This struct provides a way to store a bunch of information that is helpful
// when unwrapping v8 objects. Each v8 bindings class has exactly one static
// WrapperTypeInfo member, so comparing pointers is a safe way to determine if
// types match.
struct PLATFORM_EXPORT WrapperTypeInfo final
    : public v8::Object::WrapperTypeInfo {
  DISALLOW_NEW();

  enum WrapperTypePrototype {
    kWrapperTypeObjectPrototype,
    kWrapperTypeNoPrototype,  // For legacy callback interface
  };

  enum WrapperClassId {
    // kNoInternalFieldClassId is used for the pseudo wrapper objects which do
    // not have any internal field pointing to a Blink object.
    kNoInternalFieldClassId = 0,
    // NodeClassId must be smaller than ObjectClassId, also must be non-zero.
    kNodeClassId = 1,
    kObjectClassId,
    kCustomWrappableId,
  };

  enum IdlDefinitionKind {
    kIdlInterface,  // includes callback interfaces
    kIdlNamespace,
    // iterators, observably arrays, buffer sources, internal script functions.
    kIdlOtherType,
  };

  bool Equals(const WrapperTypeInfo* that) const { return this == that; }

  bool IsSubclass(const WrapperTypeInfo* that) const {
    for (const WrapperTypeInfo* current = this; current;
         current = current->parent_class) {
      if (current == that)
        return true;
    }

    return false;
  }

  bool SupportsDroppingWrapper() const {
    return wrapper_class_id != kNoInternalFieldClassId;
  }

  // Returns a v8::Template of interface object, namespace object, or the
  // counterpart of the IDL definition.
  //
  // - kIdlInterface: v8::FunctionTemplate of interface object
  // - kIdlNamespace: v8::ObjectTemplate of namespace object
  // - kIdlOtherType: v8::FunctionTemplate
  v8::Local<v8::Template> GetV8ClassTemplate(
      v8::Isolate* isolate,
      const DOMWrapperWorld& world) const;

  void InstallConditionalFeatures(
      v8::Local<v8::Context> context,
      const DOMWrapperWorld& world,
      v8::Local<v8::Object> instance_object,
      v8::Local<v8::Object> prototype_object,
      v8::Local<v8::Object> interface_object,
      v8::Local<v8::Template> interface_template) const {
    if (!install_context_dependent_props_func)
      return;

    install_context_dependent_props_func(
        context, world, instance_object, prototype_object, interface_object,
        interface_template, bindings::V8InterfaceBridgeBase::FeatureSelector());
  }

  static bool HasLegacyInternalFieldsSet(v8::Local<v8::Object> object) {
    for (int i = 0, n = object->InternalFieldCount(); i < n; ++i) {
      if (object->GetAlignedPointerFromInternalField(i, gin::kDeprecatedData)) {
        return true;
      }
    }
    return false;
  }

  bindings::V8InterfaceBridgeBase::InstallInterfaceTemplateFuncType
      install_interface_template_func;
  bindings::V8InterfaceBridgeBase::InstallContextDependentPropertiesFuncType
      install_context_dependent_props_func;
  const char* interface_name;
  // RAW_PTR_EXCLUSION: #global-scope, #reinterpret-cast-trivial-type
  RAW_PTR_EXCLUSION const WrapperTypeInfo* parent_class;

  // When wrapping, we provide `this_tag` to v8's type checking.
  // When unwrapping, we provide `this_tag` and `max_subclass_tag` as the valid
  // range of tags for the object  being unwrapped. The bindings generator is
  // responsible for ensuring the subclass tags are a contiguous range
  // (`this_tag', `max_subclass_tag`].
  v8::CppHeapPointerTag this_tag;
  v8::CppHeapPointerTag max_subclass_tag;

  unsigned wrapper_type_prototype : 2;  // WrapperTypePrototype
  unsigned wrapper_class_id : 2;        // WrapperClassId
  unsigned idl_definition_kind : 2;     // IdlDefinitionKind

  // This is a special case only used by V8WindowProperties::WrapperTypeInfo().
  // WindowProperties is part of Window's prototype object's prototype chain,
  // but not part of Window's interface object prototype chain. When this bit is
  // set, V8PerContextData::ConstructorForTypeSlowCase() skips over this type
  // when constructing the interface object's prototype chain.
  bool is_skipped_in_interface_object_prototype_chain : 1;
};

// `ToAnyScriptWrappable()` is only for use in cases where the subtype of
// ScriptWrappable is unknown and any subtype must be permitted.
// `ToScriptWrappable()` should be used whenever possible for stronger type
// enforcement.
// The return value may be null.
inline ScriptWrappable* ToAnyScriptWrappable(
    v8::Isolate* isolate,
    const v8::TracedReference<v8::Object>& wrapper) {
  return v8::Object::Unwrap<ScriptWrappable>(isolate, wrapper,
                                             kScriptWrappableTagRange);
}

inline v8::Object::Wrappable* ToAnyWrappable(v8::Isolate* isolate,
                                             v8::Local<v8::Object> wrapper) {
  return v8::Object::Unwrap<v8::Object::Wrappable>(
      isolate, wrapper, kScriptWrappableOrGinWrappableTagRange);
}

inline ScriptWrappable* ToAnyScriptWrappable(v8::Isolate* isolate,
                                             v8::Local<v8::Object> wrapper) {
  return v8::Object::Unwrap<ScriptWrappable>(isolate, wrapper,
                                             kScriptWrappableTagRange);
}

PLATFORM_EXPORT const WrapperTypeInfo* ToWrapperTypeInfo(
    const ScriptWrappable* wrappable);

PLATFORM_EXPORT const WrapperTypeInfo* ToWrapperTypeInfo(
    v8::Local<v8::Object> wrapper);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_WRAPPER_TYPE_INFO_H_
