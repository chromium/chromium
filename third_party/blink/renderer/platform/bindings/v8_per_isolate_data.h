/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_PER_ISOLATE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_PER_ISOLATE_DATA_H_

#include <memory>

#include "base/containers/span.h"
#include "gin/public/gin_embedders.h"
#include "gin/public/isolate_holder.h"
#include "third_party/blink/renderer/platform/bindings/active_script_wrappable_manager.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "v8/include/v8-callbacks.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-persistent-handle.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace blink {

class DOMWrapperWorld;
class ScriptState;
class StringCache;
class ThreadDebugger;
class V8PrivateProperty;
struct WrapperTypeInfo;

// Used to hold data that is associated with a single v8::Isolate object, and
// has a 1:1 relationship with v8::Isolate.
class PLATFORM_EXPORT V8PerIsolateData final {
  USING_FAST_MALLOC(V8PerIsolateData);

 public:
  enum class V8ContextSnapshotMode {
    kTakeSnapshot,
    kDontUseSnapshot,
    kUseSnapshot,
  };

  // Disables the UseCounter.
  // UseCounter depends on the current context, but it's not available during
  // the initialization of v8::Context and the global object.  So we need to
  // disable the UseCounter while the initialization of the context and global
  // object.
  // TODO(yukishiino): Come up with an idea to remove this hack.
  class UseCounterDisabledScope {
    STACK_ALLOCATED();

   public:
    explicit UseCounterDisabledScope(V8PerIsolateData* per_isolate_data)
        : per_isolate_data_(per_isolate_data),
          original_use_counter_disabled_(
              per_isolate_data_->use_counter_disabled_) {
      per_isolate_data_->use_counter_disabled_ = true;
    }
    ~UseCounterDisabledScope() {
      per_isolate_data_->use_counter_disabled_ = original_use_counter_disabled_;
    }

   private:
    V8PerIsolateData* per_isolate_data_;
    const bool original_use_counter_disabled_;
  };

  // Pointers to core/ objects that are garbage collected. Receives callback
  // when V8PerIsolateData will be destroyed.
  class PLATFORM_EXPORT GarbageCollectedData
      : public GarbageCollected<GarbageCollectedData> {
   public:
    virtual ~GarbageCollectedData() = default;
    virtual void WillBeDestroyed() {}
    virtual void Trace(Visitor*) const {}
  };

  static v8::Isolate* Initialize(scoped_refptr<base::SingleThreadTaskRunner>,
                                 scoped_refptr<base::SingleThreadTaskRunner>,
                                 V8ContextSnapshotMode,
                                 v8::CreateHistogramCallback,
                                 v8::AddHistogramSampleCallback);

  static V8PerIsolateData* From(v8::Isolate* isolate) {
    DCHECK(isolate);
    DCHECK(isolate->GetData(gin::kEmbedderBlink));
    return static_cast<V8PerIsolateData*>(
        isolate->GetData(gin::kEmbedderBlink));
  }

  V8PerIsolateData(const V8PerIsolateData&) = delete;
  V8PerIsolateData& operator=(const V8PerIsolateData&) = delete;

  static void WillBeDestroyed(v8::Isolate*);
  static void Destroy(v8::Isolate*);
  static v8::Isolate* MainThreadIsolate();

  static void EnableIdleTasks(v8::Isolate*,
                              std::unique_ptr<gin::V8IdleTaskRunner>);

  v8::Isolate* GetIsolate() { return isolate_holder_.isolate(); }

  StringCache* GetStringCache() { return string_cache_.get(); }

  RuntimeCallStats* GetRuntimeCallStats() { return &runtime_call_stats_; }

  bool IsHandlingRecursionLevelError() const {
    return is_handling_recursion_level_error_;
  }
  void SetIsHandlingRecursionLevelError(bool value) {
    is_handling_recursion_level_error_ = value;
  }

  bool IsUseCounterDisabled() const { return use_counter_disabled_; }

  V8PrivateProperty* PrivateProperty() { return private_property_.get(); }

  // Accessors to the cache of v8::Templates.
  v8::Local<v8::Template> FindV8Template(const DOMWrapperWorld& world,
                                         const void* key);
  void AddV8Template(const DOMWrapperWorld& world,
                     const void* key,
                     v8::Local<v8::Template> value);

  bool HasInstance(const WrapperTypeInfo* wrapper_type_info,
                   v8::Local<v8::Value> untrusted_value);
  bool HasInstanceOfUntrustedType(
      const WrapperTypeInfo* untrusted_wrapper_type_info,
      v8::Local<v8::Value> untrusted_value);

  // When v8::SnapshotCreator::CreateBlob() is called, we must not have
  // persistent handles in Blink. This method clears them.
  void ClearPersistentsForV8ContextSnapshot();

  v8::SnapshotCreator* GetSnapshotCreator() const {
    return isolate_holder_.snapshot_creator();
  }
  V8ContextSnapshotMode GetV8ContextSnapshotMode() const {
    return v8_context_snapshot_mode_;
  }

