// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"

#include <optional>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "components/miracle_parameter/common/public/miracle_parameter.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom-blink.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/bindings/core/v8/module_record.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_initializer.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {

namespace {

BASE_FEATURE(kConfigurableV8CodeCacheHotHours,
             "ConfigurableV8CodeCacheHotHours",
             base::FEATURE_DISABLED_BY_DEFAULT);

MIRACLE_PARAMETER_FOR_INT(GetV8CodeCacheHotHours,
                          kConfigurableV8CodeCacheHotHours,
                          "HotHours",
                          72)

enum CacheTagKind {
  kCacheTagCode = 0,
  kCacheTagTimeStamp = 1,
  kCacheTagCompileHints = 2,
  kCacheTagLast
};

static const int kCacheTagKindSize = 2;

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
         (encoding.IsNull() ? 0 : WTF::GetHash(encoding));
}

bool TimestampIsRecent(const CachedMetadata* cached_metadata) {
  const base::TimeDelta kHotHours = base::Hours(GetV8CodeCacheHotHours());
  uint64_t time_stamp_ms;
  const uint32_t size = sizeof(time_stamp_ms);
  CHECK_GE(cached_metadata->size(), size);
  memcpy(&time_stamp_ms, cached_metadata->Data(), size);
  base::TimeTicks time_stamp =
      base::TimeTicks() + base::Milliseconds(time_stamp_ms);
  return (base::TimeTicks::Now() - time_stamp) < kHotHours;
}

// Flags that can be set in the CacheMetadata header, describing how the code
// cache data was produced so that the consumer can generate better trace
// messages.
enum class DetailFlags : uint64_t {
  kNone = 0,
  kFull = 1,
};

V8CodeCache::GetMetadataType ReadGetMetadataType(
    const CachedMetadataHandler* cache_handler) {
  // Check the metadata types in the same preference order they're checked in
  // the code: code cache, local compile hints, timestamp. That way we get the
  // right sample in case several metadata types are set.
  uint32_t code_cache_tag = V8CodeCache::TagForCodeCache(cache_handler);
  if (cache_handler
          ->GetCachedMetadata(code_cache_tag,
                              CachedMetadataHandler::kAllowUnchecked)
          .get()) {
    return V8CodeCache::GetMetadataType::kCodeCache;
  }
  scoped_refptr<CachedMetadata> cached_metadata =
      cache_handler->GetCachedMetadata(
          V8CodeCache::TagForCompileHints(cache_handler),
          CachedMetadataHandler::kAllowUnchecked);
  if (cached_metadata) {
    return TimestampIsRecent(cached_metadata.get())
               ? V8CodeCache::GetMetadataType::
                     kLocalCompileHintsWithHotTimestamp
               : V8CodeCache::GetMetadataType::
                     kLocalCompileHintsWithColdTimestamp;
  }
  cached_metadata = cache_handler->GetCachedMetadata(
      V8CodeCache::TagForTimeStamp(cache_handler),
      CachedMetadataHandler::kAllowUnchecked);
  if (cached_metadata) {
    return TimestampIsRecent(cached_metadata.get())
               ? V8CodeCache::GetMetadataType::kHotTimestamp
               : V8CodeCache::GetMetadataType::kColdTimestamp;
  }
  return V8CodeCache::GetMetadataType::kNone;
}

V8CodeCache::GetMetadataType ReadGetMetadataType(
    const CachedMetadata* cached_metadata,
    const String& encoding) {
  if (!cached_metadata) {
    return V8CodeCache::GetMetadataType::kNone;
  }

  // Check the metadata types in the same preference order they're checked in
  // the code: code cache, local compile hints, timestamp. That way we get the
  // right sample in case several metadata types are set.
  if (cached_metadata->DataTypeID() == CacheTag(kCacheTagCode, encoding)) {
    return V8CodeCache::GetMetadataType::kCodeCache;
  }

  if (cached_metadata->DataTypeID() ==
      CacheTag(kCacheTagCompileHints, encoding)) {
    return TimestampIsRecent(cached_metadata)
               ? V8CodeCache::GetMetadataType::
                     kLocalCompileHintsWithHotTimestamp
               : V8CodeCache::GetMetadataType::
                     kLocalCompileHintsWithColdTimestamp;
  }

  if (cached_metadata->DataTypeID() == CacheTag(kCacheTagTimeStamp, encoding)) {
    return TimestampIsRecent(cached_metadata)
               ? V8CodeCache::GetMetadataType::kHotTimestamp
               : V8CodeCache::GetMetadataType::kColdTimestamp;
  }
  return V8CodeCache::GetMetadataType::kNone;
}

