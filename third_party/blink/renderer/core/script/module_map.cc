// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/module_map.h"

#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_loader.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_loader_client.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_loader_registry.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/module_script.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"

namespace blink {

// Entry struct represents a value in "module map" spec object.
// https://html.spec.whatwg.org/C/#module-map
class ModuleMap::Entry final : public GarbageCollected<Entry>,
                               public NameClient,
                               public ModuleScriptLoaderClient {
  USING_GARBAGE_COLLECTED_MIXIN(ModuleMap::Entry);

 public:
  explicit Entry(ModuleMap*);
  ~Entry() override {}

  void Trace(Visitor*) override;
  const char* NameInHeapSnapshot() const override { return "ModuleMap::Entry"; }

  // Notify fetched |m_moduleScript| to the client asynchronously.
  void AddClient(SingleModuleClient*);

  // This is only to be used from ModuleRecordResolver implementations.
  ModuleScript* GetModuleScript() const;

 private:
  void DispatchFinishedNotificationAsync(SingleModuleClient*);

  // Implements ModuleScriptLoaderClient
  void NotifyNewSingleModuleFinished(ModuleScript*) override;

  Member<ModuleScript> module_script_;
  Member<ModuleMap> map_;

  // Correspond to the HTML spec: "fetching" state.
  bool is_fetching_ = true;

  HeapHashSet<Member<SingleModuleClient>> clients_;
};

ModuleMap::Entry::Entry(ModuleMap* map) : map_(map) {
  DCHECK(map_);
}

void ModuleMap::Entry::Trace(Visitor* visitor) {
  visitor->Trace(module_script_);
  visitor->Trace(map_);
  visitor->Trace(clients_);
}

void ModuleMap::Entry::DispatchFinishedNotificationAsync(
    SingleModuleClient* client) {
  map_->GetModulator()->TaskRunner()->PostTask(
      FROM_HERE,
      WTF::Bind(&SingleModuleClient::NotifyModuleLoadFinished,
                WrapPersistent(client), WrapPersistent(module_script_.Get())));
}

void ModuleMap::Entry::AddClient(SingleModuleClient* new_client) {
  DCHECK(!clients_.Contains(new_client));
  if (!is_fetching_) {
    DCHECK(clients_.IsEmpty());
    DispatchFinishedNotificationAsync(new_client);
    return;
  }

  clients_.insert(new_client);
}

void ModuleMap::Entry::NotifyNewSingleModuleFinished(
    ModuleScript* module_script) {
  CHECK(is_fetching_);
  module_script_ = module_script;
  is_fetching_ = false;

  for (const auto& client : clients_) {
    DispatchFinishedNotificationAsync(client);
  }
  clients_.clear();
}

ModuleScript* ModuleMap::Entry::GetModuleScript() const {
  return module_script_.Get();
}

ModuleMap::ModuleMap(Modulator* modulator)
    : modulator_(modulator),
      loader_registry_(MakeGarbageCollected<ModuleScriptLoaderRegistry>()) {
  DCHECK(modulator);
}

void ModuleMap::Trace(Visitor* visitor) {
  visitor->Trace(map_);
  visitor->Trace(modulator_);
  visitor->Trace(loader_registry_);
}

// <specdef href="https://html.spec.whatwg.org/C/#fetch-a-single-module-script">
void ModuleMap::FetchSingleModuleScript(
    const ModuleScriptFetchRequest& request,
    ResourceFetcher* fetch_client_settings_object_fetcher,
    ModuleGraphLevel level,
    ModuleScriptCustomFetchType custom_fetch_type,
    SingleModuleClient* client) {
  // <spec step="1">Let moduleMap be module map settings object's module
  // map.</spec>
  //
  // Note: |this| is the ModuleMap.

  // <spec step="2">If moduleMap[url] is "fetching", wait in parallel until that
  // entry's value changes, then queue a task on the networking task source to
  // proceed with running the following steps.</spec>
  MapImpl::AddResult result = map_.insert(request.Url(), nullptr);
  Member<Entry>& entry = result.stored_value->value;
  if (result.is_new_entry) {
    entry = MakeGarbageCollected<Entry>(this);

    // Steps 4-9 loads a new single module script.
    // Delegates to ModuleScriptLoader via Modulator.
    ModuleScriptLoader::Fetch(request, fetch_client_settings_object_fetcher,
                              level, modulator_, custom_fetch_type,
                              loader_registry_, entry);
  }
  DCHECK(entry);

  // <spec step="3">If moduleMap[url] exists, asynchronously complete this
  // algorithm with moduleMap[url], and abort these steps.</spec>
  //
  // <spec step="14">Set moduleMap[url] to module script, and asynchronously
  // complete this algorithm with module script.</spec>
  if (client)
    entry->AddClient(client);
}

ModuleScript* ModuleMap::GetFetchedModuleScript(const KURL& url) const {
  MapImpl::const_iterator it = map_.find(url);
  if (it == map_.end())
    return nullptr;
  return it->value->GetModuleScript();
}

}  // namespace blink
