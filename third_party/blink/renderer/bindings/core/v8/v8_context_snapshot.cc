// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_context_snapshot.h"

#include <cstring>

#include "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_target.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_document.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_initializer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/origin_trial_features.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

v8::Local<v8::Function> CreateInterfaceObject(const WrapperTypeInfo* type,
                                              v8::Local<v8::Context> context,
                                              const DOMWrapperWorld& world,
                                              v8::Isolate* isolate) {
  v8::Local<v8::Function> parent_interface_object;
  if (type->parent_class) {
    parent_interface_object =
        CreateInterfaceObject(type->parent_class, context, world, isolate);
  }
  return V8ObjectConstructor::CreateInterfaceObject(
      type, context, world, isolate, parent_interface_object,
      V8ObjectConstructor::CreationMode::kDoNotInstallConditionalFeatures);
}

// TODO(peria): This method is almost a copy of
// V8PerContext::CreateWrapperFromCacheSlowCase(), so merge with it.
v8::Local<v8::Object> CreatePlainWrapper(v8::Isolate* isolate,
                                         const DOMWrapperWorld& world,
                                         v8::Local<v8::Context> context,
                                         const WrapperTypeInfo* type) {
  CHECK(V8HTMLDocument::GetWrapperTypeInfo()->Equals(type));
  v8::Context::Scope scope(context);

  v8::Local<v8::Function> interface_object =
      CreateInterfaceObject(type, context, world, isolate);
  CHECK(!interface_object.IsEmpty());
  v8::Local<v8::Object> instance_template =
      V8ObjectConstructor::NewInstance(isolate, interface_object)
          .ToLocalChecked();
  return instance_template->Clone();
}

int GetSnapshotIndexForWorld(const DOMWrapperWorld& world) {
  return world.IsMainWorld() ? 0 : 1;
}

// Interface templates of those classes are stored in a snapshot without any
// runtime enabled features, so we have to install runtime enabled features on
// them after instantiation.
struct SnapshotInterface {
  const WrapperTypeInfo* wrapper_type_info;
  InstallRuntimeEnabledFeaturesOnTemplateFunction install_function;
};
SnapshotInterface g_snapshot_interfaces[] = {
    {V8Window::GetWrapperTypeInfo(),
     V8Window::InstallRuntimeEnabledFeaturesOnTemplate},
    {V8HTMLDocument::GetWrapperTypeInfo(),
     V8HTMLDocument::InstallRuntimeEnabledFeaturesOnTemplate},
    {V8EventTarget::GetWrapperTypeInfo(),
     V8EventTarget::InstallRuntimeEnabledFeaturesOnTemplate},
    {V8Node::GetWrapperTypeInfo(),
     V8Node::InstallRuntimeEnabledFeaturesOnTemplate},
    {V8Document::GetWrapperTypeInfo(),
     V8Document::InstallRuntimeEnabledFeaturesOnTemplate},
};
constexpr size_t kSnapshotInterfaceSize = base::size(g_snapshot_interfaces);

enum class InternalFieldType : uint8_t {
  kNone,
  kNodeType,
  kDocumentType,
  kHTMLDocumentType,
  kHTMLDocumentObject,
};

const WrapperTypeInfo* FieldTypeToWrapperTypeInfo(InternalFieldType type) {
  switch (type) {
    case InternalFieldType::kNone:
      NOTREACHED();
      break;
    case InternalFieldType::kNodeType:
      return V8Node::GetWrapperTypeInfo();
    case InternalFieldType::kDocumentType:
      return V8Document::GetWrapperTypeInfo();
    case InternalFieldType::kHTMLDocumentType:
      return V8HTMLDocument::GetWrapperTypeInfo();
    case InternalFieldType::kHTMLDocumentObject:
      return V8HTMLDocument::GetWrapperTypeInfo();
  }
  NOTREACHED();
  return nullptr;
}

struct DataForDeserializer {
  STACK_ALLOCATED();
 public:
  DataForDeserializer(Document* document) : document(document) {}

  Member<Document> document;
  // Figures if we failed the deserialization.
  bool did_fail = false;
};

}  // namespace

