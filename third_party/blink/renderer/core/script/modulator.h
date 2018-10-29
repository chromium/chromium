// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MODULATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MODULATOR_H_

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/script_module.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/script/module_import_meta.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/access_control_status.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/referrer_policy.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class FetchClientSettingsObjectSnapshot;
class ModuleScript;
class ModuleScriptFetchRequest;
class ModuleScriptFetcher;
class ReferrerScriptInfo;
class ScriptModuleResolver;
class ScriptPromiseResolver;
class ScriptState;
class ScriptValue;

// A SingleModuleClient is notified when single module script node (node as in a
// module tree graph) load is complete and its corresponding entry is created in
// module map.
class CORE_EXPORT SingleModuleClient
    : public GarbageCollectedFinalized<SingleModuleClient>,
      public NameClient {
 public:
  virtual ~SingleModuleClient() = default;
  virtual void Trace(blink::Visitor* visitor) {}
  const char* NameInHeapSnapshot() const override {
    return "SingleModuleClient";
  }

  virtual void NotifyModuleLoadFinished(ModuleScript*) = 0;
};

// A ModuleTreeClient is notified when a module script and its whole descendent
// tree load is complete.
class CORE_EXPORT ModuleTreeClient
    : public GarbageCollectedFinalized<ModuleTreeClient>,
      public NameClient {
 public:
  virtual ~ModuleTreeClient() = default;
  virtual void Trace(blink::Visitor* visitor) {}
  const char* NameInHeapSnapshot() const override { return "ModuleTreeClient"; }

  virtual void NotifyModuleTreeLoadFinished(ModuleScript*) = 0;
};

// spec: "top-level module fetch flag"
// https://html.spec.whatwg.org/multipage/webappapis.html#fetching-scripts-is-top-level
enum class ModuleGraphLevel { kTopLevelModuleFetch, kDependentModuleFetch };

// spec: "custom peform the fetch hook"
// https://html.spec.whatwg.org/multipage/webappapis.html#fetching-scripts-perform-fetch
enum class ModuleScriptCustomFetchType {
  // Fetch module scripts without invoking custom fetch steps.
  kNone,

  // Perform custom fetch steps for worker's constructor defined in the HTML
  // spec:
  // https://html.spec.whatwg.org/multipage/workers.html#worker-processing-model
  kWorkerConstructor,

  // Perform custom fetch steps for Worklet's addModule() function defined in
  // the Worklet spec:
  // https://drafts.css-houdini.org/worklets/#fetch-a-worklet-script
  kWorkletAddModule,

  // Fetch a Service Worker's installed module script from the Service Worker's
  // script storage.
  kInstalledServiceWorker
};

// A Modulator is an interface for "environment settings object" concept for
// module scripts.
// https://html.spec.whatwg.org/multipage/webappapis.html#environment-settings-object
//
// A Modulator also serves as an entry point for various module spec algorithms.
class CORE_EXPORT Modulator : public GarbageCollectedFinalized<Modulator>,
                              public V8PerContextData::Data,
                              public NameClient {
  USING_GARBAGE_COLLECTED_MIXIN(Modulator);

 public:
  static Modulator* From(ScriptState*);
  virtual ~Modulator();

  static void SetModulator(ScriptState*, Modulator*);
  static void ClearModulator(ScriptState*);

  void Trace(blink::Visitor* visitor) override {}
  const char* NameInHeapSnapshot() const override { return "Modulator"; }

  virtual ScriptModuleResolver* GetScriptModuleResolver() = 0;
  virtual base::SingleThreadTaskRunner* TaskRunner() = 0;

  virtual ScriptState* GetScriptState() = 0;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-bc-noscript
  // "scripting is disabled for settings's responsible browsing context"
  virtual bool IsScriptingDisabled() const = 0;

  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-module-script-tree
  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-module-worker-script-tree
  // Note that |this| is the "module map settings object" used in the "fetch a
  // module worker script graph" algorithm.
  virtual void FetchTree(
      const KURL&,
      FetchClientSettingsObjectSnapshot* fetch_client_settings_object,
      mojom::RequestContextType destination,
      const ScriptFetchOptions&,
      ModuleScriptCustomFetchType,
      ModuleTreeClient*) = 0;

  // Asynchronously retrieve a module script from the module map, or fetch it
  // and put it in the map if it's not there already.
  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-single-module-script
  // Note that |this| is the "module map settings object".
  virtual void FetchSingle(
      const ModuleScriptFetchRequest&,
      FetchClientSettingsObjectSnapshot* fetch_client_settings_object,
      ModuleGraphLevel,
      ModuleScriptCustomFetchType,
      SingleModuleClient*) = 0;

  virtual void FetchDescendantsForInlineScript(
      ModuleScript*,
      FetchClientSettingsObjectSnapshot* fetch_client_settings_object,
      mojom::RequestContextType destination,
      ModuleTreeClient*) = 0;

  // Synchronously retrieves a single module script from existing module map
  // entry.
  // Note: returns nullptr if the module map entry doesn't exist, or
  // is still "fetching".
  virtual ModuleScript* GetFetchedModuleScript(const KURL&) = 0;

  // https://html.spec.whatwg.org/multipage/webappapis.html#resolve-a-module-specifier
  virtual KURL ResolveModuleSpecifier(const String& module_request,
                                      const KURL& base_url,
                                      String* failure_reason = nullptr) = 0;

  // https://tc39.github.io/proposal-dynamic-import/#sec-hostimportmoduledynamically
  virtual void ResolveDynamically(const String& specifier,
                                  const KURL&,
                                  const ReferrerScriptInfo&,
                                  ScriptPromiseResolver*) = 0;

  // https://html.spec.whatwg.org/multipage/webappapis.html#hostgetimportmetaproperties
  virtual ModuleImportMeta HostGetImportMetaProperties(ScriptModule) const = 0;

  virtual bool HasValidContext() = 0;

  virtual ScriptValue InstantiateModule(ScriptModule) = 0;

  struct ModuleRequest {
    String specifier;
    TextPosition position;
    ModuleRequest(const String& specifier, const TextPosition& position)
        : specifier(specifier), position(position) {}
  };
  virtual Vector<ModuleRequest> ModuleRequestsFromScriptModule(
      ScriptModule) = 0;

  enum class CaptureEvalErrorFlag : bool { kReport, kCapture };

  // ExecuteModule implements #run-a-module-script HTML spec algorithm.
  // https://html.spec.whatwg.org/multipage/webappapis.html#run-a-module-script
  // CaptureEvalErrorFlag is used to implement "rethrow errors" parameter in
  // run-a-module-script.
  // - When "rethrow errors" is to be set, use kCapture for EvaluateModule().
  // Then EvaluateModule() returns an exception if any (instead of throwing it),
  // and the caller should rethrow the returned exception. - When "rethrow
  // errors" is not to be set, use kReport. EvaluateModule() "report the error"
  // inside it (if any), and always returns null ScriptValue().
  virtual ScriptValue ExecuteModule(const ModuleScript*,
                                    CaptureEvalErrorFlag) = 0;

  virtual ModuleScriptFetcher* CreateModuleScriptFetcher(
      ModuleScriptCustomFetchType) = 0;
};

}  // namespace blink

#endif
