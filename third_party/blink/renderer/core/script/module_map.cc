// Copyright 2017 The Chromium Authors
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
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// Entry struct represents a value in "module map" spec object.
// https://html.spec.whatwg.org/C/#module-map
class ModuleMap::Entry final : public GarbageCollected<Entry>,
                               public NameClient,
                               public ModuleScriptLoaderClient {

 public:
  Entry(ModuleMap*, const Key& key);
  ~Entry() override = default;

  void Trace(Visitor*) const override;
  const char* GetHumanReadableName() const override {
    return "ModuleMap::Entry";
  }

  // Notify fetched |m_moduleScript| to the client asynchronously.
  void AddClient(SingleModuleClient*, ModuleImportPhase);

  // Set a module script that has been created outside of a fetch context.
  void SetModuleScript(ModuleScript*);

  // This is only to be used from ModuleRecordResolver implementations.
  ModuleScript* GetModuleScript() const;

 private:
  void DispatchFinishedNotificationAsync(SingleModuleClient*,
                                         ModuleImportPhase);

  // Implements ModuleScriptLoaderClient
  void NotifyNewSingleModuleFinished(ModuleScript*, ModuleImportPhase) override;

  Member<ModuleScript> module_script_;
  Member<ModuleMap> map_;

  // Key under which this entry is stored in `map_`; used to evict on failure.
  const Key key_;

  // Correspond to the HTML spec: "fetching" state.
  bool is_fetching_ = true;

  HeapHashSet<Member<SingleModuleClient>> clients_;
};

ModuleMap::Entry::Entry(ModuleMap* map, const Key& key) : map_(map), key_(key) {
  DCHECK(map_);
}

void ModuleMap::Entry::Trace(Visitor* visitor) const {
  visitor->Trace(module_script_);
  visitor->Trace(map_);
  visitor->Trace(clients_);
}

void ModuleMap::Entry::DispatchFinishedNotificationAsync(
    SingleModuleClient* client,
    ModuleImportPhase import_phase) {
  map_->GetModulator()->TaskRunner()->PostTask(
      FROM_HERE,
      blink::BindOnce(&SingleModuleClient::NotifyModuleLoadFinished,
                      WrapPersistent(client),
                      WrapPersistent(module_script_.Get()), import_phase));
}

void ModuleMap::Entry::AddClient(SingleModuleClient* new_client,
                                 ModuleImportPhase import_phase) {
  DCHECK(!clients_.Contains(new_client));
  if (!is_fetching_) {
    DCHECK(clients_.empty());
    DispatchFinishedNotificationAsync(new_client, import_phase);
    return;
  }

  clients_.insert(new_client);
}

void ModuleMap::Entry::NotifyNewSingleModuleFinished(
    ModuleScript* module_script,
    ModuleImportPhase import_phase) {
  CHECK(is_fetching_);
  module_script_ = module_script;
  is_fetching_ = false;

  // A null `module_script_` means the fetch failed (network error, non-ok
  // status, or MIME type mismatch). Such failures must not be cached, so remove
  // the entry and let a later import re-fetch. `this` is garbage-collected, so
  // the removal doesn't destroy it, and the clients below are still notified
  // with the null module script.
  // <spec href="https://html.spec.whatwg.org/C/#fetch-a-single-module-script"
  // step="9">If moduleScript is null, then remove moduleMap[(url, moduleType)];
  // otherwise set moduleMap[(url, moduleType)] to moduleScript.</spec>
  if (!module_script_ &&
      RuntimeEnabledFeatures::ModuleMapDoNotCacheFailedFetchEnabled()) {
    map_->RemoveEntry(key_, this);
  }

  for (const auto& client : clients_) {
    DispatchFinishedNotificationAsync(client, import_phase);
  }
  clients_.clear();
}

void ModuleMap::Entry::SetModuleScript(ModuleScript* module_script) {
  CHECK(clients_.empty());
  CHECK(!module_script_);
  module_script_ = module_script;
  is_fetching_ = false;
}

ModuleScript* ModuleMap::Entry::GetModuleScript() const {
  return module_script_.Get();
}

ModuleMap::ModuleMap(Modulator* modulator)
    : modulator_(modulator),
      loader_registry_(MakeGarbageCollected<ModuleScriptLoaderRegistry>()) {
  DCHECK(modulator);
}

void ModuleMap::Trace(Visitor* visitor) const {
  visitor->Trace(map_);
  visitor->Trace(modulator_);
  visitor->Trace(loader_registry_);
}

ModuleMap::Entry* ModuleMap::GetOrCreateEntry(const Key& key,
                                              bool* is_new_entry) {
  MapImpl::AddResult result = map_.insert(key, nullptr);
  *is_new_entry = result.is_new_entry;
  if (!result.is_new_entry) {
    return result.stored_value->value.Get();
  }
  Entry* entry = MakeGarbageCollected<Entry>(this, key);
  result.stored_value->value = entry;
  return entry;
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
  const Key key =
      std::make_pair(request.Url(), request.GetExpectedModuleType());
  // `entry` stays valid across the Fetch() below (which may synchronously evict
  // it from `map_`) because it is an on-stack strong reference.
  bool is_new_entry = false;
  Entry* entry = GetOrCreateEntry(key, &is_new_entry);
  if (is_new_entry) {
    // Steps 4-9 loads a new single module script.
    // Delegates to ModuleScriptLoader via Modulator. This may synchronously
    // complete the fetch and, on failure, remove this entry from `map_`.
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
    entry->AddClient(client, request.GetModuleImportPhase());
}

ModuleScript* ModuleMap::GetFetchedModuleScript(const KURL& url,
                                                ModuleType module_type) const {
  MapImpl::const_iterator it = map_.find(std::make_pair(url, module_type));
  if (it == map_.end())
    return nullptr;
  return it->value->GetModuleScript();
}

void ModuleMap::AddEntry(const KURL& url,
                         ModuleType type,
                         ModuleScript* script) {
  const Key key = std::make_pair(url, type);
  Entry* entry = MakeGarbageCollected<Entry>(this, key);
  entry->SetModuleScript(script);

  // TODO(crbug.com/448174611) - what should happen with duplicate entries?
  map_.insert(key, entry);
}

void ModuleMap::RemoveEntry(const Key& key, Entry* entry_to_remove) {
  CHECK(map_.Contains(key));
  CHECK_EQ(map_.at(key), entry_to_remove);
  map_.erase(key);
}

}  // namespace blink