constexpr const char* kCacheGetHistogram =
    "WebCore.Scripts.V8CodeCacheMetadata.Get";
constexpr const char* kCacheSetHistogram =
    "WebCore.Scripts.V8CodeCacheMetadata.Set";

}  // namespace

// Check previously stored timestamp (either from the code cache or compile
// hints cache).
bool V8CodeCache::HasHotTimestamp(const CachedMetadataHandler* cache_handler) {
  if (!cache_handler) {
    return false;
  }
  scoped_refptr<CachedMetadata> cached_metadata =
      cache_handler->GetCachedMetadata(
          V8CodeCache::TagForTimeStamp(cache_handler),
          CachedMetadataHandler::kAllowUnchecked);
  if (cached_metadata) {
    return TimestampIsRecent(cached_metadata.get());
  }
  cached_metadata = cache_handler->GetCachedMetadata(
      V8CodeCache::TagForCompileHints(cache_handler),
      CachedMetadataHandler::kAllowUnchecked);
  if (cached_metadata) {
    return TimestampIsRecent(cached_metadata.get());
  }
  return false;
}

bool V8CodeCache::HasHotTimestamp(const CachedMetadata& data,
                                  const String& encoding) {
  if (data.DataTypeID() != CacheTag(kCacheTagCompileHints, encoding) &&
      data.DataTypeID() != CacheTag(kCacheTagTimeStamp, encoding)) {
    return false;
  }
  return TimestampIsRecent(&data);
}

bool V8CodeCache::HasCodeCache(
    const CachedMetadataHandler* cache_handler,
    CachedMetadataHandler::GetCachedMetadataBehavior behavior) {
  if (!cache_handler)
    return false;

  uint32_t code_cache_tag = V8CodeCache::TagForCodeCache(cache_handler);
  return cache_handler->GetCachedMetadata(code_cache_tag, behavior).get();
}

bool V8CodeCache::HasCodeCache(const CachedMetadata& data,
                               const String& encoding) {
  return data.DataTypeID() == CacheTag(kCacheTagCode, encoding);
}

bool V8CodeCache::HasCompileHints(
    const CachedMetadataHandler* cache_handler,
    CachedMetadataHandler::GetCachedMetadataBehavior behavior) {
  if (!cache_handler) {
    return false;
  }

  uint32_t code_cache_tag = V8CodeCache::TagForCompileHints(cache_handler);
  scoped_refptr<CachedMetadata> cached_metadata =
      cache_handler->GetCachedMetadata(code_cache_tag, behavior);
  if (!cached_metadata) {
    return false;
  }
  return true;
}

bool V8CodeCache::HasHotCompileHints(const CachedMetadata& data,
                                     const String& encoding) {
  if (data.DataTypeID() != CacheTag(kCacheTagCompileHints, encoding)) {
    return false;
  }
  return TimestampIsRecent(&data);
}

std::unique_ptr<v8::ScriptCompiler::CachedData> V8CodeCache::CreateCachedData(
    const CachedMetadataHandler* cache_handler) {
  return V8CodeCache::CreateCachedData(GetCachedMetadata(cache_handler));
}

std::unique_ptr<v8::ScriptCompiler::CachedData> V8CodeCache::CreateCachedData(
    scoped_refptr<CachedMetadata> cached_metadata) {
  DCHECK(cached_metadata);
  const uint8_t* data = cached_metadata->Data();
  int length = cached_metadata->size();
  return std::make_unique<v8::ScriptCompiler::CachedData>(
      data, length, v8::ScriptCompiler::CachedData::BufferNotOwned);
}

scoped_refptr<CachedMetadata> V8CodeCache::GetCachedMetadata(
    const CachedMetadataHandler* cache_handler,
    CachedMetadataHandler::GetCachedMetadataBehavior behavior) {
  DCHECK(cache_handler);
  uint32_t code_cache_tag = V8CodeCache::TagForCodeCache(cache_handler);
  scoped_refptr<CachedMetadata> cached_metadata =
      cache_handler->GetCachedMetadata(code_cache_tag, behavior);
  DCHECK(cached_metadata);
  return cached_metadata;
}

