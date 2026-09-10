// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MODULE_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MODULE_MAP_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"

namespace blink {

class Modulator;
class ModuleScript;
class ModuleScriptFetchRequest;
class ModuleScriptLoaderRegistry;
class ResourceFetcher;
class SingleModuleClient;
enum class ModuleGraphLevel;
enum class ModuleScriptCustomFetchType;
enum class ModuleType;

// A ModuleMap implements "module map" spec.
// https://html.spec.whatwg.org/C/#module-map
class CORE_EXPORT ModuleMap final : public GarbageCollected<ModuleMap>,
                                    public NameClient {
  class Entry;

 public:
  ModuleMap(const ModuleMap&) = delete;
  ModuleMap& operator=(const ModuleMap&) = delete;
  explicit ModuleMap(Modulator*);
  ~ModuleMap() override = default;

  void Trace(Visitor*) const;
  const char* GetHumanReadableName() const override { return "ModuleMap"; }

  // https://html.spec.whatwg.org/C/#fetch-a-single-module-script
  void FetchSingleModuleScript(
      const ModuleScriptFetchRequest&,
      ResourceFetcher* fetch_client_settings_object_fetcher,
      ModuleGraphLevel,
      ModuleScriptCustomFetchType,
      SingleModuleClient*);

  // Synchronously get the ModuleScript for a given URL.
  // If the URL wasn't fetched, or is currently being fetched, this returns a
  // nullptr.
  ModuleScript* GetFetchedModuleScript(const KURL&, ModuleType) const;

  void AddEntry(const KURL& url, ModuleType type, ModuleScript* script);

  Modulator* GetModulator() { return modulator_.Get(); }

 private:
  using Key = std::pair<KURL, ModuleType>;
  using MapImpl = HeapHashMap<Key, Member<Entry>>;

  // Returns the entry for `key`, creating and inserting one if necessary, and
  // sets `*is_new_entry` accordingly. The HeapHashMap::AddResult is confined to
  // this method so it cannot outlive the insertion: the caller goes on to
  // fetch, which may synchronously evict the entry from `map_` (failures aren't
  // cached), and a live AddResult would then dangle and trip its
  // modification-count check.
  Entry* GetOrCreateEntry(const Key& key, bool* is_new_entry);

  // Removes the entry under `key`. Used by an Entry to evict itself on a failed
  // fetch, so failures are not cached. `entry_to_remove` must be the entry
  // currently mapped to `key`.
  // https://html.spec.whatwg.org/C/#fetch-a-single-module-script
  void RemoveEntry(const Key& key, Entry* entry_to_remove);

  // A module map is a map of absolute URLs to map entry.
  MapImpl map_;

  Member<Modulator> modulator_;
  Member<ModuleScriptLoaderRegistry> loader_registry_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_MODULE_MAP_H_
