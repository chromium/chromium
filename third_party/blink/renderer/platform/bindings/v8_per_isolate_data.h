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

#include "base/macros.h"
#include "gin/public/gin_embedders.h"
#include "gin/public/isolate_holder.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/scoped_persistent.h"
#include "third_party/blink/renderer/platform/bindings/v8_global_value_map.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "v8/include/v8.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

class ActiveScriptWrappableBase;
class DOMWrapperWorld;
class ScriptState;
class StringCache;
class V8PrivateProperty;
struct WrapperTypeInfo;

// Used to hold data that is associated with a single v8::Isolate object, and
// has a 1:1 relationship with v8::Isolate.
class PLATFORM_EXPORT V8PerIsolateData {
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

  // Use this class to abstract away types of members that are pointers to core/
  // objects, which are simply owned and released by V8PerIsolateData (see
  // m_threadDebugger for an example).
  class PLATFORM_EXPORT Data {
   public:
    virtual ~Data() = default;
  };

  // Pointers to core/ objects that are garbage collected. Receives callback
  // when V8PerIsolateData will be destroyed.
  class PLATFORM_EXPORT GarbageCollectedData
      : public GarbageCollected<GarbageCollectedData> {
   public:
    virtual ~GarbageCollectedData() = default;
    virtual void WillBeDestroyed() {}
    virtual void Trace(blink::Visitor*) {}
  };

  static v8::Isolate* Initialize(scoped_refptr<base::SingleThreadTaskRunner>,
                                 V8ContextSnapshotMode);

  static V8PerIsolateData* From(v8::Isolate* isolate) {
    DCHECK(isolate);
    DCHECK(isolate->GetData(gin::kEmbedderBlink));
    return static_cast<V8PerIsolateData*>(
        isolate->GetData(gin::kEmbedderBlink));
  }

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

  bool IsReportingException() const { return is_reporting_exception_; }
  void SetReportingException(bool value) { is_reporting_exception_ = value; }

  bool IsUseCounterDisabled() const { return use_counter_disabled_; }

  V8PrivateProperty* PrivateProperty() { return private_property_.get(); }

  // Accessors to the cache of interface templates.
  v8::Local<v8::FunctionTemplate> FindInterfaceTemplate(const DOMWrapperWorld&,
                                                        const void* key);
  void SetInterfaceTemplate(const DOMWrapperWorld&,
                            const void* key,
                            v8::Local<v8::FunctionTemplate>);

  // When v8::SnapshotCreator::CreateBlob() is called, we must not have
  // persistent handles in Blink. This method clears them.
  void ClearPersistentsForV8ContextSnapshot();

  v8::SnapshotCreator* GetSnapshotCreator() const {
    return isolate_holder_.snapshot_creator();
  }
  V8ContextSnapshotMode GetV8ContextSnapshotMode() const {
    return v8_context_snapshot_mode_;
  }
  void BailoutAndDisableV8ContextSnapshot() {
    DCHECK_EQ(V8ContextSnapshotMode::kUseSnapshot, v8_context_snapshot_mode_);
    v8_context_snapshot_mode_ = V8ContextSnapshotMode::kDontUseSnapshot;
  }

  // Accessor to the cache of cross-origin accessible operation's templates.
  // Created templates get automatically cached.
  v8::Local<v8::FunctionTemplate> FindOrCreateOperationTemplate(
      const DOMWrapperWorld&,
      const void* key,
      v8::FunctionCallback,
      v8::Local<v8::Value> data,
      v8::Local<v8::Signature>,
      int length);

  // Obtains a pointer to an array of names, given a lookup key. If it does not
  // yet exist, it is created from the given array of strings. Once created,
  // these live for as long as the isolate, so this is appropriate only for a
  // compile-time list of related names, such as IDL dictionary keys.
  const v8::Eternal<v8::Name>* FindOrCreateEternalNameCache(
      const void* lookup_key,
      const char* const names[],
      size_t count);

  bool HasInstance(const WrapperTypeInfo* untrusted, v8::Local<v8::Value>);
  v8::Local<v8::Object> FindInstanceInPrototypeChain(const WrapperTypeInfo*,
                                                     v8::Local<v8::Value>);

  v8::Local<v8::Context> EnsureScriptRegexpContext();
  void ClearScriptRegexpContext();