v8::Local<v8::Context> V8ContextSnapshot::CreateContextFromSnapshot(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::ExtensionConfiguration* extension_configuration,
    v8::Local<v8::Object> global_proxy,
    Document* document) {
  if (!CanCreateContextFromSnapshot(isolate, world, document)) {
    return v8::Local<v8::Context>();
  }

  const int index = GetSnapshotIndexForWorld(world);
  DataForDeserializer data(document);
  v8::DeserializeInternalFieldsCallback callback =
      v8::DeserializeInternalFieldsCallback(&DeserializeInternalField, &data);

  v8::Local<v8::Context> context =
      v8::Context::FromSnapshot(isolate, index, callback,
                                extension_configuration, global_proxy,
                                document->GetMicrotaskQueue())
          .ToLocalChecked();

  // In case we fail to deserialize v8::Context from snapshot,
  // disable the snapshot feature and returns an empty handle.
  // TODO(peria): Drop this fallback routine. crbug.com/881417
  if (data.did_fail) {
    V8PerIsolateData::From(isolate)->BailoutAndDisableV8ContextSnapshot();
    return v8::Local<v8::Context>();
  }

  VLOG(1) << "A context is created from snapshot for "
          << (world.IsMainWorld() ? "" : "non-") << "main world";

  return context;
}

bool V8ContextSnapshot::InstallConditionalFeatures(
    v8::Local<v8::Context> context,
    Document* document) {
  ScriptState* script_state = ScriptState::From(context);
  v8::Isolate* isolate = script_state->GetIsolate();
  const DOMWrapperWorld& world = script_state->World();
  if (!CanCreateContextFromSnapshot(isolate, world, document)) {
    return false;
  }

  TRACE_EVENT1("v8", "V8ContextSnapshot::InstallRuntimeEnabled", "IsMainFrame",
               world.IsMainWorld());

  v8::Local<v8::String> prototype_str = V8AtomicString(isolate, "prototype");
  V8PerContextData* data = script_state->PerContextData();

  v8::Local<v8::Object> global_proxy = context->Global();
  {
    v8::Local<v8::Object> window_wrapper =
        global_proxy->GetPrototype().As<v8::Object>();
    const WrapperTypeInfo* type = V8Window::GetWrapperTypeInfo();
    v8::Local<v8::Function> interface = data->ConstructorForType(type);
    v8::Local<v8::Object> prototype = interface->Get(context, prototype_str)
                                          .ToLocalChecked()
                                          .As<v8::Object>();
    V8Window::InstallRuntimeEnabledFeatures(isolate, world, window_wrapper,
                                            prototype, interface);
    type->InstallConditionalFeatures(context, world, window_wrapper, prototype,
                                     interface,
                                     type->DomTemplate(isolate, world));
    InstallOriginTrialFeatures(type, script_state, prototype, interface);
  }
  {
    const WrapperTypeInfo* type = V8EventTarget::GetWrapperTypeInfo();
    v8::Local<v8::Function> interface = data->ConstructorForType(type);
    v8::Local<v8::Object> prototype = interface->Get(context, prototype_str)
                                          .ToLocalChecked()
                                          .As<v8::Object>();
    V8EventTarget::InstallRuntimeEnabledFeatures(
        isolate, world, v8::Local<v8::Object>(), prototype, interface);
    type->InstallConditionalFeatures(context, world, v8::Local<v8::Object>(),
                                     prototype, interface,
                                     type->DomTemplate(isolate, world));
    InstallOriginTrialFeatures(type, script_state, prototype, interface);
  }

  if (!world.IsMainWorld()) {
    return true;
  }

  // The below code handles window.document on the main world.
  {
    CHECK(document);
    DCHECK(document->IsHTMLDocument());
    CHECK(document->ContainsWrapper());
    v8::Local<v8::Object> document_wrapper =
        ToV8(document, global_proxy, isolate).As<v8::Object>();
    const WrapperTypeInfo* type = V8HTMLDocument::GetWrapperTypeInfo();
    v8::Local<v8::Function> interface = data->ConstructorForType(type);
    v8::Local<v8::Object> prototype = interface->Get(context, prototype_str)
                                          .ToLocalChecked()
                                          .As<v8::Object>();
    V8HTMLDocument::InstallRuntimeEnabledFeatures(
        isolate, world, document_wrapper, prototype, interface);
    type->InstallConditionalFeatures(context, world, document_wrapper,
                                     prototype, interface,
                                     type->DomTemplate(isolate, world));
    InstallOriginTrialFeatures(type, script_state, prototype, interface);
  }
  {
    const WrapperTypeInfo* type = V8Document::GetWrapperTypeInfo();
    v8::Local<v8::Function> interface = data->ConstructorForType(type);
    v8::Local<v8::Object> prototype = interface->Get(context, prototype_str)
                                          .ToLocalChecked()
                                          .As<v8::Object>();
    V8Document::InstallRuntimeEnabledFeatures(
        isolate, world, v8::Local<v8::Object>(), prototype, interface);
    type->InstallConditionalFeatures(context, world, v8::Local<v8::Object>(),
                                     prototype, interface,
                                     type->DomTemplate(isolate, world));
    InstallOriginTrialFeatures(type, script_state, prototype, interface);
  }
  {
    const WrapperTypeInfo* type = V8Node::GetWrapperTypeInfo();
    v8::Local<v8::Function> interface = data->ConstructorForType(type);
    v8::Local<v8::Object> prototype = interface->Get(context, prototype_str)
                                          .ToLocalChecked()
                                          .As<v8::Object>();
    V8Node::InstallRuntimeEnabledFeatures(
        isolate, world, v8::Local<v8::Object>(), prototype, interface);
    type->InstallConditionalFeatures(context, world, v8::Local<v8::Object>(),
                                     prototype, interface,
                                     type->DomTemplate(isolate, world));
    InstallOriginTrialFeatures(type, script_state, prototype, interface);
  }

  return true;
}

