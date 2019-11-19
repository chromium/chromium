// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_DUMMY_MODULATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_DUMMY_MODULATOR_H_

#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/module_record.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ModuleRecordResolver;

// DummyModulator provides empty Modulator interface implementation w/
// NOTREACHED().
//
// DummyModulator is useful for unit-testing.
// Not all module implementation components require full-blown Modulator
// implementation. Unit tests can implement a subset of Modulator interface
// which is exercised from unit-under-test.
class DummyModulator : public Modulator {
  DISALLOW_COPY_AND_ASSIGN(DummyModulator);

 public:
  DummyModulator();
  ~DummyModulator() override;
  void Trace(blink::Visitor*) override;

  ModuleRecordResolver* GetModuleRecordResolver() override;
  base::SingleThreadTaskRunner* TaskRunner() override;
  ScriptState* GetScriptState() override;
  V8CacheOptions GetV8CacheOptions() const override;
  bool IsScriptingDisabled() const override;

  bool ImportMapsEnabled() const override;
  bool BuiltInModuleInfraEnabled() const override;
  bool BuiltInModuleEnabled(blink::layered_api::Module) const override;
  void BuiltInModuleUseCount(blink::layered_api::Module) const override;

  void FetchTree(const KURL&,
                 ResourceFetcher*,
                 mojom::RequestContextType destination,
                 const ScriptFetchOptions&,
                 ModuleScriptCustomFetchType,
                 ModuleTreeClient*) override;
  void FetchSingle(const ModuleScriptFetchRequest&,
                   ResourceFetcher*,
                   ModuleGraphLevel,
                   ModuleScriptCustomFetchType,
                   SingleModuleClient*) override;
  void FetchDescendantsForInlineScript(ModuleScript*,
                                       ResourceFetcher*,
                                       mojom::RequestContextType destination,
                                       ModuleTreeClient*) override;
  ModuleScript* GetFetchedModuleScript(const KURL&) override;
  KURL ResolveModuleSpecifier(const String&, const KURL&, String*) override;
  bool HasValidContext() override;
  void ResolveDynamically(const String& specifier,
                          const KURL&,
                          const ReferrerScriptInfo&,
                          ScriptPromiseResolver*) override;
  ScriptValue CreateTypeError(const String& message) const override;
  ScriptValue CreateSyntaxError(const String& message) const override;
  void RegisterImportMap(const ImportMap*,
                         ScriptValue error_to_rethrow) override;
  bool IsAcquiringImportMaps() const override;
  void ClearIsAcquiringImportMaps() override;
  ModuleImportMeta HostGetImportMetaProperties(
      v8::Local<v8::Module>) const override;
  const ImportMap* GetImportMapForTest() const override;
  ScriptValue InstantiateModule(v8::Local<v8::Module>, const KURL&) override;
  Vector<ModuleRequest> ModuleRequestsFromModuleRecord(
      v8::Local<v8::Module>) override;
  ScriptValue ExecuteModule(ModuleScript*, CaptureEvalErrorFlag) override;
  ModuleScriptFetcher* CreateModuleScriptFetcher(
      ModuleScriptCustomFetchType) override;

  Member<ModuleRecordResolver> resolver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_DUMMY_MODULATOR_H_
