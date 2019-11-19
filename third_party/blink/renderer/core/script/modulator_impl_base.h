// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MODULATOR_IMPL_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MODULATOR_IMPL_BASE_H_

#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/module_record.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class DynamicModuleResolver;
class ExecutionContext;
class ModuleMap;
class ModuleTreeLinkerRegistry;
class ScriptState;

// ModulatorImplBase is the base implementation of Modulator interface, which
// represents "environment settings object" concept for module scripts.
// ModulatorImplBase serves as the backplane for tieing all ES6 module algorithm
// components together.
class ModulatorImplBase : public Modulator {
 public:
  ~ModulatorImplBase() override;
  void Trace(Visitor*) override;

 protected:
  explicit ModulatorImplBase(ScriptState*);

  ExecutionContext* GetExecutionContext() const;

  ScriptState* GetScriptState() override { return script_state_; }

 private:
  // Implements Modulator

  bool IsScriptingDisabled() const override;

  bool ImportMapsEnabled() const override;
  bool BuiltInModuleInfraEnabled() const override;
  bool BuiltInModuleEnabled(layered_api::Module) const override;
  void BuiltInModuleUseCount(layered_api::Module) const override;

  static bool BuiltInModuleRequireSecureContext(layered_api::Module);

  ModuleRecordResolver* GetModuleRecordResolver() override {
    return module_record_resolver_.Get();
  }
  base::SingleThreadTaskRunner* TaskRunner() override {
    return task_runner_.get();
  }

  void FetchTree(const KURL&,
                 ResourceFetcher* fetch_client_settings_object_fetcher,
                 mojom::RequestContextType destination,
                 const ScriptFetchOptions&,
                 ModuleScriptCustomFetchType,
                 ModuleTreeClient*) override;
  void FetchDescendantsForInlineScript(
      ModuleScript*,
      ResourceFetcher* fetch_client_settings_object_fetcher,
      mojom::RequestContextType destination,
      ModuleTreeClient*) override;
  void FetchSingle(const ModuleScriptFetchRequest&,
                   ResourceFetcher* fetch_client_settings_object_fetcher,
                   ModuleGraphLevel,
                   ModuleScriptCustomFetchType,
                   SingleModuleClient*) override;
  ModuleScript* GetFetchedModuleScript(const KURL&) override;
  bool HasValidContext() override;
  KURL ResolveModuleSpecifier(const String& module_request,
                              const KURL& base_url,
                              String* failure_reason) final;
  void ResolveDynamically(const String& specifier,
                          const KURL&,
                          const ReferrerScriptInfo&,
                          ScriptPromiseResolver*) override;
  const ImportMap* GetImportMapForTest() const final { return import_map_; }

  ScriptValue CreateTypeError(const String& message) const override;
  ScriptValue CreateSyntaxError(const String& message) const override;
  void RegisterImportMap(const ImportMap*, ScriptValue error_to_rethrow) final;
  bool IsAcquiringImportMaps() const final { return acquiring_import_maps_; }
  void ClearIsAcquiringImportMaps() final { acquiring_import_maps_ = false; }
  ModuleImportMeta HostGetImportMetaProperties(
      v8::Local<v8::Module>) const override;
  ScriptValue InstantiateModule(v8::Local<v8::Module>, const KURL&) override;
  Vector<ModuleRequest> ModuleRequestsFromModuleRecord(
      v8::Local<v8::Module>) override;
  ScriptValue ExecuteModule(ModuleScript*, CaptureEvalErrorFlag) override;

  // Populates |reason| and returns true if the dynamic import is disallowed on
  // the associated execution context. In that case, a caller of this function
  // is expected to reject the dynamic import with |reason|. If the dynamic
  // import is allowed on the execution context, returns false without
  // modification of |reason|.
  virtual bool IsDynamicImportForbidden(String* reason) = 0;

  void ProduceCacheModuleTreeTopLevel(ModuleScript*);
  void ProduceCacheModuleTree(ModuleScript*,
                              HeapHashSet<Member<const ModuleScript>>*);

  Member<ScriptState> script_state_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  Member<ModuleMap> map_;
  Member<ModuleTreeLinkerRegistry> tree_linker_registry_;
  Member<ModuleRecordResolver> module_record_resolver_;
  Member<DynamicModuleResolver> dynamic_module_resolver_;

  Member<const ImportMap> import_map_;

  // https://github.com/WICG/import-maps/blob/master/spec.md#when-import-maps-can-be-encountered
  // Each realm (environment settings object) has a boolean, acquiring import
  // maps. It is initially true. [spec text]
  bool acquiring_import_maps_ = true;
};

}  // namespace blink

#endif
