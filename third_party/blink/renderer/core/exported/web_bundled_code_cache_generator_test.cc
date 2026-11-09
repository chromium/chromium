// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_bundled_code_cache_generator.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_location_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/webui_bundled_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

class WebBundledCodeCacheGeneratorTest : public testing::Test {
 public:
  // Checks to ensure generated `cache_data` is accepted when the `module_code`
  // is instantiated by V8.
  void ValidateCodeCacheAcceptedByV8(
      v8::Isolate* isolate,
      const WebString& module_code,
      const WebBundledCodeCacheGenerator::SerializedCodeCacheData& cache_data) {
    const std::string module_code_utf8 = module_code.Utf8();
    v8::Local<v8::String> module_source =
        v8::String::NewFromUtf8(
            isolate, module_code_utf8.c_str(), v8::NewStringType::kNormal,
            base::checked_cast<int>(module_code_utf8.size()))
            .ToLocalChecked();

    // Use a runtime module URL distinct from the generator's placeholder
    // resource name so V8 does not hit `CompilationCacheScript` from
    // `CreateSerializedCodeCacheForModule()` in the same isolate and instead
    // exercises `CodeSerializer::Deserialize()`.
    v8::ScriptOrigin origin(
        /*resource_name=*/v8::String::NewFromUtf8Literal(
            isolate, "chrome://webui-test/compiled-module.js"),
        /*resource_line_offset =*/0,
        /*resource_column_offset=*/0, /*resource_is_shared_cross_origin=*/true,
        /*script_id=*/-1,
        /*source_map_url=*/v8::Local<v8::Value>(), /*resource_is_opaque=*/false,
        /*is_wasm=*/false, /*is_module=*/true);

    scoped_refptr<CachedMetadata> cached_metadata =
        CachedMetadata::CreateFromSerializedData(Vector<uint8_t>(cache_data));
    std::unique_ptr<v8::ScriptCompiler::CachedData> cached_data =
        V8CodeCache::CreateCachedData(cached_metadata);
    const v8::ScriptCompiler::CachedData* cached_data_ptr = cached_data.get();

    // Source takes ownership of cached_data.
    v8::ScriptCompiler::Source source(module_source, origin,
                                      cached_data.release());
    v8::Local<v8::Module> module;
    EXPECT_TRUE(v8::ScriptCompiler::CompileModule(
                    isolate, &source, v8::ScriptCompiler::kConsumeCodeCache)
                    .ToLocal(&module));
    EXPECT_EQ(source.GetCompilationDetails().in_memory_cache_result,
              v8::ScriptCompiler::InMemoryCacheResult::kMiss);
    EXPECT_FALSE(cached_data_ptr->rejected);
    cached_data_ptr = nullptr;
  }

 private:
  test::TaskEnvironment task_environment_;
};

TEST_F(WebBundledCodeCacheGeneratorTest, ValidJavaScriptModule) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // Request generation of code cache data for a given module.
  const WebString module_code = WebString(
      "export const message = 'Hello, world!'; export function greet() { "
      "return message; }");
  WebBundledCodeCacheGenerator::SerializedCodeCacheData cache_data =
      WebBundledCodeCacheGenerator::CreateSerializedCodeCacheForModule(
          isolate, module_code);

  // Ensure code cache data was generated.g
  EXPECT_FALSE(cache_data.empty());

  // Ensure generated code cache data is accepted by V8.
  ValidateCodeCacheAcceptedByV8(isolate, module_code, cache_data);
}

TEST_F(WebBundledCodeCacheGeneratorTest, ValidJavaScriptModuleWithDeps) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // Request generation of code cache data for a given module with external
  // dependencies.
  const WebString module_code = WebString(
      "import { add } from './math.js'; export function calculate(a, b) { "
      "return add(a, b); }");
  WebBundledCodeCacheGenerator::SerializedCodeCacheData cache_data =
      WebBundledCodeCacheGenerator::CreateSerializedCodeCacheForModule(
          isolate, module_code);

  // Ensure code cache data was generated.
  EXPECT_FALSE(cache_data.empty());

  // Ensure generated code cache data is accepted by V8.
  ValidateCodeCacheAcceptedByV8(isolate, module_code, cache_data);
}

TEST_F(WebBundledCodeCacheGeneratorTest, InvalidJavaScriptModuleSyntax) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // Request generation of code cache data for a given module with incorrect
  // syntax.
  const WebString module_code = WebString("export const = 5;");
  WebBundledCodeCacheGenerator::SerializedCodeCacheData cache_data =
      WebBundledCodeCacheGenerator::CreateSerializedCodeCacheForModule(
          isolate, module_code);

  // Invalid modules should produce empty cache data.
  EXPECT_TRUE(cache_data.empty());
}

