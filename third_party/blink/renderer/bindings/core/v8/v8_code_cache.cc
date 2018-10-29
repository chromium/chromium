// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"

#include "base/optional.h"
#include "build/build_config.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_initializer.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

namespace {

enum CacheTagKind { kCacheTagCode = 0, kCacheTagTimeStamp = 1, kCacheTagLast };

static const int kCacheTagKindSize = 1;

static_assert((1 << kCacheTagKindSize) >= kCacheTagLast,
              "CacheTagLast must be large enough");

uint32_t CacheTag(CacheTagKind kind, const String& encoding) {
  static uint32_t v8_cache_data_version =
      v8::ScriptCompiler::CachedDataVersionTag() << kCacheTagKindSize;

  // A script can be (successfully) interpreted with different encodings,
  // depending on the page it appears in. The cache doesn't know anything
  // about encodings, but the cached data is specific to one encoding. If we
  // later load the script from the cache and interpret it with a different
  // encoding, the cached data is not valid for that encoding.
  return (v8_cache_data_version | kind) +
         (encoding.IsNull() ? 0 : StringHash::GetHash(encoding));
}

// Check previously stored timestamp.
bool IsResourceHotForCaching(SingleCachedMetadataHandler* cache_handler,
                             int hot_hours) {
  const double hot_seconds = hot_hours * 60 * 60;
  scoped_refptr<CachedMetadata> cached_metadata =
      cache_handler->GetCachedMetadata(
          V8CodeCache::TagForTimeStamp(cache_handler));
  if (!cached_metadata)
    return false;
  double time_stamp;
  const int size = sizeof(time_stamp);
  DCHECK_EQ(cached_metadata->size(), static_cast<unsigned long>(size));
  memcpy(&time_stamp, cached_metadata->Data(), size);
  return (WTF::CurrentTime() - time_stamp) < hot_seconds;
}

}  // namespace

bool V8CodeCache::HasCodeCache(SingleCachedMetadataHandler* cache_handler) {
  if (!cache_handler)
    return false;

  uint32_t code_cache_tag = V8CodeCache::TagForCodeCache(cache_handler);
  return cache_handler->GetCachedMetadata(code_cache_tag).get();
}

v8::ScriptCompiler::CachedData* V8CodeCache::CreateCachedData(
    SingleCachedMetadataHandler* cache_handler) {
  DCHECK(cache_handler);
  uint32_t code_cache_tag = V8CodeCache::TagForCodeCache(cache_handler);
  scoped_refptr<CachedMetadata> cached_metadata =
      cache_handler->GetCachedMetadata(code_cache_tag);
  DCHECK(cached_metadata);
  const char* data = cached_metadata->Data();
  int length = cached_metadata->size();
  return new v8::ScriptCompiler::CachedData(
      reinterpret_cast<const uint8_t*>(data), length,
      v8::ScriptCompiler::CachedData::BufferNotOwned);
}

std::tuple<v8::ScriptCompiler::CompileOptions,
           V8CodeCache::ProduceCacheOptions,
           v8::ScriptCompiler::NoCacheReason>
