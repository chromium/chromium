// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_bundled_code_cache_generator.h"

#include "base/logging.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"

namespace blink {

WebBundledCodeCacheGenerator::SerializedCodeCacheData
WebBundledCodeCacheGenerator::CreateSerializedCodeCacheForModule(
    v8::Isolate* isolate,
    const WebString& module_text) {
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);
  v8::TryCatch trycatch(isolate);

  const std::string module_text_utf8 = module_text.Utf8();
  v8::Local<v8::String> module_source =
      v8::String::NewFromUtf8(isolate, module_text_utf8.c_str(),
                              v8::NewStringType::kNormal,
                              module_text_utf8.size())
          .ToLocalChecked();

  v8::Local<v8::String> resource_name =
      v8::String::NewFromUtf8Literal(isolate, "compiled-module");
  v8::ScriptOrigin origin(
      resource_name, /*resource_line_offset =*/0,
      /*resource_column_offset=*/0, /*resource_is_shared_cross_origin=*/false,
      /*script_id=*/-1,
      /*source_map_url=*/v8::Local<v8::Value>(), /*resource_is_opaque=*/false,
      /*is_wasm=*/false, /*is_module=*/true);
  v8::ScriptCompiler::Source source(module_source, origin);
  v8::Local<v8::Module> module;
  if (!v8::ScriptCompiler::CompileModule(isolate, &source,
                                         v8::ScriptCompiler::kEagerCompile)
           .ToLocal(&module)) {
    LOG(ERROR) << "Module compilation failed.";
    CHECK(trycatch.HasCaught());
    return WebBundledCodeCacheGenerator::SerializedCodeCacheData();
  }

  std::unique_ptr<v8::ScriptCompiler::CachedData> cache_data(
      v8::ScriptCompiler::CreateCodeCache(module->GetUnboundModuleScript()));
  scoped_refptr<blink::CachedMetadata> cached_metadata =
      blink::CachedMetadata::Create(
          blink::V8CodeCache::TagForBundledCodeCache(), ToSpan(*cache_data));
  base::span<const uint8_t> serialized_data = cached_metadata->SerializedData();
  return {serialized_data.begin(), serialized_data.end()};
}

}  // namespace blink
