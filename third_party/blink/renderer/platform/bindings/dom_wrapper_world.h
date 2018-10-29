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
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "v8/include/v8.h"

namespace blink {

class DOMDataStore;
class DOMObjectHolderBase;
class ScriptWrappable;
class SecurityOrigin;

// This class represent a collection of DOM wrappers for a specific world. This
// is identified by a world id that is a per-thread global identifier (see
// WorldId enum).
class PLATFORM_EXPORT DOMWrapperWorld : public RefCounted<DOMWrapperWorld> {
 public:
  // Per-thread global identifiers for DOMWrapperWorld.
  enum WorldId {
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
    kTesting,
    kForV8ContextSnapshotNonMain,
    kWorker,
  };

  // Creates a world other than IsolatedWorld. Note this can return nullptr if
  // GenerateWorldIdForType fails to allocate a valid id.
  static scoped_refptr<DOMWrapperWorld> Create(v8::Isolate*, WorldType);

  // Ensures an IsolatedWorld for |worldId|.
  static scoped_refptr<DOMWrapperWorld> EnsureIsolatedWorld(v8::Isolate*,
                                                            int world_id);
  ~DOMWrapperWorld();
  void Dispose();

  // Called from performance-sensitive functions, so we should keep this simple
  // and fast as much as possible.
  static bool NonMainWorldsExistInMainThread() {
    return number_of_non_main_worlds_in_main_thread_;
  }

  static void AllWorldsInCurrentThread(
      Vector<scoped_refptr<DOMWrapperWorld>>& worlds);

  // Traces wrappers corresponding to the ScriptWrappable in DOM data stores.
  static void Trace(const ScriptWrappable*, Visitor*);

  static DOMWrapperWorld& World(v8::Local<v8::Context> context) {
    return ScriptState::From(context)->World();
  }

  static DOMWrapperWorld& Current(v8::Isolate* isolate) {
    return World(isolate->GetCurrentContext());
  }

  static DOMWrapperWorld& MainWorld();

  static void SetNonMainWorldHumanReadableName(int world_id, const String&);
  String NonMainWorldHumanReadableName();

  // Associates an isolated world (see above for description) with a security
  // origin. XMLHttpRequest instances used in that world will be considered
  // to come from that origin, not the frame's.
  static void SetIsolatedWorldSecurityOrigin(int world_id,
                                             scoped_refptr<SecurityOrigin>);
  SecurityOrigin* IsolatedWorldSecurityOrigin();

  // Associated an isolated world with a Content Security Policy. Resources
  // embedded into the main world's DOM from script executed in an isolated
  // world should be restricted based on the isolated world's DOM, not the
  // main world's.
  //
  // FIXME: Right now, resource injection simply bypasses the main world's
  // DOM. More work is necessary to allow the isolated world's policy to be
  // applied correctly.
  static void SetIsolatedWorldContentSecurityPolicy(int world_id,
                                                    const String& policy);
  bool IsolatedWorldHasContentSecurityPolicy();

  static bool HasWrapperInAnyWorldInMainThread(ScriptWrappable*);

  bool IsMainWorld() const { return world_type_ == WorldType::kMain; }
  bool IsWorkerWorld() const { return world_type_ == WorldType::kWorker; }
  bool IsIsolatedWorld() const {
    return world_type_ == WorldType::kIsolated ||
           world_type_ == WorldType::kInspectorIsolated;
  }

  int GetWorldId() const { return world_id_; }
  DOMDataStore& DomDataStore() const { return *dom_data_store_; }

  template <typename T>
  void RegisterDOMObjectHolder(v8::Isolate* isolate,
                               T* object,
                               v8::Local<v8::Value> wrapper) {
    RegisterDOMObjectHolderInternal(
        DOMObjectHolder<T>::Create(isolate, object, wrapper));
  }

 private:
  class DOMObjectHolderBase {
    USING_FAST_MALLOC(DOMObjectHolderBase);

   public:
    DOMObjectHolderBase(v8::Isolate* isolate, v8::Local<v8::Value> wrapper)
        : wrapper_(isolate, wrapper), world_(nullptr) {}
    virtual ~DOMObjectHolderBase() = default;

    DOMWrapperWorld* World() const { return world_; }
    void SetWorld(DOMWrapperWorld* world) { world_ = world; }
    void SetWeak(v8::WeakCallbackInfo<DOMObjectHolderBase>::Callback callback) {
      wrapper_.SetWeak(this, callback);
    }

   private:
    ScopedPersistent<v8::Value> wrapper_;
    DOMWrapperWorld* world_;
  };

  template <typename T>
  class DOMObjectHolder : public DOMObjectHolderBase {
   public:
    static std::unique_ptr<DOMObjectHolder<T>>
    Create(v8::Isolate* isolate, T* object, v8::Local<v8::Value> wrapper) {
      return base::WrapUnique(new DOMObjectHolder(isolate, object, wrapper));
    }

   private:
    DOMObjectHolder(v8::Isolate* isolate,
                    T* object,
                    v8::Local<v8::Value> wrapper)
        : DOMObjectHolderBase(isolate, wrapper), object_(object) {}

    Persistent<T> object_;
  };

  DOMWrapperWorld(v8::Isolate*, WorldType, int world_id);

  static void WeakCallbackForDOMObjectHolder(
      const v8::WeakCallbackInfo<DOMObjectHolderBase>&);
  void RegisterDOMObjectHolderInternal(std::unique_ptr<DOMObjectHolderBase>);
  void UnregisterDOMObjectHolder(DOMObjectHolderBase*);

  static unsigned number_of_non_main_worlds_in_main_thread_;

  // Returns an identifier for a given world type. This must not be called for
  // WorldType::IsolatedWorld because an identifier for the world is given from
  // out of DOMWrapperWorld.
  static int GenerateWorldIdForType(WorldType);

  // Dissociates all wrappers in all worlds associated with |script_wrappable|.
  //
  // Do not use this function except for DOMWindow.  Only DOMWindow needs to
  // dissociate wrappers from the ScriptWrappable because of the following two
  // reasons.
  //
  // Reason 1) Case of the main world
  // A DOMWindow may be collected by Blink GC *before* V8 GC collects the
  // wrapper because the wrapper object associated with a DOMWindow is a global
  // proxy, which remains after navigations.  We don't want V8 GC to reset the
  // weak persistent handle to a wrapper within the DOMWindow
  // (ScriptWrappable::main_world_wrapper_) *after* Blink GC collects the
  // DOMWindow because it's use-after-free.  Thus, we need to dissociate the
  // wrapper in advance.
  //
  // Reason 2) Case of isolated worlds
  // As same, a DOMWindow may be collected before the wrapper gets collected.
  // A DOMWrapperMap supports mapping from ScriptWrappable* to v8::Global<T>,
  // and we don't want to leave an entry of an already-dead DOMWindow* to the
  // persistent handle for the global proxy object, especially considering that
  // the address to the already-dead DOMWindow* may be re-used.
  friend class DOMWindow;
  static void DissociateDOMWindowWrappersInAllWorlds(ScriptWrappable*);

  const WorldType world_type_;
  const int world_id_;
  std::unique_ptr<DOMDataStore> dom_data_store_;
  HashSet<std::unique_ptr<DOMObjectHolderBase>> dom_object_holders_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_DOM_WRAPPER_WORLD_H_
