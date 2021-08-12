// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MODULATOR_IMPL_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MODULATOR_IMPL_BASE_H_

#include "base/single_thread_task_runner.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/module_record.h"
#include "third_party/blink/renderer/core/script/import_map_error.h"
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
  void Trace(Visitor*) const override;

 protected:
  explicit ModulatorImplBase(ScriptState*);

  ExecutionContext* GetExecutionContext() const;

  ScriptState* GetScriptState() override { return script_state_; }

 private:
  // Implements Modulator

  bool IsScriptingDisabled() const override;

  mojom::blink::V8CacheOptions GetV8CacheOptions() const final;

  ModuleRecordResolver* GetModuleRecordResolver() override {
    return module_record_resolver_.Get();
  }
  base::SingleThreadTaskRunner* TaskRunner() override {
    return task_runner_.get();
  }

  void FetchTree(const KURL&,
                 ModuleType,
                 ResourceFetcher* fetch_client_settings_object_fetcher,
                 mojom::blink::RequestContextType context_type,
                 network::mojom::RequestDestination destination,
                 const ScriptFetchOptions&,
                 ModuleScriptCustomFetchType,
                 ModuleTreeClient*) override;
  void FetchDescendantsForInlineScript(
      ModuleScript*,
      ResourceFetcher* fetch_client_settings_object_fetcher,
      mojom::blink::RequestContextType context_type,
      network::mojom::RequestDestination destination,
      ModuleTreeClient*) override;
  void FetchSingle(const ModuleScriptFetchRequest&,
                   ResourceFetcher* fetch_client_settings_object_fetcher,
                   ModuleGraphLevel,
                   ModuleScriptCustomFetchType,
                   SingleModuleClient*) override;
  ModuleScript* GetFetchedModuleScript(const KURL&, ModuleType) override;
  bool HasValidContext() override;
  KURL ResolveModuleSpecifier(const String& module_request,
                              const KURL& base_url,
                              String* failure_reason) final;
  void ResolveDynamically(const ModuleRequest& module_request,
                          const ReferrerScriptInfo&,
                          ScriptPromiseResolver*) override;
  const ImportMap* GetImportMapForTest() const final { return import_map_; }

  void RegisterImportMap(const ImportMap*,
                         absl::optional<ImportMapError> error_to_rethrow) final;
  AcquiringImportMapsState GetAcquiringImportMapsState() const final {
    return acquiring_import_maps_;
  }
  void SetAcquiringImportMapsState(AcquiringImportMapsState value) final {
    acquiring_import_maps_ = value;
  }
  ModuleImportMeta HostGetImportMetaProperties(
      v8::Local<v8::Module>) const override;
  ModuleType ModuleTypeFromRequest(
      const ModuleRequest& module_request) const override;

  // Populates |reason| and returns true if the dynamic import is disallowed on
  // the associated execution context. In that case, a caller of this function
  // is expected to reject the dynamic import with |reason|. If the dynamic
  // import is allowed on the execution context, returns false without
  // modification of |reason|.
  virtual bool IsDynamicImportForbidden(String* reason) = 0;

  void ProduceCacheModuleTreeTopLevel(ModuleScript*) override;
  void ProduceCacheModuleTree(ModuleScript*,
                              HeapHashSet<Member<const ModuleScript>>*);

  Member<ScriptState> script_state_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  Member<ModuleMap> map_;
  Member<ModuleTreeLinkerRegistry> tree_linker_registry_;
  Member<ModuleRecordResolver> module_record_resolver_;
  Member<DynamicModuleResolver> dynamic_module_resolver_;

  Member<const ImportMap> import_map_;

  // https://wicg.github.io/import-maps/#document-acquiring-import-maps
  // Each Document has an acquiring import maps boolean. It is initially true.
  // [spec text]
  AcquiringImportMapsState acquiring_import_maps_ =
      AcquiringImportMapsState::kAcquiring;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MODULATOR_IMPL_BASE_H_