void V8ContextSnapshot::EnsureInterfaceTemplates(v8::Isolate* isolate) {
  if (V8PerIsolateData::From(isolate)->GetV8ContextSnapshotMode() !=
      V8PerIsolateData::V8ContextSnapshotMode::kUseSnapshot) {
    return;
  }

  v8::HandleScope handle_scope(isolate);
  // Update the install functions for V8Window and V8Document to work for their
  // partial interfaces.
  SnapshotInterface& snapshot_window = g_snapshot_interfaces[0];
  DCHECK(V8Window::GetWrapperTypeInfo()->Equals(
      snapshot_window.wrapper_type_info));
  snapshot_window.install_function =
      V8Window::install_runtime_enabled_features_on_template_function_;

  SnapshotInterface& snapshot_document = g_snapshot_interfaces[4];
  DCHECK(V8Document::GetWrapperTypeInfo()->Equals(
      snapshot_document.wrapper_type_info));
  snapshot_document.install_function =
      V8Document::install_runtime_enabled_features_on_template_function_;

  EnsureInterfaceTemplatesForWorld(isolate, DOMWrapperWorld::MainWorld());
  // Any world types other than |kMain| are acceptable for this.
  scoped_refptr<DOMWrapperWorld> isolated_world = DOMWrapperWorld::Create(
      isolate, DOMWrapperWorld::WorldType::kForV8ContextSnapshotNonMain);
  EnsureInterfaceTemplatesForWorld(isolate, *isolated_world);
}

v8::StartupData V8ContextSnapshot::TakeSnapshot() {
  DCHECK_EQ(V8PerIsolateData::From(V8PerIsolateData::MainThreadIsolate())
                ->GetV8ContextSnapshotMode(),
            V8PerIsolateData::V8ContextSnapshotMode::kTakeSnapshot);

  v8::SnapshotCreator* creator =
      V8PerIsolateData::From(V8PerIsolateData::MainThreadIsolate())
          ->GetSnapshotCreator();
  v8::Isolate* isolate = creator->GetIsolate();
  CHECK_EQ(isolate, v8::Isolate::GetCurrent());

  // Disable all runtime enabled features
  RuntimeEnabledFeatures::SetStableFeaturesEnabled(false);
  RuntimeEnabledFeatures::SetExperimentalFeaturesEnabled(false);
  RuntimeEnabledFeatures::SetTestFeaturesEnabled(false);

  {
    v8::HandleScope handleScope(isolate);
    creator->SetDefaultContext(v8::Context::New(isolate));

    TakeSnapshotForWorld(creator, DOMWrapperWorld::MainWorld());
    // For non main worlds, we can use any type to create a context.
    TakeSnapshotForWorld(
        creator,
        *DOMWrapperWorld::Create(
            isolate, DOMWrapperWorld::WorldType::kForV8ContextSnapshotNonMain));
  }

  isolate->RemoveMessageListeners(V8Initializer::MessageHandlerInMainThread);

  v8::StartupData blob =
      creator->CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);

  return blob;
}

