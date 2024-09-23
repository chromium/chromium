/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/notreached.h"
#include "third_party/blink/public/platform/web_isolated_world_info.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_data_store.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

namespace {

constexpr bool IsMainWorldId(int32_t world_id) {
  return world_id == DOMWrapperWorld::kMainWorldId;
}

static_assert(IsMainWorldId(kMainDOMWorldId),
              "The publicly-exposed kMainWorldId constant must match "
              "the internal blink value.");

// This does not contain the main world because the WorldMap needs
// non-default hashmap traits (WTF::IntWithZeroKeyHashTraits) to contain
// it for the main world's id (0), and it may change the performance trends.
// (see https://crbug.com/704778#c6).
using WorldMap = HeapHashMap<int, WeakMember<DOMWrapperWorld>>;
static WorldMap& GetWorldMap() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<Persistent<WorldMap>>, map,
                                  ());
  Persistent<WorldMap>& persistent_map = *map;
  if (!persistent_map) {
    persistent_map = MakeGarbageCollected<WorldMap>();
  }
  return *persistent_map;
}

}  // namespace

unsigned DOMWrapperWorld::number_of_non_main_worlds_in_main_thread_ = 0;

DOMWrapperWorld* DOMWrapperWorld::Create(v8::Isolate* isolate,
                                         WorldType world_type,
                                         bool is_default_world_of_isolate) {
  DCHECK(isolate);
  DCHECK_NE(WorldType::kIsolated, world_type);
  const auto world_id = GenerateWorldIdForType(world_type);
  if (world_id.has_value()) [[likely]] {
    return MakeGarbageCollected<DOMWrapperWorld>(PassKey(), isolate, world_type,
                                                 world_id.value(),
                                                 is_default_world_of_isolate);
  }
  return nullptr;
}

DOMWrapperWorld* DOMWrapperWorld::EnsureIsolatedWorld(v8::Isolate* isolate,
                                                      int32_t world_id) {
  DCHECK(IsIsolatedWorldId(world_id));
  WorldMap& map = GetWorldMap();
  auto it = map.find(world_id);
  if (it != map.end()) {
    DOMWrapperWorld* world = it->value.Get();
    DCHECK(world->IsIsolatedWorld());
    DCHECK_EQ(world_id, world->GetWorldId());
    return world;
  }
  return MakeGarbageCollected<DOMWrapperWorld>(
      PassKey(), isolate, WorldType::kIsolated, world_id,
      /*is_default_world_of_isolate=*/false);
}

DOMWrapperWorld::DOMWrapperWorld(PassKey,
                                 v8::Isolate* isolate,
                                 WorldType world_type,
                                 int32_t world_id,
                                 bool is_default_world_of_isolate)
    : world_type_(world_type),
      world_id_(world_id),
      dom_data_store_(
          MakeGarbageCollected<DOMDataStore>(isolate,
                                             is_default_world_of_isolate)),
      v8_object_data_store_(MakeGarbageCollected<V8ObjectDataStore>()) {
  switch (world_type_) {
    case WorldType::kMain:
      // The main world is managed separately from worldMap(). See worldMap().
      break;
    case WorldType::kIsolated:
    case WorldType::kInspectorIsolated:
    case WorldType::kRegExp:
    case WorldType::kForV8ContextSnapshotNonMain:
    case WorldType::kWorkerOrWorklet:
    case WorldType::kShadowRealm: {
      WorldMap& map = GetWorldMap();
      DCHECK(!map.Contains(world_id_));
      map.insert(world_id_, this);
      if (IsMainThread())
        number_of_non_main_worlds_in_main_thread_++;
      break;
    }
  }
}

DOMWrapperWorld& DOMWrapperWorld::MainWorld(v8::Isolate* isolate) {
  DCHECK(IsMainThread());
  return V8PerIsolateData::From(isolate)->GetMainWorld();
}

void DOMWrapperWorld::AllWorldsInIsolate(
    v8::Isolate* isolate,
    HeapVector<Member<DOMWrapperWorld>>& worlds) {
  DCHECK(worlds.empty());
  WTF::CopyValuesToVector(GetWorldMap(), worlds);
  if (IsMainThread()) {
    worlds.push_back(&MainWorld(isolate));
  }
}

DOMWrapperWorld::~DOMWrapperWorld() {
  if (IsMainThread() && !IsMainWorld()) {
    number_of_non_main_worlds_in_main_thread_--;
  }
}

void DOMWrapperWorld::Dispose() {
  CHECK(!IsMainWorld());
  if (dom_data_store_) {
    // The data_store_ might be cleared on thread termination in the same
    // garbage collection cycle which prohibits accessing the references from
    // the dtor.
    dom_data_store_->Dispose();
  }
}

typedef HashMap<int, scoped_refptr<SecurityOrigin>>
    IsolatedWorldSecurityOriginMap;
static IsolatedWorldSecurityOriginMap& IsolatedWorldSecurityOrigins() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(IsolatedWorldSecurityOriginMap, map, ());
  return map;
}

