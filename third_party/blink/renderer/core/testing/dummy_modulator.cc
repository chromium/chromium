// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/dummy_modulator.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/script/script_module_resolver.h"

namespace blink {

namespace {

class EmptyScriptModuleResolver final : public ScriptModuleResolver {
 public:
  EmptyScriptModuleResolver() = default;

  // We ignore {Unr,R}egisterModuleScript() calls caused by
  // ModuleScript::CreateForTest().
  void RegisterModuleScript(ModuleScript*) override {}
  void UnregisterModuleScript(ModuleScript*) override {}

  ModuleScript* GetHostDefined(const ScriptModule&) const override {
    NOTREACHED();
    return nullptr;
  }

  ScriptModule Resolve(const String& specifier,
                       const ScriptModule& referrer,
                       ExceptionState&) override {
    NOTREACHED();
    return ScriptModule();
  }
};

}  // namespace

DummyModulator::DummyModulator() : resolver_(new EmptyScriptModuleResolver()) {}

DummyModulator::~DummyModulator() = default;

void DummyModulator::Trace(blink::Visitor* visitor) {
  visitor->Trace(resolver_);
  Modulator::Trace(visitor);
}

ScriptState* DummyModulator::GetScriptState() {
  NOTREACHED();
  return nullptr;
}

bool DummyModulator::IsScriptingDisabled() const {
  return false;
}

ScriptModuleResolver* DummyModulator::GetScriptModuleResolver() {
  return resolver_.Get();
}

base::SingleThreadTaskRunner* DummyModulator::TaskRunner() {
  NOTREACHED();
  return nullptr;
};

void DummyModulator::FetchTree(const KURL&,
                               FetchClientSettingsObjectSnapshot*,
                               mojom::RequestContextType,
                               const ScriptFetchOptions&,
                               ModuleScriptCustomFetchType,
                               ModuleTreeClient*) {
  NOTREACHED();
}

void DummyModulator::FetchSingle(const ModuleScriptFetchRequest&,
                                 FetchClientSettingsObjectSnapshot*,
                                 ModuleGraphLevel,
                                 ModuleScriptCustomFetchType,
                                 SingleModuleClient*) {
  NOTREACHED();
}

void DummyModulator::FetchDescendantsForInlineScript(
    ModuleScript*,
    FetchClientSettingsObjectSnapshot* fetch_client_settings_object,
    mojom::RequestContextType,
    ModuleTreeClient*) {
  NOTREACHED();
}

ModuleScript* DummyModulator::GetFetchedModuleScript(const KURL&) {
  NOTREACHED();
  return nullptr;
}

KURL DummyModulator::ResolveModuleSpecifier(const String&,
                                            const KURL&,
                                            String*) {
  NOTREACHED();
  return KURL();
}

bool DummyModulator::HasValidContext() {
  return true;
}

void DummyModulator::ResolveDynamically(const String&,
                                        const KURL&,
                                        const ReferrerScriptInfo&,
                                        ScriptPromiseResolver*) {
  NOTREACHED();
}

ModuleImportMeta DummyModulator::HostGetImportMetaProperties(
    ScriptModule) const {
  NOTREACHED();
  return ModuleImportMeta(String());
}

ScriptValue DummyModulator::InstantiateModule(ScriptModule) {
  NOTREACHED();
  return ScriptValue();
}

Vector<Modulator::ModuleRequest> DummyModulator::ModuleRequestsFromScriptModule(
    ScriptModule) {
  NOTREACHED();
  return Vector<ModuleRequest>();
}

ScriptValue DummyModulator::ExecuteModule(const ModuleScript*,
                                          CaptureEvalErrorFlag) {
  NOTREACHED();
  return ScriptValue();
}

ModuleScriptFetcher* DummyModulator::CreateModuleScriptFetcher(
    ModuleScriptCustomFetchType) {
  NOTREACHED();
  return nullptr;
}

}  // namespace blink