v8::StartupData V8ContextSnapshot::SerializeInternalField(
    v8::Local<v8::Object> object,
    int index,
    void*) {
  InternalFieldType field_type = InternalFieldType::kNone;
  const WrapperTypeInfo* wrapper_type = ToWrapperTypeInfo(object);
  if (kV8DOMWrapperObjectIndex == index) {
    if (blink::V8HTMLDocument::GetWrapperTypeInfo()->Equals(wrapper_type)) {
      field_type = InternalFieldType::kHTMLDocumentObject;
    }
    DCHECK_LE(kV8DefaultWrapperInternalFieldCount,
              object->InternalFieldCount());
  } else if (kV8DOMWrapperTypeIndex == index) {
    if (blink::V8HTMLDocument::GetWrapperTypeInfo()->Equals(wrapper_type)) {
      field_type = InternalFieldType::kHTMLDocumentType;
    } else if (blink::V8Document::GetWrapperTypeInfo()->Equals(wrapper_type)) {
      field_type = InternalFieldType::kDocumentType;
    } else if (blink::V8Node::GetWrapperTypeInfo()->Equals(wrapper_type)) {
      field_type = InternalFieldType::kNodeType;
    }
    DCHECK_LE(kV8PrototypeInternalFieldcount, object->InternalFieldCount());
  }
  CHECK_NE(field_type, InternalFieldType::kNone);

  int size = sizeof(InternalFieldType);
  // Allocated memory on |data| will be released in
  // v8::i::PartialSerializer::SerializeEmbedderFields().
  char* data = new char[size];
  std::memcpy(data, &field_type, size);

  return {data, size};
}

void V8ContextSnapshot::DeserializeInternalField(v8::Local<v8::Object> object,
                                                 int index,
                                                 v8::StartupData payload,
                                                 void* ptr) {
  // DeserializeInternalField() expects to be called in the main world
  // with |document| being HTMLDocument.
  CHECK_EQ(payload.raw_size, static_cast<int>(sizeof(InternalFieldType)));
  InternalFieldType type =
      *reinterpret_cast<const InternalFieldType*>(payload.data);

  const WrapperTypeInfo* wrapper_type_info = FieldTypeToWrapperTypeInfo(type);
  DataForDeserializer* embed_data = static_cast<DataForDeserializer*>(ptr);
  switch (type) {
    case InternalFieldType::kNodeType:
    case InternalFieldType::kDocumentType:
    case InternalFieldType::kHTMLDocumentType: {
      // TODO(peria): Make this branch back to CHECK_EQ. crbug.com/881417
      if (index != kV8DOMWrapperTypeIndex) {
        LOG(ERROR) << "Invalid index for wrpper type info: " << index;
        embed_data->did_fail = true;
        return;
      }
      object->SetAlignedPointerInInternalField(
          index, const_cast<WrapperTypeInfo*>(wrapper_type_info));
      return;
    }
    case InternalFieldType::kHTMLDocumentObject: {
      // There seems to be few crash reports with invalid |index|.
      // In such cases, we fallback to create v8::Context without snapshots.
      // TODO(peria): Make this branch back to CHECK_EQ. crbug.com/881417
      if (index != kV8DOMWrapperObjectIndex) {
        LOG(ERROR) << "Invalid index for HTMLDocument object: " << index;
        embed_data->did_fail = true;
        return;
      }

      // The below code handles window.document on the main world.
      v8::Isolate* isolate = v8::Isolate::GetCurrent();
      ScriptWrappable* document = embed_data->document;
      DCHECK(document);

      // Make reference from wrapper to document
      object->SetAlignedPointerInInternalField(index, document);
      // Make reference from document to wrapper
      // TODO(peria): Make this branch back to CHECK. crbug.com/881417
      if (!document->SetWrapper(isolate, wrapper_type_info, object)) {
        LOG(ERROR) << "Failed to set HTMLDocument wrapper on Blink object.";
        embed_data->did_fail = true;
        return;
      }
      WrapperTypeInfo::WrapperCreated();
      return;
    }
    case InternalFieldType::kNone:
      NOTREACHED();
      return;
  }

  NOTREACHED();
}

