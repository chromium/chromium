// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MODULE_RECORD_RESOLVER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MODULE_RECORD_RESOLVER_IMPL_H_

#include "third_party/blink/renderer/bindings/core/v8/boxed_v8_module.h"
#include "third_party/blink/renderer/bindings/core/v8/module_record.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/script/module_record_resolver.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Modulator;
class ModuleScript;
class ModuleRecord;

// The ModuleRecordResolverImpl implements ModuleRecordResolver interface
// and implements "HostResolveImportedModule" HTML spec algorithm to bridge
// ModuleMap (via Modulator) and V8 bindings.
class CORE_EXPORT ModuleRecordResolverImpl final
    : public ModuleRecordResolver,
      public ContextLifecycleObserver {
 public:
  explicit ModuleRecordResolverImpl(Modulator* modulator,
                                    ExecutionContext* execution_context)
      : ContextLifecycleObserver(execution_context), modulator_(modulator) {}

  void Trace(Visitor*) override;
  USING_GARBAGE_COLLECTED_MIXIN(ModuleRecordResolverImpl);

 private:
  // Implements ModuleRecordResolver:

  void RegisterModuleScript(const ModuleScript*) final;
  void UnregisterModuleScript(const ModuleScript*) final;
  const ModuleScript* GetModuleScriptFromModuleRecord(
      v8::Local<v8::Module>) const final;

  // Implements "Runtime Semantics: HostResolveImportedModule" per HTML spec.
  // https://html.spec.whatwg.org/C/#hostresolveimportedmodule(referencingscriptormodule,-specifier))
  v8::Local<v8::Module> Resolve(const String& specifier,
                                v8::Local<v8::Module> referrer,
                                ExceptionState&) final;

  // Implements ContextLifecycleObserver:
  void ContextDestroyed(ExecutionContext*) final;

  // Corresponds to the spec concept "referencingModule.[[HostDefined]]".
  // crbug.com/725816 : ModuleRecord contains strong ref to v8::Module thus we
  // should not use ModuleRecord as the map key. We currently rely on Detach()
  // to clear the refs, but we should implement a key type which keeps a
  // weak-ref to v8::Module.
  HeapHashMap<Member<BoxedV8Module>, Member<const ModuleScript>>
      record_to_module_script_map_;
  Member<Modulator> modulator_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MODULE_RECORD_RESOLVER_IMPL_H_