V8CodeCache::GetCompileOptions(V8CacheOptions cache_options,
                               const ScriptSourceCode& source) {
  static const int kMinimalCodeLength = 1024;
  static const int kHotHours = 72;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;

  switch (source.SourceLocationType()) {
    case ScriptSourceLocationType::kInline:
      no_cache_reason = v8::ScriptCompiler::kNoCacheBecauseInlineScript;
      break;
    case ScriptSourceLocationType::kInlineInsideDocumentWrite:
      no_cache_reason = v8::ScriptCompiler::kNoCacheBecauseInDocumentWrite;
      break;
    case ScriptSourceLocationType::kExternalFile:
      no_cache_reason =
          v8::ScriptCompiler::kNoCacheBecauseResourceWithNoCacheHandler;
      break;
    // TODO(leszeks): Possibly differentiate between the other kinds of script
    // origin also.
    default:
      no_cache_reason = v8::ScriptCompiler::kNoCacheBecauseNoResource;
      break;
  }

  SingleCachedMetadataHandler* cache_handler = source.CacheHandler();
  if (!cache_handler) {
    return std::make_tuple(v8::ScriptCompiler::kNoCompileOptions,
                           ProduceCacheOptions::kNoProduceCache,
                           no_cache_reason);
  }

  if (cache_options == kV8CacheOptionsNone) {
    no_cache_reason = v8::ScriptCompiler::kNoCacheBecauseCachingDisabled;
    return std::make_tuple(v8::ScriptCompiler::kNoCompileOptions,
                           ProduceCacheOptions::kNoProduceCache,
                           no_cache_reason);
  }

  if (source.Source().length() < kMinimalCodeLength) {
    no_cache_reason = v8::ScriptCompiler::kNoCacheBecauseScriptTooSmall;
    return std::make_tuple(v8::ScriptCompiler::kNoCompileOptions,
                           ProduceCacheOptions::kNoProduceCache,
                           no_cache_reason);
  }

  if (HasCodeCache(cache_handler)) {
    return std::make_tuple(v8::ScriptCompiler::kConsumeCodeCache,
                           ProduceCacheOptions::kNoProduceCache,
                           no_cache_reason);
  }

  // If the resource is served from CacheStorage, generate the V8 code cache in
  // the first load.
  if (cache_handler->IsServedFromCacheStorage())
    cache_options = kV8CacheOptionsCodeWithoutHeatCheck;

  switch (cache_options) {
    case kV8CacheOptionsDefault:
    case kV8CacheOptionsCode:
      if (!IsResourceHotForCaching(cache_handler, kHotHours)) {
        return std::make_tuple(v8::ScriptCompiler::kNoCompileOptions,
                               ProduceCacheOptions::kSetTimeStamp,
                               v8::ScriptCompiler::kNoCacheBecauseCacheTooCold);
      }
      return std::make_tuple(
          v8::ScriptCompiler::kNoCompileOptions,
          ProduceCacheOptions::kProduceCodeCache,
          v8::ScriptCompiler::kNoCacheBecauseDeferredProduceCodeCache);
    case kV8CacheOptionsCodeWithoutHeatCheck:
      return std::make_tuple(
          v8::ScriptCompiler::kNoCompileOptions,
          ProduceCacheOptions::kProduceCodeCache,
          v8::ScriptCompiler::kNoCacheBecauseDeferredProduceCodeCache);
    case kV8CacheOptionsFullCodeWithoutHeatCheck:
      return std::make_tuple(
          v8::ScriptCompiler::kEagerCompile,
          ProduceCacheOptions::kProduceCodeCache,
          v8::ScriptCompiler::kNoCacheBecauseDeferredProduceCodeCache);
    case kV8CacheOptionsNone:
      // Shouldn't happen, as this is handled above.
      // Case is here so that compiler can check all cases are handled.
      NOTREACHED();
      break;
  }

  // All switch branches should return and we should never get here.
  // But some compilers aren't sure, hence this default.
  NOTREACHED();
  return std::make_tuple(v8::ScriptCompiler::kNoCompileOptions,
                         ProduceCacheOptions::kNoProduceCache,
                         v8::ScriptCompiler::kNoCacheNoReason);
}

void V8CodeCache::ProduceCache(
    v8::Isolate* isolate,
    v8::Local<v8::Script> script,
    const ScriptSourceCode& source,
    ProduceCacheOptions produce_cache_options,
    v8::ScriptCompiler::CompileOptions compile_options) {
  TRACE_EVENT0("v8", "v8.compile");
  RuntimeCallStatsScopedTracer rcs_scoped_tracer(isolate);
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kV8);

  switch (produce_cache_options) {
    case ProduceCacheOptions::kSetTimeStamp:
      V8CodeCache::SetCacheTimeStamp(source.CacheHandler());
      break;
    case ProduceCacheOptions::kProduceCodeCache: {
      constexpr const char* kTraceEventCategoryGroup = "v8,devtools.timeline";
      TRACE_EVENT_BEGIN1(kTraceEventCategoryGroup, "v8.compile", "fileName",
                         source.Url().GetString().Utf8());

      std::unique_ptr<v8::ScriptCompiler::CachedData> cached_data(
          v8::ScriptCompiler::CreateCodeCache(script->GetUnboundScript()));
      if (cached_data) {
        const char* data = reinterpret_cast<const char*>(cached_data->data);
        int length = cached_data->length;
        if (length > 1024) {
          // Omit histogram samples for small cache data to avoid outliers.
          int cache_size_ratio =
              static_cast<int>(100.0 * length / source.Source().length());
          DEFINE_THREAD_SAFE_STATIC_LOCAL(
              CustomCountHistogram, code_cache_size_histogram,
              ("V8.CodeCacheSizeRatio", 0, 10000, 50));
          code_cache_size_histogram.Count(cache_size_ratio);
        }
        SingleCachedMetadataHandler* cache_handler = source.CacheHandler();
        cache_handler->ClearCachedMetadata(
            CachedMetadataHandler::kCacheLocally);
        cache_handler->SetCachedMetadata(
            V8CodeCache::TagForCodeCache(cache_handler), data, length,
            CachedMetadataHandler::kSendToPlatform);
      }

      TRACE_EVENT_END1(
          kTraceEventCategoryGroup, "v8.compile", "data",
          InspectorCompileScriptEvent::Data(
              source.Url().GetString(), source.StartPosition(),
              InspectorCompileScriptEvent::V8CacheResult(
                  InspectorCompileScriptEvent::V8CacheResult::ProduceResult(
                      compile_options, cached_data ? cached_data->length : 0),
                  base::Optional<InspectorCompileScriptEvent::V8CacheResult::
                                     ConsumeResult>()),
              source.Streamer(), source.NotStreamingReason()));
      break;
    }
    case ProduceCacheOptions::kNoProduceCache:
      break;
  }
}