TEST_F(WebBundledCodeCacheGeneratorTest, EmptyJavaScriptModule) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // Request generation of code cache data for a an empty module.
  const WebString module_code = WebString("");
  WebBundledCodeCacheGenerator::SerializedCodeCacheData cache_data =
      WebBundledCodeCacheGenerator::CreateSerializedCodeCacheForModule(
          isolate, module_code);

  // Even an empty module should produce non-empty code cache data, which
  // includes relevant metadata headers.
  EXPECT_FALSE(cache_data.empty());
}

TEST_F(WebBundledCodeCacheGeneratorTest, VerifyCacheDataTypeId) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // Request generation of code cache data for a given module.
  const WebString module_code = WebString(
      "export const message = 'Hello, world!'; export function greet() { "
      "return message; }");
  WebBundledCodeCacheGenerator::SerializedCodeCacheData cache_data =
      WebBundledCodeCacheGenerator::CreateSerializedCodeCacheForModule(
          isolate, module_code);

  // Ensure code cache data was generated.
  EXPECT_FALSE(cache_data.empty());

  // Create a CachedMetadata object from the serialized data.
  scoped_refptr<CachedMetadata> cached_metadata =
      CachedMetadata::CreateFromSerializedData(Vector<uint8_t>(cache_data));
  EXPECT_FALSE(cached_metadata->SerializedData().empty());

  // Check that the DataTypeID is a match for the bundled code cache.
  EXPECT_EQ(cached_metadata->DataTypeID(),
            V8CodeCache::TagForBundledCodeCache());
}

TEST_F(WebBundledCodeCacheGeneratorTest,
       HitsIsolateCacheAcrossV8ScriptRunnerModuleCompiles) {
  base::HistogramTester histogram_tester;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  const WebString module_code = WebString(
      "export const message = 'Hello, world!'; export function greet() { "
      "return message; }");
  WebBundledCodeCacheGenerator::SerializedCodeCacheData cache_data =
      WebBundledCodeCacheGenerator::CreateSerializedCodeCacheForModule(
          isolate, module_code);
  ASSERT_FALSE(cache_data.empty());

  auto* cache_handler =
      MakeGarbageCollected<WebUIBundledCachedMetadataHandler>();
  cache_handler->SetSerializedCachedMetadata(mojo_base::BigBuffer(cache_data));

  const KURL module_url("chrome://webui-test/module.js");
  const ReferrerScriptInfo referrer_info(module_url, ScriptFetchOptions());
  ModuleScriptCreationParams cached_params(
      module_url, module_url, ScriptSourceLocationType::kExternalFile,
      ResolvedModuleType::kJavaScript,
      ParkableString(String(module_code).Impl()), cache_handler,
      network::mojom::ReferrerPolicy::kDefault, /*source_map_url=*/String());

  // First runtime compile consumes the bundled code cache and promotes the
  // deserialized Script into V8's per-isolate CompilationCacheScript.
  v8::Local<v8::Module> first_module;
  ASSERT_TRUE(V8ScriptRunner::CompileModule(
                  isolate, cached_params, TextPosition::MinimumPosition(),
                  v8::ScriptCompiler::kConsumeCodeCache,
                  v8::ScriptCompiler::kNoCacheBecauseStaticCodeCache,
                  referrer_info)
                  .ToLocal(&first_module));
  EXPECT_TRUE(cache_handler->did_use_code_cache_for_testing());
  histogram_tester.ExpectUniqueSample(
      "Blink.ResourceRequest.WebUIBundledCachedMetadataHandler.ConsumeCache",
      true, 1);

  // Subsequent runtime compiles of the same module URL in the same isolate
  // must hit CompilationCacheScript and reuse the same underlying V8 Script.
  ModuleScriptCreationParams warm_params(
      module_url, module_url, ScriptSourceLocationType::kExternalFile,
      ResolvedModuleType::kJavaScript,
      ParkableString(String(module_code).Impl()), /*cache_handler=*/nullptr,
      network::mojom::ReferrerPolicy::kDefault, /*source_map_url=*/String());
  v8::Local<v8::Module> second_module;
  ASSERT_TRUE(V8ScriptRunner::CompileModule(
                  isolate, warm_params, TextPosition::MinimumPosition(),
                  v8::ScriptCompiler::kNoCompileOptions,
                  v8::ScriptCompiler::kNoCacheBecauseStaticCodeCache,
                  referrer_info)
                  .ToLocal(&second_module));
  EXPECT_EQ(first_module->ScriptId(), second_module->ScriptId());
}

}  // namespace blink
