// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_MODULE_RECORD_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_MODULE_RECORD_H_

#include "third_party/blink/renderer/bindings/core/v8/script_source_location_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class KURL;
class ScriptFetchOptions;
class ScriptState;
class ScriptValue;
class ScriptPromise;

// ModuleEvaluationResult encapsulates the result of a module evaluation.
// - Without top-level-await
//   - succeed and not return a value, or
//     (IsSuccess() == true), no return value is available.
//   - throw any object.
//     (IsException() == true && GetException()) returns the thrown exception
// - With top-level-await a module can either
//   - return a promise, or
//     (IsSuccess() == true && GetPromise()) returns a valid ScriptPromise())
//   - throw any object.
//     (IsException() == true && GetException()) returns the thrown exception
class CORE_EXPORT ModuleEvaluationResult final {
  STACK_ALLOCATED();

 public:
  ModuleEvaluationResult() = delete;
  static ModuleEvaluationResult Empty();
  static ModuleEvaluationResult FromResult(v8::Local<v8::Value> promise);
  static ModuleEvaluationResult FromException(v8::Local<v8::Value> exception);

  ModuleEvaluationResult(const ModuleEvaluationResult& value) = default;
  ModuleEvaluationResult& operator=(const ModuleEvaluationResult& value) =
      default;
  ~ModuleEvaluationResult() = default;

  ModuleEvaluationResult& Escape(ScriptState::EscapableScope* scope);

  bool IsSuccess() const { return is_success_; }
  bool IsException() const { return !is_success_; }

  v8::Local<v8::Value> GetException() const;
  ScriptPromise GetPromise(ScriptState* script_state) const;

 private:
  ModuleEvaluationResult(bool is_success, v8::Local<v8::Value> value)
      : is_success_(is_success), value_(value) {}

  bool is_success_;
  v8::Local<v8::Value> value_;
};

// ModuleRecordProduceCacheData is a parameter object for
// ModuleRecord::ProduceCache().
class CORE_EXPORT ModuleRecordProduceCacheData final
    : public GarbageCollected<ModuleRecordProduceCacheData> {
 public:
  ModuleRecordProduceCacheData(v8::Isolate*,
                               SingleCachedMetadataHandler*,
                               V8CodeCache::ProduceCacheOptions,
                               v8::Local<v8::Module>);

  void Trace(Visitor*) const;

  SingleCachedMetadataHandler* CacheHandler() const { return cache_handler_; }
  V8CodeCache::ProduceCacheOptions GetProduceCacheOptions() const {
    return produce_cache_options_;
  }
  v8::Local<v8::UnboundModuleScript> UnboundScript(v8::Isolate* isolate) const {
    return unbound_script_.NewLocal(isolate);
  }

 private:
  Member<SingleCachedMetadataHandler> cache_handler_;
  V8CodeCache::ProduceCacheOptions produce_cache_options_;

  // TODO(keishi): Visitor only defines a trace method for v8::Value so this
  // needs to be cast.
  GC_PLUGIN_IGNORE("757708")
  TraceWrapperV8Reference<v8::UnboundModuleScript> unbound_script_;
};

// TODO(rikaf): Add a class level comment
class CORE_EXPORT ModuleRecord final {
  STATIC_ONLY(ModuleRecord);

 public:
  static v8::Local<v8::Module> Compile(
      v8::Isolate*,
      const String& source,
      const KURL& source_url,
      const KURL& base_url,
      const ScriptFetchOptions&,
      const TextPosition&,
      ExceptionState&,
      V8CacheOptions = kV8CacheOptionsDefault,
      SingleCachedMetadataHandler* = nullptr,
      ScriptSourceLocationType source_location_type =
          ScriptSourceLocationType::kInternal,
      ModuleRecordProduceCacheData** out_produce_cache_data = nullptr);

  // Returns exception, if any.
  static ScriptValue Instantiate(ScriptState*,
                                 v8::Local<v8::Module> record,
                                 const KURL& source_url);

  static ModuleEvaluationResult Evaluate(ScriptState*,
                                         v8::Local<v8::Module> record,
                                         const KURL& source_url);

  static void ReportException(ScriptState*, v8::Local<v8::Value> exception);

  static Vector<String> ModuleRequests(ScriptState*,
                                       v8::Local<v8::Module> record);
  static Vector<TextPosition> ModuleRequestPositions(
      ScriptState*,
      v8::Local<v8::Module> record);

  static v8::Local<v8::Value> V8Namespace(v8::Local<v8::Module> record);

 private:
  static v8::MaybeLocal<v8::Module> ResolveModuleCallback(
      v8::Local<v8::Context>,
      v8::Local<v8::String> specifier,
      v8::Local<v8::Module> referrer);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_MODULE_RECORD_H_