scoped_refptr<CachedMetadata> V8CodeCache::GetCachedMetadataForCompileHints(
    const CachedMetadataHandler* cache_handler,
    CachedMetadataHandler::GetCachedMetadataBehavior behavior) {
  CHECK(cache_handler);
  uint32_t code_cache_tag = V8CodeCache::TagForCompileHints(cache_handler);
  scoped_refptr<CachedMetadata> cached_metadata =
      cache_handler->GetCachedMetadata(code_cache_tag, behavior);
  CHECK(cached_metadata);
  return cached_metadata;
}

std::tuple<v8::ScriptCompiler::CompileOptions,
           V8CodeCache::ProduceCacheOptions,
           v8::ScriptCompiler::NoCacheReason>
V8CodeCache::GetCompileOptions(
    mojom::blink::V8CacheOptions cache_options,
    const ClassicScript& classic_script,
    bool might_generate_crowdsourced_compile_hints,
    bool can_use_crowdsourced_compile_hints,
    bool v8_compile_hints_magic_comment_runtime_enabled) {
  return GetCompileOptions(
      cache_options, classic_script.CacheHandler(),
      classic_script.SourceText().length(), classic_script.SourceLocationType(),
      classic_script.SourceUrl(), might_generate_crowdsourced_compile_hints,
      can_use_crowdsourced_compile_hints,
      v8_compile_hints_magic_comment_runtime_enabled);
}

namespace {
v8::ScriptCompiler::CompileOptions MaybeAddCompileHintsMagic(
    v8::ScriptCompiler::CompileOptions compile_options,
    bool v8_compile_hints_magic_comment_runtime_enabled) {
  if (v8_compile_hints_magic_comment_runtime_enabled) {
    return v8::ScriptCompiler::CompileOptions(
        compile_options | v8::ScriptCompiler::kFollowCompileHintsMagicComment);
  }
  return compile_options;
}

}  // namespace

std::tuple<v8::ScriptCompiler::CompileOptions,
           V8CodeCache::ProduceCacheOptions,
           v8::ScriptCompiler::NoCacheReason>
