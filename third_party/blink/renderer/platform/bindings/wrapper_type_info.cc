// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"

namespace blink {

static_assert(offsetof(struct WrapperTypeInfo, type_id) ==
                  offsetof(struct gin::DeprecatedWrapperInfo, embedder),
              "offset of WrapperTypeInfo.ginEmbedder must be the same as "
              "gin::DeprecatedWrapperInfo.embedder");

v8::Local<v8::Template> WrapperTypeInfo::GetV8ClassTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world) const {
  V8PerIsolateData* per_isolate_data = V8PerIsolateData::From(isolate);
  v8::Local<v8::Template> v8_template =
      per_isolate_data->FindV8Template(world, this);
  if (!v8_template.IsEmpty())
    return v8_template;

  switch (idl_definition_kind) {
    case kIdlInterface:
      v8_template = v8::FunctionTemplate::New(
          isolate, V8ObjectConstructor::IsValidConstructorMode);
      break;
    case kIdlNamespace:
      v8_template = v8::ObjectTemplate::New(isolate);
      break;
    case kIdlOtherType:
      v8_template = v8::FunctionTemplate::New(isolate);
      break;
    default:
      NOTREACHED();
  }
  install_interface_template_func(isolate, world, v8_template);

  per_isolate_data->AddV8Template(world, this, v8_template);
  return v8_template;
}

const WrapperTypeInfo* ToWrapperTypeInfo(const ScriptWrappable* wrappable) {
  DCHECK(wrappable);
  DCHECK_EQ(wrappable->GetWrapperTypeInfo()->type_id, gin::kEmbedderBlink);
  return static_cast<const WrapperTypeInfo*>(wrappable->GetWrapperTypeInfo());
}

const WrapperTypeInfo* ToWrapperTypeInfo(v8::Local<v8::Object> wrapper) {
  const v8::Object::Wrappable* wrappable =
      ToAnyWrappable(v8::Isolate::GetCurrent(), wrapper);
  // It's either us or legacy embedders
  DCHECK(!wrappable || !WrapperTypeInfo::HasLegacyInternalFieldsSet(wrapper));
  if (!wrappable) {
    return nullptr;
  }
  const v8::Object::WrapperTypeInfo* type_info =
      wrappable->GetWrapperTypeInfo();
  if (!type_info || type_info->type_id != gin::kEmbedderBlink) {
    return nullptr;
  }
  return static_cast<const WrapperTypeInfo*>(type_info);
}

}  // namespace blink