  // Obtains a pointer to an array of names, given a lookup key. If it does not
  // yet exist, it is created from the given array of strings. Once created,
  // these live for as long as the isolate, so this is appropriate only for a
  // compile-time list of related names, such as IDL dictionary keys.
  const base::span<const v8::Eternal<v8::Name>> FindOrCreateEternalNameCache(
      const void* lookup_key,
      const base::span<const char* const>& names);

  v8::Local<v8::Context> EnsureScriptRegexpContext();
  void ClearScriptRegexpContext();

  ThreadDebugger* GetThreadDebugger() const { return thread_debugger_.get(); }
  void SetThreadDebugger(std::unique_ptr<ThreadDebugger> thread_debugger);

  void SetProfilerGroup(V8PerIsolateData::GarbageCollectedData*);
  V8PerIsolateData::GarbageCollectedData* ProfilerGroup();

  void SetCanvasResourceTracker(V8PerIsolateData::GarbageCollectedData*);
  V8PerIsolateData::GarbageCollectedData* CanvasResourceTracker();

  ActiveScriptWrappableManager* GetActiveScriptWrappableManager() const {
    return active_script_wrappable_manager_;
  }

  void SetActiveScriptWrappableManager(ActiveScriptWrappableManager* manager) {
    DCHECK(manager);
    active_script_wrappable_manager_ = manager;
  }

  void SetGCCallbacks(v8::Isolate* isolate,
                      v8::Isolate::GCCallback prologue_callback,
                      v8::Isolate::GCCallback epilogue_callback);

  void EnterGC() { gc_callback_depth_++; }

  void LeaveGC() { gc_callback_depth_--; }

 private:
  V8PerIsolateData(scoped_refptr<base::SingleThreadTaskRunner>,
                   scoped_refptr<base::SingleThreadTaskRunner>,
                   V8ContextSnapshotMode,
                   v8::CreateHistogramCallback,
                   v8::AddHistogramSampleCallback);
  ~V8PerIsolateData();

  // A really simple hash function, which makes lookups faster. The set of
  // possible keys for this is relatively small and fixed at compile time, so
  // collisions are less of a worry than they would otherwise be.
  struct SimplePtrHashTraits : public GenericHashTraits<const void*> {
    static unsigned GetHash(const void* key) {
      uintptr_t k = reinterpret_cast<uintptr_t>(key);
      return static_cast<unsigned>(k ^ (k >> 8));
    }
  };
  using V8TemplateMap =
      HashMap<const void*, v8::Eternal<v8::Template>, SimplePtrHashTraits>;
  V8TemplateMap& SelectV8TemplateMap(const DOMWrapperWorld&);
  bool HasInstance(const WrapperTypeInfo* wrapper_type_info,
                   v8::Local<v8::Value> untrusted_value,
                   const V8TemplateMap& map);
  bool HasInstanceOfUntrustedType(
      const WrapperTypeInfo* untrusted_wrapper_type_info,
      v8::Local<v8::Value> untrusted_value,
      const V8TemplateMap& map);

  V8ContextSnapshotMode v8_context_snapshot_mode_;

  // This isolate_holder_ must be initialized before initializing some other
  // members below.
  gin::IsolateHolder isolate_holder_;

  // v8::Template cache of interface objects, namespace objects, etc.
  V8TemplateMap v8_template_map_for_main_world_;
  V8TemplateMap v8_template_map_for_non_main_worlds_;

  // Contains lists of eternal names, such as dictionary keys.
  HashMap<const void*, Vector<v8::Eternal<v8::Name>>> eternal_name_cache_;

  std::unique_ptr<StringCache> string_cache_;
  std::unique_ptr<V8PrivateProperty> private_property_;
  Persistent<ScriptState> script_regexp_script_state_;

  bool constructor_mode_;
  friend class ConstructorMode;

  bool use_counter_disabled_ = false;
  friend class UseCounterDisabledScope;

  bool is_handling_recursion_level_error_ = false;

  Vector<base::OnceClosure> end_of_scope_tasks_;
  std::unique_ptr<ThreadDebugger> thread_debugger_;
  Persistent<GarbageCollectedData> profiler_group_;
  Persistent<GarbageCollectedData> canvas_resource_tracker_;

  Persistent<ActiveScriptWrappableManager> active_script_wrappable_manager_;

  RuntimeCallStats runtime_call_stats_;

  v8::Isolate::GCCallback prologue_callback_;
  v8::Isolate::GCCallback epilogue_callback_;
  size_t gc_callback_depth_ = 0;
};

// Creates a histogram for V8. The returned value is a base::Histogram, but
// typed to void* for v8.
PLATFORM_EXPORT void* CreateHistogram(const char* name,
                                      int min,
                                      int max,
                                      size_t buckets);

// Adds an entry to the supplied histogram. `hist` was previously returned from
// CreateHistogram().
PLATFORM_EXPORT void AddHistogramSample(void* hist, int sample);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_PER_ISOLATE_DATA_H_