V8CodeCache::GetCompileOptions(
    mojom::blink::V8CacheOptions cache_options,
    const CachedMetadataHandler* cache_handler,
    size_t source_text_length,
    ScriptSourceLocationType source_location_type,
    const KURL& url,
    bool might_generate_crowdsourced_compile_hints,
    bool can_use_crowdsourced_compile_hints,
    bool v8_compile_hints_magic_comment_runtime_enabled) {
  static const int kMinimalCodeLength = 1024;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;

  auto no_code_cache_compile_options = v8::ScriptCompiler::kNoCompileOptions;

  if (might_generate_crowdsourced_compile_hints) {
    DCHECK(base::FeatureList::IsEnabled(features::kProduceCompileHints2));

    // If we end up compiling the script without forced eager compilation, we'll
    // also produce compile hints. This is orthogonal to producing the code
    // cache: if we don't want to create a code cache for some reason
    // (e.g., script too small, or not hot enough) we still want to produce
    // compile hints.

    // When we're forcing eager compilation, we cannot produce compile hints
    // (we won't gather data about which eagerly compiled functions are
    // actually used).

    // We also disable reading the script from the code cache when producing
    // compile hints. This is because we cannot generate compile hints for
    // cached scripts (especially if they've been eagerly compiled by a
    // ServiceWorker) and omitting cached scripts would deteriorate the data.
    no_code_cache_compile_options = v8::ScriptCompiler::kProduceCompileHints;
  } else if (can_use_crowdsourced_compile_hints) {
    // This doesn't need to be gated behind a runtime flag, because there won't
    // be any data unless the v8_compile_hints::kConsumeCompileHints
    // flag is on.
    no_code_cache_compile_options = v8::ScriptCompiler::kConsumeCompileHints;
  }

  switch (source_location_type) {
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

  if (!cache_handler) {
    return std::make_tuple(no_code_cache_compile_options,
                           ProduceCacheOptions::kNoProduceCache,
                           no_cache_reason);
  }

  if (cache_options == mojom::blink::V8CacheOptions::kNone) {
    no_cache_reason = v8::ScriptCompiler::kNoCacheBecauseCachingDisabled;
    return std::make_tuple(no_code_cache_compile_options,
                           ProduceCacheOptions::kNoProduceCache,
                           no_cache_reason);
  }

  if (source_text_length < kMinimalCodeLength) {
    no_cache_reason = v8::ScriptCompiler::kNoCacheBecauseScriptTooSmall;
    return std::make_tuple(no_code_cache_compile_options,
                           ProduceCacheOptions::kNoProduceCache,
                           no_cache_reason);
  }

  // By recording statistics at this point we exclude scripts for which we're
  // not going to generate metadata.
  RecordCacheGetStatistics(cache_handler);

  if (HasCodeCache(cache_handler) &&
      (no_code_cache_compile_options &
       v8::ScriptCompiler::kProduceCompileHints) == 0) {
    return std::make_tuple(v8::ScriptCompiler::kConsumeCodeCache,
                           ProduceCacheOptions::kNoProduceCache,
                           no_cache_reason);
  }

  // If the resource is served from CacheStorage, generate the V8 code cache in
  // the first load.
  if (cache_handler->IsServedFromCacheStorage())
    cache_options = mojom::blink::V8CacheOptions::kCodeWithoutHeatCheck;

  bool local_compile_hints_enabled =
      base::FeatureList::IsEnabled(features::kLocalCompileHints) &&
      !might_generate_crowdsourced_compile_hints &&
      !can_use_crowdsourced_compile_hints;

  switch (cache_options) {
    case mojom::blink::V8CacheOptions::kDefault:
    case mojom::blink::V8CacheOptions::kCode: {
      if (!HasHotTimestamp(cache_handler)) {
        if (local_compile_hints_enabled) {
          // If the resource is not yet hot for caching, set the timestamp and
          // produce compile hints. Setting the time stamp first is important,
          // because compile hints are only produced later (when the page turns
          // interactive). If the user navigates away before that happens, we
          // don't want to end up with no cache at all, since the resource would
          // then appear to be cold during the next run.

          // TODO(1495723): This branch doesn't check HasCompileHints. It's not
          // clear what we should do if the resource is not hot but we have
          // compile hints. 1) Consume compile hints and produce new ones
          // (currently not possible in the API) and combine both compile hints.
          // 2) Ignore existing compile hints (we're anyway not creating the
          // code cache yet) and produce new ones.
          return std::make_tuple(
              v8::ScriptCompiler::kProduceCompileHints,
              ProduceCacheOptions::kSetTimeStamp,
              v8::ScriptCompiler::kNoCacheBecauseCacheTooCold);
        }
        return std::make_tuple(
            MaybeAddCompileHintsMagic(
                no_code_cache_compile_options,
                v8_compile_hints_magic_comment_runtime_enabled),
            ProduceCacheOptions::kSetTimeStamp,
            v8::ScriptCompiler::kNoCacheBecauseCacheTooCold);
      }
      if (local_compile_hints_enabled && HasCompileHints(cache_handler)) {
        // In this branch, the timestamp in the compile hints is hot.
        return std::make_tuple(
            MaybeAddCompileHintsMagic(
                v8::ScriptCompiler::kConsumeCompileHints,
                v8_compile_hints_magic_comment_runtime_enabled),
            ProduceCacheOptions::kProduceCodeCache,
            v8::ScriptCompiler::kNoCacheBecauseDeferredProduceCodeCache);
      }
      return std::make_tuple(
          MaybeAddCompileHintsMagic(
              no_code_cache_compile_options,
              v8_compile_hints_magic_comment_runtime_enabled),
          ProduceCacheOptions::kProduceCodeCache,
          v8::ScriptCompiler::kNoCacheBecauseDeferredProduceCodeCache);
    }
    case mojom::blink::V8CacheOptions::kCodeWithoutHeatCheck:
      return std::make_tuple(
          MaybeAddCompileHintsMagic(
              no_code_cache_compile_options,
              v8_compile_hints_magic_comment_runtime_enabled),
          ProduceCacheOptions::kProduceCodeCache,
          v8::ScriptCompiler::kNoCacheBecauseDeferredProduceCodeCache);
    case mojom::blink::V8CacheOptions::kFullCodeWithoutHeatCheck:
      return std::make_tuple(
          v8::ScriptCompiler::kEagerCompile,
          ProduceCacheOptions::kProduceCodeCache,
          v8::ScriptCompiler::kNoCacheBecauseDeferredProduceCodeCache);
    case mojom::blink::V8CacheOptions::kNone:
      // Shouldn't happen, as this is handled above.
      // Case is here so that compiler can check all cases are handled.
      NOTREACHED_IN_MIGRATION();
      break;
  }

  // All switch branches should return and we should never get here.
  // But some compilers aren't sure, hence this default.
  NOTREACHED_IN_MIGRATION();
  return std::make_tuple(no_code_cache_compile_options,
                         ProduceCacheOptions::kNoProduceCache,
                         v8::ScriptCompiler::kNoCacheNoReason);
}

bool V8CodeCache::IsFull(const CachedMetadata* metadata) {
  const uint64_t full_flag = static_cast<uint64_t>(DetailFlags::kFull);
  return (metadata->tag() & full_flag) != 0;
}

template <typename UnboundScript>
static void ProduceCacheInternal(
    v8::Isolate* isolate,
    CodeCacheHost* code_cache_host,
    v8::Local<UnboundScript> unbound_script,
    CachedMetadataHandler* cache_handler,
    size_t source_text_length,
    const KURL& source_url,
    const TextPosition& source_start_position,
    const char* trace_name,
    V8CodeCache::ProduceCacheOptions produce_cache_options) {
  TRACE_EVENT0("v8", trace_name);
  RuntimeCallStatsScopedTracer rcs_scoped_tracer(isolate);
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kV8);

  switch (produce_cache_options) {
    case V8CodeCache::ProduceCacheOptions::kSetTimeStamp:
      V8CodeCache::SetCacheTimeStamp(code_cache_host, cache_handler);
      break;
    case V8CodeCache::ProduceCacheOptions::kProduceCodeCache: {
      // TODO(crbug.com/938269): Investigate why this can be empty here.
      if (unbound_script.IsEmpty())
        break;

      constexpr const char* kTraceEventCategoryGroup = "v8,devtools.timeline";
      TRACE_EVENT_BEGIN1(kTraceEventCategoryGroup, trace_name, "fileName",
                         source_url.GetString().Utf8());

      base::ElapsedTimer timer;
      std::unique_ptr<v8::ScriptCompiler::CachedData> cached_data(
          v8::ScriptCompiler::CreateCodeCache(unbound_script));
      if (cached_data) {
        V8CodeCache::RecordCacheSetStatistics(
            V8CodeCache::SetMetadataType::kCodeCache);
        const uint8_t* data = cached_data->data;
        int length = cached_data->length;
        cache_handler->ClearCachedMetadata(
            code_cache_host, CachedMetadataHandler::kClearLocally);
        cache_handler->SetCachedMetadata(
            code_cache_host, V8CodeCache::TagForCodeCache(cache_handler), data,
            length);
        base::UmaHistogramMicrosecondsTimes("V8.ProduceCodeCacheMicroseconds",
                                            timer.Elapsed());
      }

      TRACE_EVENT_END1(kTraceEventCategoryGroup, trace_name, "data",
                       [&](perfetto::TracedValue context) {
                         inspector_produce_script_cache_event::Data(
                             std::move(context), source_url.GetString(),
                             source_start_position,
                             cached_data ? cached_data->length : 0);
                       });
      break;
    }
    case V8CodeCache::ProduceCacheOptions::kNoProduceCache:
      break;
  }
}