uint32_t V8CodeCache::TagForCodeCache(
    SingleCachedMetadataHandler* cache_handler) {
  return CacheTag(kCacheTagCode, cache_handler->Encoding());
}

uint32_t V8CodeCache::TagForTimeStamp(
    SingleCachedMetadataHandler* cache_handler) {
  return CacheTag(kCacheTagTimeStamp, cache_handler->Encoding());
}

// Store a timestamp to the cache as hint.
void V8CodeCache::SetCacheTimeStamp(
    SingleCachedMetadataHandler* cache_handler) {
  double now = WTF::CurrentTime();
  cache_handler->ClearCachedMetadata(CachedMetadataHandler::kCacheLocally);
  cache_handler->SetCachedMetadata(TagForTimeStamp(cache_handler),
                                   reinterpret_cast<char*>(&now), sizeof(now),
                                   CachedMetadataHandler::kSendToPlatform);
}

// static
scoped_refptr<CachedMetadata> V8CodeCache::GenerateFullCodeCache(
    ScriptState* script_state,
    const String& script_string,
    const String& file_name,
    const WTF::TextEncoding& encoding,
    OpaqueMode opaque_mode) {
  constexpr const char* kTraceEventCategoryGroup = "v8,devtools.timeline";
  TRACE_EVENT_BEGIN1(kTraceEventCategoryGroup, "v8.compile", "fileName",
                     file_name.Utf8());

  ScriptState::Scope scope(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();
  // v8::TryCatch is needed to suppress all exceptions thrown during the code
  // cache generation.
  v8::TryCatch block(isolate);
  ReferrerScriptInfo referrer_info;
  v8::ScriptOrigin origin(
      V8String(isolate, file_name),
      v8::Integer::New(isolate, 0),  // line_offset
      v8::Integer::New(isolate, 0),  // column_offset
      v8::Boolean::New(
          isolate,
          opaque_mode == OpaqueMode::kNotOpaque),  // is_shared_cross_origin
      v8::Local<v8::Integer>(),                    // script_id
      V8String(isolate, String("")),               // source_map_url
      v8::Boolean::New(isolate,
                       opaque_mode == OpaqueMode::kOpaque),  // is_opaque
      v8::False(isolate),                                    // is_wasm
      v8::False(isolate),                                    // is_module
      referrer_info.ToV8HostDefinedOptions(isolate));
  v8::Local<v8::String> code(V8String(isolate, script_string));
  v8::ScriptCompiler::Source source(code, origin);
  scoped_refptr<CachedMetadata> cached_metadata;
  std::unique_ptr<v8::ScriptCompiler::CachedData> cached_data;

  v8::Local<v8::UnboundScript> unbound_script;
  // When failed to compile the script with syntax error, the exceptions is
  // suppressed by the v8::TryCatch, and returns null.
  if (v8::ScriptCompiler::CompileUnboundScript(
          isolate, &source, v8::ScriptCompiler::kEagerCompile)
          .ToLocal(&unbound_script)) {
    cached_data.reset(v8::ScriptCompiler::CreateCodeCache(unbound_script));
    if (cached_data && cached_data->length) {
      cached_metadata = CachedMetadata::Create(
          CacheTag(kCacheTagCode, encoding.GetName()),
          reinterpret_cast<const char*>(cached_data->data),
          cached_data->length);
    }
  }

  TRACE_EVENT_END1(
      kTraceEventCategoryGroup, "v8.compile", "data",
      InspectorCompileScriptEvent::Data(
          file_name, TextPosition(),
          InspectorCompileScriptEvent::V8CacheResult(
              InspectorCompileScriptEvent::V8CacheResult::ProduceResult(
                  v8::ScriptCompiler::kEagerCompile,
                  cached_data ? cached_data->length : 0),
              base::Optional<
                  InspectorCompileScriptEvent::V8CacheResult::ConsumeResult>()),
          false, ScriptStreamer::kHasCodeCache));

  return cached_metadata;
}

STATIC_ASSERT_ENUM(WebSettings::V8CacheOptions::kDefault,
                   kV8CacheOptionsDefault);
STATIC_ASSERT_ENUM(WebSettings::V8CacheOptions::kNone, kV8CacheOptionsNone);
STATIC_ASSERT_ENUM(WebSettings::V8CacheOptions::kCode, kV8CacheOptionsCode);
STATIC_ASSERT_ENUM(WebSettings::V8CacheOptions::kCodeWithoutHeatCheck,
                   kV8CacheOptionsCodeWithoutHeatCheck);
STATIC_ASSERT_ENUM(WebSettings::V8CacheOptions::kFullCodeWithoutHeatCheck,
                   kV8CacheOptionsFullCodeWithoutHeatCheck);

}  // namespace blink