  // EndOfScopeTasks are run when control is returning
  // to C++ from script, after executing a script task (e.g. callback,
  // event) or microtasks (e.g. promise). This is explicitly needed for
  // Indexed DB transactions per spec, but should in general be avoided.
  void AddEndOfScopeTask(base::OnceClosure);
  void RunEndOfScopeTasks();
  void ClearEndOfScopeTasks();

  void SetThreadDebugger(std::unique_ptr<Data>);
  Data* ThreadDebugger();

  void SetProfilerGroup(V8PerIsolateData::GarbageCollectedData*);
  V8PerIsolateData::GarbageCollectedData* ProfilerGroup();

  using ActiveScriptWrappableSet =
      HeapHashSet<WeakMember<ActiveScriptWrappableBase>>;
  void AddActiveScriptWrappable(ActiveScriptWrappableBase*);
  const ActiveScriptWrappableSet* ActiveScriptWrappables() const {
    return active_script_wrappables_.Get();
  }

 private:
  V8PerIsolateData(scoped_refptr<base::SingleThreadTaskRunner>,
                   V8ContextSnapshotMode);
  V8PerIsolateData();
  ~V8PerIsolateData();

  // A really simple hash function, which makes lookups faster. The set of
  // possible keys for this is relatively small and fixed at compile time, so
  // collisions are less of a worry than they would otherwise be.
  struct SimplePtrHash : WTF::PtrHash<const void> {
    static unsigned GetHash(const void* key) {
      uintptr_t k = reinterpret_cast<uintptr_t>(key);
      return static_cast<unsigned>(k ^ (k >> 8));
    }
  };
  using V8FunctionTemplateMap =
      HashMap<const void*, v8::Eternal<v8::FunctionTemplate>, SimplePtrHash>;
  V8FunctionTemplateMap& SelectInterfaceTemplateMap(const DOMWrapperWorld&);
  V8FunctionTemplateMap& SelectOperationTemplateMap(const DOMWrapperWorld&);
  bool HasInstance(const WrapperTypeInfo* untrusted,
                   v8::Local<v8::Value>,
                   V8FunctionTemplateMap&);
  v8::Local<v8::Object> FindInstanceInPrototypeChain(const WrapperTypeInfo*,
                                                     v8::Local<v8::Value>,
                                                     V8FunctionTemplateMap&);

  V8ContextSnapshotMode v8_context_snapshot_mode_;
  // This isolate_holder_ must be initialized before initializing some other
  // members below.
  gin::IsolateHolder isolate_holder_;

  // interface_template_map_for_{,non_}main_world holds function templates for
  // the inerface objects.
  V8FunctionTemplateMap interface_template_map_for_main_world_;
  V8FunctionTemplateMap interface_template_map_for_non_main_world_;

  // m_operationTemplateMapFor{,Non}MainWorld holds function templates for
  // the cross-origin accessible DOM operations.
  V8FunctionTemplateMap operation_template_map_for_main_world_;
  V8FunctionTemplateMap operation_template_map_for_non_main_world_;

  // Contains lists of eternal names, such as dictionary keys.
  HashMap<const void*, Vector<v8::Eternal<v8::Name>>> eternal_name_cache_;

  // When taking a V8 context snapshot, we can't keep V8 objects with eternal
  // handles. So we use a special interface map that doesn't use eternal handles
  // instead of the default V8FunctionTemplateMap.
  V8GlobalValueMap<const WrapperTypeInfo*, v8::FunctionTemplate>
      interface_template_map_for_v8_context_snapshot_;

  std::unique_ptr<StringCache> string_cache_;
  std::unique_ptr<V8PrivateProperty> private_property_;
  Persistent<ScriptState> script_regexp_script_state_;

  bool constructor_mode_;
  friend class ConstructorMode;

  bool use_counter_disabled_;
  friend class UseCounterDisabledScope;

  bool is_handling_recursion_level_error_;
  bool is_reporting_exception_;

  Vector<base::OnceClosure> end_of_scope_tasks_;
  std::unique_ptr<Data> thread_debugger_;
  Persistent<GarbageCollectedData> profiler_group_;

  Persistent<ActiveScriptWrappableSet> active_script_wrappables_;

  RuntimeCallStats runtime_call_stats_;

  DISALLOW_COPY_AND_ASSIGN(V8PerIsolateData);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_PER_ISOLATE_DATA_H_