void V8CodeCache::ProduceCache(v8::Isolate* isolate,
                               CodeCacheHost* code_cache_host,
                               v8::Local<v8::Script> script,
                               CachedMetadataHandler* cache_handler,
                               size_t source_text_length,
                               const KURL& source_url,
                               const TextPosition& source_start_position,
                               ProduceCacheOptions produce_cache_options) {
  ProduceCacheInternal(isolate, code_cache_host, script->GetUnboundScript(),
                       cache_handler, source_text_length, source_url,
                       source_start_position, "v8.produceCache",
                       produce_cache_options);
}

void V8CodeCache::ProduceCache(v8::Isolate* isolate,
                               CodeCacheHost* code_cache_host,
                               ModuleRecordProduceCacheData* produce_cache_data,
                               size_t source_text_length,
                               const KURL& source_url,
                               const TextPosition& source_start_position) {
  ProduceCacheInternal(
      isolate, code_cache_host, produce_cache_data->UnboundScript(isolate),
      produce_cache_data->CacheHandler(), source_text_length, source_url,
      source_start_position, "v8.produceModuleCache",
      produce_cache_data->GetProduceCacheOptions());
}

uint32_t V8CodeCache::TagForCodeCache(
    const CachedMetadataHandler* cache_handler) {
  return CacheTag(kCacheTagCode, cache_handler->Encoding());
}

uint32_t V8CodeCache::TagForTimeStamp(
    const CachedMetadataHandler* cache_handler) {
  return CacheTag(kCacheTagTimeStamp, cache_handler->Encoding());
}

uint32_t V8CodeCache::TagForCompileHints(
    const CachedMetadataHandler* cache_handler) {
  return CacheTag(kCacheTagCompileHints, cache_handler->Encoding());
}

