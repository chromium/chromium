// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/dummy_modulator.h"

#include "third_party/blink/renderer/bindings/core/v8/module_record.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/core/script/import_map_error.h"
#include "third_party/blink/renderer/core/script/module_record_resolver.h"

namespace blink {

namespace {

class EmptyModuleRecordResolver final : public ModuleRecordResolver {
 public:
  EmptyModuleRecordResolver() = default;

  // We ignore {Unr,R}egisterModuleScript() calls caused by
  // ModuleScript::CreateForTest().
  void RegisterModuleScript(const ModuleScript*) override {}
  void UnregisterModuleScript(const ModuleScript*) override {}

  const ModuleScript* GetModuleScriptFromModuleRecord(
      v8::Local<v8::Module>) const override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  v8::Local<v8::Module> Resolve(const ModuleRequest& module_request,
                                v8::Local<v8::Module> referrer,
                                ExceptionState&) override {
    NOTREACHED_IN_MIGRATION();
    return v8::Local<v8::Module>();
  }
};

}  // namespace

DummyModulator::DummyModulator()
    : resolver_(MakeGarbageCollected<EmptyModuleRecordResolver>()) {}

DummyModulator::~DummyModulator() = default;

void DummyModulator::Trace(Visitor* visitor) const {
  visitor->Trace(resolver_);
  Modulator::Trace(visitor);
}

ScriptState* DummyModulator::GetScriptState() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

mojom::blink::V8CacheOptions DummyModulator::GetV8CacheOptions() const {
  return mojom::blink::V8CacheOptions::kDefault;
}

bool DummyModulator::IsScriptingDisabled() const {
  return false;
}

ModuleRecordResolver* DummyModulator::GetModuleRecordResolver() {
  return resolver_.Get();
}

base::SingleThreadTaskRunner* DummyModulator::TaskRunner() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void DummyModulator::FetchTree(const KURL&,
                               ModuleType,
                               ResourceFetcher*,
                               mojom::blink::RequestContextType,
                               network::mojom::RequestDestination,
                               const ScriptFetchOptions&,
                               ModuleScriptCustomFetchType,
                               ModuleTreeClient*,
                               String referrer) {
  NOTREACHED_IN_MIGRATION();
}

void DummyModulator::FetchSingle(const ModuleScriptFetchRequest&,
                                 ResourceFetcher*,
                                 ModuleGraphLevel,
                                 ModuleScriptCustomFetchType,
                                 SingleModuleClient*) {
  NOTREACHED_IN_MIGRATION();
}

void DummyModulator::FetchDescendantsForInlineScript(
    ModuleScript*,
    ResourceFetcher*,
    mojom::blink::RequestContextType,
    network::mojom::RequestDestination,
    ModuleTreeClient*) {
  NOTREACHED_IN_MIGRATION();
}

ModuleScript* DummyModulator::GetFetchedModuleScript(const KURL&, ModuleType) {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

KURL DummyModulator::ResolveModuleSpecifier(const String&,
                                            const KURL&,
                                            String*) {
  NOTREACHED_IN_MIGRATION();
  return KURL();
}

String DummyModulator::GetIntegrityMetadataString(const KURL&) const {
  return String();
}

IntegrityMetadataSet DummyModulator::GetIntegrityMetadata(const KURL&) const {
  return IntegrityMetadataSet();
}

bool DummyModulator::HasValidContext() {
  return true;
}

void DummyModulator::ResolveDynamically(const ModuleRequest& module_request,
                                        const ReferrerScriptInfo&,
                                        ScriptPromiseResolver<IDLAny>*) {
  NOTREACHED_IN_MIGRATION();
}

ModuleImportMeta DummyModulator::HostGetImportMetaProperties(
    v8::Local<v8::Module>) const {
  NOTREACHED_IN_MIGRATION();
  return ModuleImportMeta(String());
}

ModuleType DummyModulator::ModuleTypeFromRequest(
    const ModuleRequest& module_request) const {
  String module_type_string = module_request.GetModuleTypeString();
  if (module_type_string.IsNull()) {
    // Per https://github.com/whatwg/html/pull/5883, if no type assertion is
    // provided then the import should be treated as a JavaScript module.
    return ModuleType::kJavaScript;
  } else if (module_type_string == "json") {
    // Per https://github.com/whatwg/html/pull/5658, a "json" type assertion
    // indicates that the import should be treated as a JSON module script.
    return ModuleType::kJSON;
  } else if (module_type_string == "css") {
    // Per https://github.com/whatwg/html/pull/4898, a "css" type assertion
    // indicates that the import should be treated as a CSS module script.
    return ModuleType::kCSS;
  } else {
    // Per https://github.com/whatwg/html/pull/5883, if an unsupported type
    // assertion is provided then the import should be treated as an error
    // similar to an invalid module specifier.
    return ModuleType::kInvalid;
  }
}

ModuleScriptFetcher* DummyModulator::CreateModuleScriptFetcher(
    ModuleScriptCustomFetchType,
    base::PassKey<ModuleScriptLoader> pass_key) {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void DummyModulator::ProduceCacheModuleTreeTopLevel(ModuleScript*) {}

}  // namespace blink
