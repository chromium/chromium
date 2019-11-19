// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_CODE_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_CODE_CACHE_H_

#include <stdint.h>

#include "third_party/blink/renderer/bindings/core/v8/script_source_location_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_cache_options.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace WTF {
class TextEncoding;
class TextPosition;
}  // namespace WTF

namespace blink {

class CachedMetadata;
class KURL;
class SingleCachedMetadataHandler;
class ScriptSourceCode;
class ModuleRecordProduceCacheData;

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
  static void SetCacheTimeStamp(SingleCachedMetadataHandler*);

  // Returns true iff the SingleCachedMetadataHandler contains a code cache
  // that can be consumed by V8.
  static bool HasCodeCache(const SingleCachedMetadataHandler*);

  static std::tuple<v8::ScriptCompiler::CompileOptions,
                    ProduceCacheOptions,
                    v8::ScriptCompiler::NoCacheReason>
  GetCompileOptions(V8CacheOptions, const ScriptSourceCode&);
  static std::tuple<v8::ScriptCompiler::CompileOptions,
                    ProduceCacheOptions,
                    v8::ScriptCompiler::NoCacheReason>
  GetCompileOptions(V8CacheOptions,
                    const SingleCachedMetadataHandler*,
                    size_t source_text_length,
                    ScriptSourceLocationType);

  static v8::ScriptCompiler::CachedData* CreateCachedData(
      const SingleCachedMetadataHandler*);

  static void ProduceCache(v8::Isolate*,
                           v8::Local<v8::Script>,
                           const ScriptSourceCode&,
                           ProduceCacheOptions);
  static void ProduceCache(v8::Isolate*,
                           ModuleRecordProduceCacheData*,
                           size_t source_text_length,
                           const KURL& source_url,
                           const WTF::TextPosition& source_start_position);

  static scoped_refptr<CachedMetadata> GenerateFullCodeCache(
      ScriptState*,
      const String& script_string,
      const String& file_name,
      const WTF::TextEncoding&,
      OpaqueMode);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_CODE_CACHE_H_