// Store a timestamp to the cache as hint.
void V8CodeCache::SetCacheTimeStamp(CodeCacheHost* code_cache_host,
                                    CachedMetadataHandler* cache_handler) {
  RecordCacheSetStatistics(V8CodeCache::SetMetadataType::kTimestamp);
  uint64_t now_ms = GetTimestamp();
  cache_handler->ClearCachedMetadata(code_cache_host,
                                     CachedMetadataHandler::kClearLocally);
  cache_handler->SetCachedMetadata(
      code_cache_host, TagForTimeStamp(cache_handler),
      reinterpret_cast<uint8_t*>(&now_ms), sizeof(now_ms));
}

uint64_t V8CodeCache::GetTimestamp() {
  return base::TimeTicks::Now().since_origin().InMilliseconds();
}

// static
scoped_refptr<CachedMetadata> V8CodeCache::GenerateFullCodeCache(
    ScriptState* script_state,
    const String& script_string,
    const KURL& source_url,
    const WTF::TextEncoding& encoding,
    OpaqueMode opaque_mode) {
  const String file_name = source_url.GetString();

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
      0,                                      // line_offset
      0,                                      // column_offset
      opaque_mode == OpaqueMode::kNotOpaque,  // is_shared_cross_origin
      -1,                                     // script_id
      V8String(isolate, String("")),          // source_map_url
      opaque_mode == OpaqueMode::kOpaque,     // is_opaque
      false,                                  // is_wasm
      false,                                  // is_module
      referrer_info.ToV8HostDefinedOptions(isolate, source_url));
  v8::Local<v8::String> code(V8String(isolate, script_string));
  v8::ScriptCompiler::Source source(code, origin);
  scoped_refptr<CachedMetadata> cached_metadata;

  v8::MaybeLocal<v8::UnboundScript> maybe_unbound_script =
      v8::ScriptCompiler::CompileUnboundScript(
          isolate, &source, v8::ScriptCompiler::kEagerCompile);

  TRACE_EVENT_END1(
      kTraceEventCategoryGroup, "v8.compile", "data",
      [&](perfetto::TracedValue context) {
        inspector_compile_script_event::Data(
            std::move(context), file_name, TextPosition::MinimumPosition(),
            std::nullopt, true, false,
            ScriptStreamer::NotStreamingReason::kStreamingDisabled);
      });

  v8::Local<v8::UnboundScript> unbound_script;
  // When failed to compile the script with syntax error, the exceptions is
  // suppressed by the v8::TryCatch, and returns null.
  if (maybe_unbound_script.ToLocal(&unbound_script)) {
    TRACE_EVENT_BEGIN1(kTraceEventCategoryGroup, "v8.produceCache", "fileName",
                       file_name.Utf8());

    std::unique_ptr<v8::ScriptCompiler::CachedData> cached_data(
        v8::ScriptCompiler::CreateCodeCache(unbound_script));
    if (cached_data && cached_data->length) {
      cached_metadata = CachedMetadata::Create(
          CacheTag(kCacheTagCode, encoding.GetName()), cached_data->data,
          cached_data->length, static_cast<uint64_t>(DetailFlags::kFull));
    }

    TRACE_EVENT_END1(kTraceEventCategoryGroup, "v8.produceCache", "data",
                     [&](perfetto::TracedValue context) {
                       inspector_produce_script_cache_event::Data(
                           std::move(context), file_name,
                           TextPosition::MinimumPosition(),
                           cached_data ? cached_data->length : 0);
                     });
  }

  return cached_metadata;
}

void V8CodeCache::RecordCacheGetStatistics(
    const CachedMetadataHandler* cache_handler) {
  base::UmaHistogramEnumeration(kCacheGetHistogram,
                                ReadGetMetadataType(cache_handler));
}

void V8CodeCache::RecordCacheGetStatistics(
    const CachedMetadata* cached_metadata,
    const String& encoding) {
  base::UmaHistogramEnumeration(kCacheGetHistogram,
                                ReadGetMetadataType(cached_metadata, encoding));
}

void V8CodeCache::RecordCacheGetStatistics(
    V8CodeCache::GetMetadataType metadata_type) {
  base::UmaHistogramEnumeration(kCacheGetHistogram, metadata_type);
}

void V8CodeCache::RecordCacheSetStatistics(
    V8CodeCache::SetMetadataType metadata_type) {
  base::UmaHistogramEnumeration(kCacheSetHistogram, metadata_type);
}

}  // namespace blink
