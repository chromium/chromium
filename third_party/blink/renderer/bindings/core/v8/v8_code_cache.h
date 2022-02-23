// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_CODE_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_CODE_CACHE_H_

#include <stdint.h>

#include "third_party/blink/public/mojom/v8_cache_options.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_location_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace WTF {
class TextEncoding;
class TextPosition;
}  // namespace WTF

namespace blink {

class CachedMetadata;
class ClassicScript;
class KURL;
class ModuleRecordProduceCacheData;

namespace mojom {
class CodeCacheHost;
}

class CORE_EXPORT V8CodeCache final {
  STATIC_ONLY(V8CodeCache);

 public:
  enum class OpaqueMode {
    kOpaque,
    kNotOpaque,
  };

  enum class ProduceCacheOptions {
    kNoProduceCache,
    kSetTimeStamp,
    kProduceCodeCache,
  };

  static uint32_t TagForCodeCache(const SingleCachedMetadataHandler*);
  static uint32_t TagForTimeStamp(const SingleCachedMetadataHandler*);
  static void SetCacheTimeStamp(CodeCacheHost*, SingleCachedMetadataHandler*);

  // Returns true iff the SingleCachedMetadataHandler contains a code cache
  // that can be consumed by V8.
  static bool HasCodeCache(
      const SingleCachedMetadataHandler*,
      SingleCachedMetadataHandler::GetCachedMetadataBehavior behavior =
          SingleCachedMetadataHandler::kCrashIfUnchecked);

  static std::tuple<v8::ScriptCompiler::CompileOptions,
                    ProduceCacheOptions,
                    v8::ScriptCompiler::NoCacheReason>
  GetCompileOptions(mojom::blink::V8CacheOptions, const ClassicScript&);
  static std::tuple<v8::ScriptCompiler::CompileOptions,
                    ProduceCacheOptions,
                    v8::ScriptCompiler::NoCacheReason>
  GetCompileOptions(mojom::blink::V8CacheOptions,
                    const SingleCachedMetadataHandler*,
                    size_t source_text_length,
                    ScriptSourceLocationType);

  static scoped_refptr<CachedMetadata> GetCachedMetadata(
      const SingleCachedMetadataHandler* cache_handler,
      SingleCachedMetadataHandler::GetCachedMetadataBehavior behavior =
          SingleCachedMetadataHandler::kCrashIfUnchecked);
  static std::unique_ptr<v8::ScriptCompiler::CachedData> CreateCachedData(
      scoped_refptr<CachedMetadata>);
  static std::unique_ptr<v8::ScriptCompiler::CachedData> CreateCachedData(
      const SingleCachedMetadataHandler*);

  static void ProduceCache(v8::Isolate*,
                           CodeCacheHost*,
                           v8::Local<v8::Script>,
                           SingleCachedMetadataHandler*,
                           size_t source_text_length,
                           const KURL& source_url,
                           const WTF::TextPosition& source_start_position,
                           ProduceCacheOptions);
  static void ProduceCache(v8::Isolate*,
                           CodeCacheHost*,
                           ModuleRecordProduceCacheData*,
                           size_t source_text_length,
                           const KURL& source_url,
                           const WTF::TextPosition& source_start_position);

  static scoped_refptr<CachedMetadata> GenerateFullCodeCache(
      ScriptState*,
      const String& script_string,
      const KURL& source_url,
      const WTF::TextEncoding&,
      OpaqueMode);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_CODE_CACHE_H_
