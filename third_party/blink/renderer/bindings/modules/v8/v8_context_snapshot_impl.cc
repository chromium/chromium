// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/modules/v8/v8_context_snapshot_impl.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_context_snapshot.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_target.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_document.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_initializer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_document.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_window.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "tools/v8_context_snapshot/buildflags.h"

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#include "gin/public/v8_snapshot_file_type.h"
#endif

namespace blink {
namespace {

bool IsUsingContextSnapshot() {
#if BUILDFLAG(USE_V8_CONTEXT_SNAPSHOT)
  if (Platform::Current()->IsTakingV8ContextSnapshot() ||
      gin::GetLoadedSnapshotFileType() ==
          gin::V8SnapshotFileType::kWithAdditionalContext) {
    return true;
  }
#endif  // BUILDFLAG(USE_V8_CONTEXT_SNAPSHOT)
  return false;
}

}  // namespace

void V8ContextSnapshotImpl::Init() {
  V8ContextSnapshot::SetCreateContextFromSnapshotFunc(CreateContext);
  V8ContextSnapshot::SetInstallContextIndependentPropsFunc(
      InstallContextIndependentProps);
  V8ContextSnapshot::SetEnsureInterfaceTemplatesFunc(InstallInterfaceTemplates);
  V8ContextSnapshot::SetTakeSnapshotFunc(TakeSnapshot);
  V8ContextSnapshot::SetGetReferenceTableFunc(GetReferenceTable);
}

namespace {

// Layout of the snapshot
//
// Context:
//   [ main world context, isolated world context ]
// Data:
//   [ main world: [ Window template, HTMLDocument template, ... ],
//     isolated world: [ Window template, HTMLDocument template, ... ],
//   ]
//
// The main world's snapshot contains the window object (as the global object)
// and the main document of type HTMLDocument (although the main document is
// not necessarily an HTMLDocument).  The isolated world's snapshot contains
// the window object only.

constexpr const size_t kNumOfWorlds = 2;

inline scoped_refptr<DOMWrapperWorld> IndexToWorld(v8::Isolate* isolate,
                                                   size_t index) {
  return index == 0
             ? scoped_refptr<DOMWrapperWorld>(&DOMWrapperWorld::MainWorld())
             : DOMWrapperWorld::EnsureIsolatedWorld(
                   isolate, DOMWrapperWorld::WorldId::kMainWorldId + 1);
}

inline int WorldToIndex(const DOMWrapperWorld& world) {
  if (world.IsMainWorld()) {
    return 0;
  } else if (world.IsIsolatedWorld()) {
    return 1;
  } else {
    LOG(FATAL) << "Unknown DOMWrapperWorld";
    return 1;
  }
}

using InstallPropsPerContext =
    void (*)(v8::Local<v8::Context> context,
             const DOMWrapperWorld& world,
             v8::Local<v8::Object> instance_object,
             v8::Local<v8::Object> prototype_object,
             v8::Local<v8::Object> interface_object,
             v8::Local<v8::Template> interface_template);
using InstallPropsPerIsolate =
    void (*)(v8::Isolate* isolate,
             const DOMWrapperWorld& world,
             v8::Local<v8::Template> instance_template,
             v8::Local<v8::Template> prototype_template,
             v8::Local<v8::Template> interface_template);

// Construction of |type_info_table| requires non-trivial initialization due
// to cross-component address resolution.  We ignore this issue because the
// issue happens only on component builds and the official release builds
// (statically-linked builds) are never affected by this issue.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

const struct {
  const WrapperTypeInfo* wrapper_type_info;
  // Installs context-independent properties to per-isolate templates.
  InstallPropsPerIsolate install_props_per_isolate;
  // Installs context-independent properties to objects in the context.
  InstallPropsPerContext install_props_per_context;
  bool needs_per_context_install[kNumOfWorlds];
} type_info_table[] = {
    {V8Window::GetWrapperTypeInfo(),
     bindings::v8_context_snapshot::InstallPropsOfV8Window,
     bindings::v8_context_snapshot::InstallPropsOfV8Window,
     {true, true}},
    {V8HTMLDocument::GetWrapperTypeInfo(),
     bindings::v8_context_snapshot::InstallPropsOfV8HTMLDocument,
     bindings::v8_context_snapshot::InstallPropsOfV8HTMLDocument,
     {true, false}},
    {V8Document::GetWrapperTypeInfo(),
     bindings::v8_context_snapshot::InstallPropsOfV8Document,
     bindings::v8_context_snapshot::InstallPropsOfV8Document,
     {true, false}},
    {V8Node::GetWrapperTypeInfo(),
     bindings::v8_context_snapshot::InstallPropsOfV8Node,
     bindings::v8_context_snapshot::InstallPropsOfV8Node,
     {true, false}},
    {V8EventTarget::GetWrapperTypeInfo(),
     bindings::v8_context_snapshot::InstallPropsOfV8EventTarget,
     bindings::v8_context_snapshot::InstallPropsOfV8EventTarget,
     {true, true}},
};

#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

enum class InternalFieldSerializedValue : uint8_t {
  // ScriptWrappable pointer
  kSwHTMLDocument = 1,
  kSwWindow,
  // WrapperTypeInfo pointer
  kWtiHTMLDocument,
  kWtiWindow,
};

struct DeserializerData {
  STACK_ALLOCATED();

