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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_DOM_WRAPPER_WORLD_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_DOM_WRAPPER_WORLD_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/web_isolated_world_ids.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "v8/include/v8.h"

namespace blink {

class DOMDataStore;
class ScriptWrappable;
class SecurityOrigin;

// This class represent a collection of DOM wrappers for a specific world. This
// is identified by a world id that is a per-thread global identifier (see
// WorldId enum).
class PLATFORM_EXPORT DOMWrapperWorld : public RefCounted<DOMWrapperWorld> {
  USING_FAST_MALLOC(DOMWrapperWorld);

 public:
  // Per-thread global identifiers for DOMWrapperWorld.
  enum WorldId : int32_t {
    kInvalidWorldId = -1,
    kMainWorldId = 0,

    kDOMWrapperWorldEmbedderWorldIdLimit =
        IsolatedWorldId::kEmbedderWorldIdLimit,
    kDOMWrapperWorldIsolatedWorldIdLimit =
        IsolatedWorldId::kIsolatedWorldIdLimit,

    // Other worlds can use IDs after this. Don't manually pick up an ID from
    // this range. generateWorldIdForType() picks it up on behalf of you.
    kUnspecifiedWorldIdStart,
  };

  enum class WorldType {
    kMain,
    kIsolated,
    kInspectorIsolated,
    kRegExp,
    kForV8ContextSnapshotNonMain,
    kWorker,
  };

  static bool IsIsolatedWorldId(int32_t world_id) {
    return DOMWrapperWorld::kMainWorldId < world_id &&
           world_id < DOMWrapperWorld::kDOMWrapperWorldIsolatedWorldIdLimit;
  }

  // Creates a world other than IsolatedWorld. Note this can return nullptr if
  // GenerateWorldIdForType fails to allocate a valid id.
  static scoped_refptr<DOMWrapperWorld> Create(v8::Isolate*, WorldType);

  // Ensures an IsolatedWorld for |worldId|.
  static scoped_refptr<DOMWrapperWorld> EnsureIsolatedWorld(v8::Isolate*,
                                                            int32_t world_id);
  ~DOMWrapperWorld();
  void Dispose();

  // Called from performance-sensitive functions, so we should keep this simple
  // and fast as much as possible.
  static bool NonMainWorldsExistInMainThread() {
    return number_of_non_main_worlds_in_main_thread_;
  }

  static void AllWorldsInCurrentThread(
      Vector<scoped_refptr<DOMWrapperWorld>>& worlds);

  static DOMWrapperWorld& World(v8::Local<v8::Context> context) {
    return ScriptState::From(context)->World();
  }

  static DOMWrapperWorld& Current(v8::Isolate* isolate) {
    return World(isolate->GetCurrentContext());
  }

  static DOMWrapperWorld& MainWorld();

  static void SetNonMainWorldHumanReadableName(int32_t world_id, const String&);
  String NonMainWorldHumanReadableName();

  // Associates an isolated world (see above for description) with a security
  // origin. XMLHttpRequest instances used in that world will be considered
  // to come from that origin, not the frame's.
  // Note: if |security_origin| is null, the security origin stored for the
  // isolated world is cleared.
  static void SetIsolatedWorldSecurityOrigin(
      int32_t world_id,
      scoped_refptr<SecurityOrigin> security_origin);
  SecurityOrigin* IsolatedWorldSecurityOrigin();

  static bool HasWrapperInAnyWorldInMainThread(ScriptWrappable*);

  bool IsMainWorld() const { return world_type_ == WorldType::kMain; }
  bool IsWorkerWorld() const { return world_type_ == WorldType::kWorker; }
  bool IsIsolatedWorld() const {
    return world_type_ == WorldType::kIsolated ||
           world_type_ == WorldType::kInspectorIsolated;
  }

  int GetWorldId() const { return world_id_; }
  DOMDataStore& DomDataStore() const { return *dom_data_store_; }

  // Clear the reference pointing from |object| to |handle| in any world.
  static bool UnsetSpecificWrapperIfSet(
      ScriptWrappable* object,
      const v8::TracedReference<v8::Object>& handle);

 private:
  static bool UnsetNonMainWorldWrapperIfSet(
      ScriptWrappable* object,
      const v8::TracedReference<v8::Object>& handle);

  DOMWrapperWorld(v8::Isolate*, WorldType, int32_t world_id);

  static unsigned number_of_non_main_worlds_in_main_thread_;

  // Returns an identifier for a given world type. This must not be called for
  // WorldType::IsolatedWorld because an identifier for the world is given from
  // out of DOMWrapperWorld.
  static int GenerateWorldIdForType(WorldType);

  const WorldType world_type_;
  const int32_t world_id_;
  Persistent<DOMDataStore> dom_data_store_;
};

// static
inline bool DOMWrapperWorld::UnsetSpecificWrapperIfSet(
    ScriptWrappable* object,
    const v8::TracedReference<v8::Object>& handle) {
  // Fast path for main world.
  if (object->UnsetMainWorldWrapperIfSet(handle))
    return true;

  // Slow path: |object| may point to |handle| in any non-main DOM world.
  return DOMWrapperWorld::UnsetNonMainWorldWrapperIfSet(object, handle);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_DOM_WRAPPER_WORLD_H_
