// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"

namespace blink {

static_assert(offsetof(struct WrapperTypeInfo, gin_embedder) ==
                  offsetof(struct gin::WrapperInfo, embedder),
              "offset of WrapperTypeInfo.ginEmbedder must be the same as "
              "gin::WrapperInfo.embedder");

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
    case kIdlCallbackInterface:
      v8_template = v8::FunctionTemplate::New(
          isolate, V8ObjectConstructor::IsValidConstructorMode);
      break;
    case kIdlBufferSourceType:
      NOTREACHED_IN_MIGRATION();
      break;
    case kIdlObservableArray:
      v8_template = v8::FunctionTemplate::New(isolate);
      break;
    case kIdlAsyncOrSyncIterator:
      v8_template = v8::FunctionTemplate::New(isolate);
      break;
    case kCustomWrappableKind:
      v8_template = v8::FunctionTemplate::New(isolate);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  install_interface_template_func(isolate, world, v8_template);

  per_isolate_data->AddV8Template(world, this, v8_template);
  return v8_template;
}

const WrapperTypeInfo* ToWrapperTypeInfo(v8::Local<v8::Object> wrapper) {
  const auto* wrappable = ToAnyScriptWrappable(wrapper->GetIsolate(), wrapper);
  // It's either us or legacy embedders
  DCHECK(!wrappable || !WrapperTypeInfo::HasLegacyInternalFieldsSet(wrapper));
  return wrappable ? wrappable->GetWrapperTypeInfo() : nullptr;
}

}  // namespace blink