 public:
  v8::Isolate* isolate;
  const DOMWrapperWorld& world;
  HTMLDocument* html_document;
};

v8::Local<v8::Function> CreateInterfaceObject(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    const DOMWrapperWorld& world,
    const WrapperTypeInfo* wrapper_type_info) {
  v8::Local<v8::Function> parent_interface_object;
  if (wrapper_type_info->parent_class) {
    parent_interface_object = CreateInterfaceObject(
        isolate, context, world, wrapper_type_info->parent_class);
  }
  return V8ObjectConstructor::CreateInterfaceObject(
      wrapper_type_info, context, world, isolate, parent_interface_object,
      V8ObjectConstructor::CreationMode::kDoNotInstallConditionalFeatures);
}

v8::Local<v8::Object> CreatePlatformObject(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    const DOMWrapperWorld& world,
    const WrapperTypeInfo* wrapper_type_info) {
  v8::Local<v8::Function> interface_object =
      CreateInterfaceObject(isolate, context, world, wrapper_type_info);
  v8::Context::Scope context_scope(context);
  return V8ObjectConstructor::NewInstance(isolate, interface_object)
      .ToLocalChecked();
}

v8::StartupData SerializeInternalFieldCallback(v8::Local<v8::Object> object,
                                               int index,
                                               void* unused_data) {
  InternalFieldSerializedValue value;
  const WrapperTypeInfo* wrapper_type_info = ToWrapperTypeInfo(object);
  if (index == kV8DOMWrapperObjectIndex) {
    if (wrapper_type_info == V8HTMLDocument::GetWrapperTypeInfo()) {
      value = InternalFieldSerializedValue::kSwHTMLDocument;
    } else if (wrapper_type_info == V8Window::GetWrapperTypeInfo()) {
      value = InternalFieldSerializedValue::kSwWindow;
    } else {
      LOG(FATAL) << "Unknown WrapperTypeInfo";
      return {nullptr, 0};
    }
  } else if (index == kV8DOMWrapperTypeIndex) {
    if (wrapper_type_info == V8HTMLDocument::GetWrapperTypeInfo()) {
      value = InternalFieldSerializedValue::kWtiHTMLDocument;
    } else if (wrapper_type_info == V8Window::GetWrapperTypeInfo()) {
      value = InternalFieldSerializedValue::kWtiWindow;
    } else {
      LOG(FATAL) << "Unknown WrapperTypeInfo";
      return {nullptr, 0};
    }
  } else {
    LOG(FATAL) << "Unknown internal field";
    return {nullptr, 0};
  }

  int size = 1;  // No endian support
  uint8_t* data = new uint8_t[size];
  *data = static_cast<uint8_t>(value);
  CHECK_EQ(static_cast<InternalFieldSerializedValue>(*data), value);
  return {reinterpret_cast<char*>(data), size};
}

void DeserializeInternalFieldCallback(v8::Local<v8::Object> object,
                                      int index,
                                      v8::StartupData payload,
                                      void* data) {
  CHECK_EQ(payload.raw_size, 1);  // No endian support
  uint8_t value = *reinterpret_cast<const uint8_t*>(payload.data);

  DeserializerData* deserializer_data =
      reinterpret_cast<DeserializerData*>(data);

  switch (static_cast<InternalFieldSerializedValue>(value)) {
    case InternalFieldSerializedValue::kSwHTMLDocument: {
      CHECK_EQ(index, kV8DOMWrapperObjectIndex);
      CHECK(deserializer_data->html_document);
      CHECK(deserializer_data->world.IsMainWorld());
      V8DOMWrapper::SetNativeInfo(deserializer_data->isolate, object,
                                  V8HTMLDocument::GetWrapperTypeInfo(),
                                  deserializer_data->html_document);
      bool result = deserializer_data->html_document->SetWrapper(
          deserializer_data->isolate, V8HTMLDocument::GetWrapperTypeInfo(),
          object);
      CHECK(result);
      break;
    }
    case InternalFieldSerializedValue::kSwWindow:
      CHECK_EQ(index, kV8DOMWrapperObjectIndex);
      // The global object's internal fields will be set in LocalWindowProxy.
      break;
    case InternalFieldSerializedValue::kWtiHTMLDocument:
      CHECK_EQ(index, kV8DOMWrapperTypeIndex);
      CHECK(deserializer_data->html_document);
      CHECK(deserializer_data->world.IsMainWorld());
      // The internal field of WrapperTypeInfo must be filled in
      // kSwHTMLDocument case.
      break;
    case InternalFieldSerializedValue::kWtiWindow:
      CHECK_EQ(index, kV8DOMWrapperTypeIndex);
      // The global object's internal fields will be set in LocalWindowProxy.
      break;
    default:
      LOG(FATAL) << "Unknown serialized value";
  }
}

void TakeSnapshotForWorld(v8::SnapshotCreator* snapshot_creator,
                          const DOMWrapperWorld& world) {
  v8::Isolate* isolate = snapshot_creator->GetIsolate();
  V8PerIsolateData* per_isolate_data = V8PerIsolateData::From(isolate);

  // Set up the context and global object.
  v8::Local<v8::FunctionTemplate> window_interface_template =
      V8Window::GetWrapperTypeInfo()
          ->GetV8ClassTemplate(isolate, world)
          .As<v8::FunctionTemplate>();
  v8::Local<v8::ObjectTemplate> window_instance_template =
      window_interface_template->InstanceTemplate();
  v8::Local<v8::Context> context;
  {
    V8PerIsolateData::UseCounterDisabledScope use_counter_disabled_scope(
        per_isolate_data);
    context = v8::Context::New(isolate, nullptr, window_instance_template);
    CHECK(!context.IsEmpty());
  }

  // Set up the cached accessor of 'window.document'.
  if (world.IsMainWorld()) {
    v8::Context::Scope context_scope(context);

    const WrapperTypeInfo* document_wrapper_type_info =
        V8HTMLDocument::GetWrapperTypeInfo();
    v8::Local<v8::Object> document_wrapper = CreatePlatformObject(
        isolate, context, world, document_wrapper_type_info);

    int indices[] = {kV8DOMWrapperObjectIndex, kV8DOMWrapperTypeIndex};
    void* values[] = {nullptr,
                      const_cast<WrapperTypeInfo*>(document_wrapper_type_info)};
    document_wrapper->SetAlignedPointerInInternalFields(std::size(indices),
                                                        indices, values);

    V8PrivateProperty::GetWindowDocumentCachedAccessor(isolate).Set(
        context->Global(), document_wrapper);
  }

  snapshot_creator->AddContext(context, SerializeInternalFieldCallback);
  for (const auto& type_info : type_info_table) {
    snapshot_creator->AddData(
        type_info.wrapper_type_info->GetV8ClassTemplate(isolate, world));
  }
}

}  // namespace

v8::Local<v8::Context> V8ContextSnapshotImpl::CreateContext(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::ExtensionConfiguration* extension_config,
    v8::Local<v8::Object> global_proxy,
    Document* document) {
  DCHECK(document);
  if (!IsUsingContextSnapshot())
    return v8::Local<v8::Context>();

  V8PerIsolateData* per_isolate_data = V8PerIsolateData::From(isolate);
  if (per_isolate_data->GetV8ContextSnapshotMode() !=
      V8PerIsolateData::V8ContextSnapshotMode::kUseSnapshot) {
    return v8::Local<v8::Context>();
  }

  HTMLDocument* html_document = DynamicTo<HTMLDocument>(document);
  CHECK(!html_document || html_document->GetWrapperTypeInfo() ==
                              V8HTMLDocument::GetWrapperTypeInfo());
  if (world.IsMainWorld()) {
    if (!html_document)
      return v8::Local<v8::Context>();
  } else {
    // Prevent an accidental misuse in a non-main world.
    html_document = nullptr;
  }

  DeserializerData deserializer_data = {isolate, world, html_document};
  v8::DeserializeInternalFieldsCallback internal_field_desrializer(
      DeserializeInternalFieldCallback, &deserializer_data);
  return v8::Context::FromSnapshot(
             isolate, WorldToIndex(world), internal_field_desrializer,
             extension_config, global_proxy,
             document->GetExecutionContext()->GetMicrotaskQueue())
      .ToLocalChecked();
}

void V8ContextSnapshotImpl::InstallContextIndependentProps(
    ScriptState* script_state) {
  if (!IsUsingContextSnapshot())
    return;

  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Context> context = script_state->GetContext();
  const DOMWrapperWorld& world = script_state->World();
  const int world_index = WorldToIndex(world);
  V8PerContextData* per_context_data = script_state->PerContextData();
  v8::Local<v8::String> prototype_string = V8AtomicString(isolate, "prototype");

  for (const auto& type_info : type_info_table) {
    if (!type_info.needs_per_context_install[world_index])
      continue;

    const auto* wrapper_type_info = type_info.wrapper_type_info;
    v8::Local<v8::Template> interface_template =
        wrapper_type_info->GetV8ClassTemplate(isolate, world);
    v8::Local<v8::Function> interface_object =
        per_context_data->ConstructorForType(wrapper_type_info);
    v8::Local<v8::Object> prototype_object =
        interface_object->Get(context, prototype_string)
            .ToLocalChecked()
            .As<v8::Object>();
    v8::Local<v8::Object> instance_object;
    type_info.install_props_per_context(context, world, instance_object,
                                        prototype_object, interface_object,
                                        interface_template);
  }
}

void V8ContextSnapshotImpl::InstallInterfaceTemplates(v8::Isolate* isolate) {
  if (!IsUsingContextSnapshot())
    return;

  V8PerIsolateData* per_isolate_data = V8PerIsolateData::From(isolate);
  if (per_isolate_data->GetV8ContextSnapshotMode() !=
      V8PerIsolateData::V8ContextSnapshotMode::kUseSnapshot) {
    return;
  }

  v8::HandleScope handle_scope(isolate);

  for (size_t world_index = 0; world_index < kNumOfWorlds; ++world_index) {
    scoped_refptr<DOMWrapperWorld> world = IndexToWorld(isolate, world_index);
    for (size_t i = 0; i < std::size(type_info_table); ++i) {
      const auto& type_info = type_info_table[i];
      v8::Local<v8::FunctionTemplate> interface_template =
          isolate
              ->GetDataFromSnapshotOnce<v8::FunctionTemplate>(
                  world_index * std::size(type_info_table) + i)
              .ToLocalChecked();
      per_isolate_data->AddV8Template(*world, type_info.wrapper_type_info,
                                      interface_template);
      type_info.install_props_per_isolate(
          isolate, *world, interface_template->InstanceTemplate(),
          interface_template->PrototypeTemplate(), interface_template);
    }
  }
}

v8::StartupData V8ContextSnapshotImpl::TakeSnapshot() {
  v8::Isolate* isolate = V8PerIsolateData::MainThreadIsolate();
  CHECK(isolate);
  CHECK(isolate->IsCurrent());
  V8PerIsolateData* per_isolate_data = V8PerIsolateData::From(isolate);
  CHECK_EQ(per_isolate_data->GetV8ContextSnapshotMode(),
           V8PerIsolateData::V8ContextSnapshotMode::kTakeSnapshot);
  DCHECK(IsUsingContextSnapshot());

  // Take a snapshot with minimum set-up.  It's easier to add properties than
  // removing ones, so make it no need to remove any property.
  RuntimeEnabledFeatures::SetStableFeaturesEnabled(false);
  RuntimeEnabledFeatures::SetExperimentalFeaturesEnabled(false);
  RuntimeEnabledFeatures::SetTestFeaturesEnabled(false);

  v8::SnapshotCreator* snapshot_creator =
      per_isolate_data->GetSnapshotCreator();

  {
    v8::HandleScope handle_scope(isolate);
    snapshot_creator->SetDefaultContext(v8::Context::New(isolate));
    for (size_t i = 0; i < kNumOfWorlds; ++i) {
      scoped_refptr<DOMWrapperWorld> world = IndexToWorld(isolate, i);
      TakeSnapshotForWorld(snapshot_creator, *world);
    }
  }

  // Remove v8::Eternal in V8PerIsolateData before creating the blob.
  per_isolate_data->ClearPersistentsForV8ContextSnapshot();
  // V8Initializer::MessageHandlerInMainThread will be installed regardless of
  // the V8 context snapshot.
  isolate->RemoveMessageListeners(V8Initializer::MessageHandlerInMainThread);

  return snapshot_creator->CreateBlob(
      v8::SnapshotCreator::FunctionCodeHandling::kClear);
}

const intptr_t* V8ContextSnapshotImpl::GetReferenceTable() {
  DCHECK(IsMainThread());

  if (!IsUsingContextSnapshot())
    return nullptr;

  DEFINE_STATIC_LOCAL(const intptr_t*, reference_table, (nullptr));
  if (reference_table)
    return reference_table;

  intptr_t last_table[] = {
      reinterpret_cast<intptr_t>(V8ObjectConstructor::IsValidConstructorMode),
      0,  // nullptr termination
  };
  base::span<const intptr_t> tables[] = {
      bindings::v8_context_snapshot::GetRefTableOfV8Document(),
      bindings::v8_context_snapshot::GetRefTableOfV8EventTarget(),
      bindings::v8_context_snapshot::GetRefTableOfV8HTMLDocument(),
      bindings::v8_context_snapshot::GetRefTableOfV8Node(),
      bindings::v8_context_snapshot::GetRefTableOfV8Window(),
      last_table,
  };
  DCHECK_EQ(std::size(tables), std::size(type_info_table) + 1);

  size_t size_bytes = 0;
  for (const auto& table : tables)
    size_bytes += table.size_bytes();
  intptr_t* unified_table =
      static_cast<intptr_t*>(::WTF::Partitions::FastMalloc(
          size_bytes, "V8ContextSnapshotImpl::GetReferenceTable"));
  size_t offset_count = 0;
  for (const auto& table : tables) {
    std::memcpy(unified_table + offset_count, table.data(), table.size_bytes());
    offset_count += table.size();
  }
  reference_table = unified_table;

  return reference_table;
}

}  // namespace blink