static scoped_refptr<SecurityOrigin> GetIsolatedWorldSecurityOrigin(
    int32_t world_id,
    const base::UnguessableToken& cluster_id) {
  IsolatedWorldSecurityOriginMap& origins = IsolatedWorldSecurityOrigins();
  auto it = origins.find(world_id);
  if (it == origins.end())
    return nullptr;

  return it->value->GetOriginForAgentCluster(cluster_id);
}

scoped_refptr<SecurityOrigin> DOMWrapperWorld::IsolatedWorldSecurityOrigin(
    const base::UnguessableToken& cluster_id) {
  DCHECK(IsIsolatedWorld());
  return GetIsolatedWorldSecurityOrigin(GetWorldId(), cluster_id);
}

scoped_refptr<const SecurityOrigin>
DOMWrapperWorld::IsolatedWorldSecurityOrigin(
    const base::UnguessableToken& cluster_id) const {
  DCHECK(IsIsolatedWorld());
  return GetIsolatedWorldSecurityOrigin(GetWorldId(), cluster_id);
}

void DOMWrapperWorld::SetIsolatedWorldSecurityOrigin(
    int32_t world_id,
    scoped_refptr<SecurityOrigin> security_origin) {
  DCHECK(IsIsolatedWorldId(world_id));
  if (security_origin)
    IsolatedWorldSecurityOrigins().Set(world_id, std::move(security_origin));
  else
    IsolatedWorldSecurityOrigins().erase(world_id);
}

typedef HashMap<int, String> IsolatedWorldStableIdMap;
static IsolatedWorldStableIdMap& IsolatedWorldStableIds() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(IsolatedWorldStableIdMap, map, ());
  return map;
}

String DOMWrapperWorld::NonMainWorldStableId() const {
  DCHECK(!IsMainWorld());
  const auto& map = IsolatedWorldStableIds();
  const auto it = map.find(GetWorldId());
  return it != map.end() ? it->value : String();
}

void DOMWrapperWorld::SetNonMainWorldStableId(int32_t world_id,
                                              const String& stable_id) {
  DCHECK(!IsMainWorldId(world_id));
  IsolatedWorldStableIds().Set(world_id, stable_id);
}

typedef HashMap<int, String> IsolatedWorldHumanReadableNameMap;
static IsolatedWorldHumanReadableNameMap& IsolatedWorldHumanReadableNames() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(IsolatedWorldHumanReadableNameMap, map, ());
  return map;
}

String DOMWrapperWorld::NonMainWorldHumanReadableName() const {
  DCHECK(!IsMainWorld());
  const auto& map = IsolatedWorldHumanReadableNames();
  const auto it = map.find(GetWorldId());
  return it != map.end() ? it->value : String();
}

void DOMWrapperWorld::SetNonMainWorldHumanReadableName(
    int32_t world_id,
    const String& human_readable_name) {
  DCHECK(!IsMainWorldId(world_id));
  IsolatedWorldHumanReadableNames().Set(world_id, human_readable_name);
}

constinit thread_local int next_world_id =
    DOMWrapperWorld::kUnspecifiedWorldIdStart;

// static
std::optional<int> DOMWrapperWorld::GenerateWorldIdForType(
    WorldType world_type) {
  switch (world_type) {
    case WorldType::kMain:
      return kMainWorldId;
    case WorldType::kIsolated:
      // This function should not be called for IsolatedWorld because an
      // identifier for the world is given from out of DOMWrapperWorld.
      NOTREACHED();
    case WorldType::kInspectorIsolated: {
      DCHECK(IsMainThread());
      static int next_devtools_isolated_world_id =
          IsolatedWorldId::kDevToolsFirstIsolatedWorldId;
      if (next_devtools_isolated_world_id >
          IsolatedWorldId::kDevToolsLastIsolatedWorldId) {
        return std::nullopt;
      }
      return next_devtools_isolated_world_id++;
    }
    case WorldType::kRegExp:
    case WorldType::kForV8ContextSnapshotNonMain:
    case WorldType::kWorkerOrWorklet:
    case WorldType::kShadowRealm: {
      CHECK_GE(next_world_id, kUnspecifiedWorldIdStart);
      return next_world_id++;
    }
  }
  NOTREACHED();
}

// static
bool DOMWrapperWorld::ClearWrapperInAnyNonInlineStorageWorldIfEqualTo(
    ScriptWrappable* object,
    const v8::Local<v8::Object>& handle) {
  for (DOMWrapperWorld* world : GetWorldMap().Values()) {
    DOMDataStore& data_store = world->DomDataStore();
    if (data_store.ClearInMapIfEqualTo(object, handle)) {
      return true;
    }
  }
  return false;
}

// static
bool DOMWrapperWorld::ClearWrapperInAnyNonInlineStorageWorldIfEqualTo(
    ScriptWrappable* object,
    const v8::TracedReference<v8::Object>& handle) {
  for (DOMWrapperWorld* world : GetWorldMap().Values()) {
    DOMDataStore& data_store = world->DomDataStore();
    if (data_store.ClearInMapIfEqualTo(object, handle)) {
      return true;
    }
  }
  return false;
}

void DOMWrapperWorld::Trace(Visitor* visitor) const {
  visitor->Trace(dom_data_store_);
  visitor->Trace(v8_object_data_store_);
}

}  // namespace blink