bool V8ContextSnapshot::CanCreateContextFromSnapshot(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    Document* document) {
  DCHECK(document);
  if (V8PerIsolateData::From(isolate)->GetV8ContextSnapshotMode() !=
      V8PerIsolateData::V8ContextSnapshotMode::kUseSnapshot) {
    return false;
  }

  // When creating a context for the main world from snapshot, we also need a
  // HTMLDocument instance. If typeof window.document is not HTMLDocument, e.g.
  // SVGDocument or XMLDocument, we can't create contexts from the snapshot.
  return !world.IsMainWorld() || document->IsHTMLDocument();
}

void V8ContextSnapshot::EnsureInterfaceTemplatesForWorld(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world) {
  V8PerIsolateData* data = V8PerIsolateData::From(isolate);

  // A snapshot has some interface templates in it.  The first
  // |kSnapshotInterfaceSize| templates are for the main world, and the
  // remaining templates are for isolated worlds.
  const int index_offset = world.IsMainWorld() ? 0 : kSnapshotInterfaceSize;

  for (size_t i = 0; i < kSnapshotInterfaceSize; ++i) {
    auto& snapshot_interface = g_snapshot_interfaces[i];
    const WrapperTypeInfo* wrapper_type_info =
        snapshot_interface.wrapper_type_info;
    v8::Local<v8::FunctionTemplate> interface_template =
        isolate->GetDataFromSnapshotOnce<v8::FunctionTemplate>(index_offset + i)
            .ToLocalChecked();
    snapshot_interface.install_function(isolate, world, interface_template);
    CHECK(!interface_template.IsEmpty());
    data->SetInterfaceTemplate(world, wrapper_type_info, interface_template);
  }
}

void V8ContextSnapshot::TakeSnapshotForWorld(v8::SnapshotCreator* creator,
                                             const DOMWrapperWorld& world) {
  v8::Isolate* isolate = creator->GetIsolate();
  CHECK_EQ(isolate, v8::Isolate::GetCurrent());

  // Function templates
  v8::HandleScope handleScope(isolate);
  Vector<v8::Local<v8::FunctionTemplate>> interface_templates(
      kSnapshotInterfaceSize);
  v8::Local<v8::FunctionTemplate> window_template;
  for (size_t i = 0; i < kSnapshotInterfaceSize; ++i) {
    const WrapperTypeInfo* wrapper_type_info =
        g_snapshot_interfaces[i].wrapper_type_info;
    v8::Local<v8::FunctionTemplate> interface_template =
        wrapper_type_info->DomTemplate(isolate, world);
    CHECK(!interface_template.IsEmpty());
    interface_templates[i] = interface_template;
    if (V8Window::GetWrapperTypeInfo()->Equals(wrapper_type_info)) {
      window_template = interface_template;
    }
  }
  CHECK(!window_template.IsEmpty());

  v8::Local<v8::ObjectTemplate> window_instance_template =
      window_template->InstanceTemplate();
  CHECK(!window_instance_template.IsEmpty());

  v8::Local<v8::Context> context;
  {
    V8PerIsolateData::UseCounterDisabledScope use_counter_disabled(
        V8PerIsolateData::From(isolate));
    context = v8::Context::New(isolate, nullptr, window_instance_template);
  }
  CHECK(!context.IsEmpty());

  // For the main world context, we need to prepare a HTMLDocument wrapper and
  // set it to window.documnt.
  if (world.IsMainWorld()) {
    v8::Context::Scope scope(context);
    v8::Local<v8::Object> document_wrapper = CreatePlainWrapper(
        isolate, world, context, V8HTMLDocument::GetWrapperTypeInfo());
    int indices[] = {kV8DOMWrapperObjectIndex, kV8DOMWrapperTypeIndex};
    void* values[] = {nullptr, const_cast<WrapperTypeInfo*>(
                                   V8HTMLDocument::GetWrapperTypeInfo())};
    document_wrapper->SetAlignedPointerInInternalFields(base::size(indices),
                                                        indices, values);

    // Set the cached accessor for window.document.
    CHECK(V8PrivateProperty::GetWindowDocumentCachedAccessor(isolate).Set(
        context->Global(), document_wrapper));
  }

  for (auto& interface_template : interface_templates) {
    creator->AddData(interface_template);
  }
  creator->AddContext(context, SerializeInternalField);

  V8PerIsolateData::From(isolate)->ClearPersistentsForV8ContextSnapshot();
}

}  // namespace blink
