// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_bundled_code_cache_generator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class WebBundledCodeCacheGeneratorTest : public testing::Test {
 public:
  // Checks to ensure generated `cache_data` is accepted when the `module_code`
  // is instantiated by V8.
  void ValidateCodeCacheAcceptedByV8(
      v8::Isolate* isolate,
      const WebString& module_code,
      const WebBundledCodeCacheGenerator::SerializedCodeCacheData& cache_data) {
    v8::Local<v8::String> module_source =
        v8::String::NewFromUtf8(isolate, module_code.Utf8().c_str(),
                                v8::NewStringType::kNormal,
                                module_code.Utf8().size())
            .ToLocalChecked();

    v8::ScriptOrigin origin(
        /*resource_name=*/v8::String::NewFromUtf8Literal(isolate,
                                                         "compiled-module"),
        /*resource_line_offset =*/0,
        /*resource_column_offset=*/0, /*resource_is_shared_cross_origin=*/false,
        /*script_id=*/-1,
        /*source_map_url=*/v8::Local<v8::Value>(), /*resource_is_opaque=*/false,
        /*is_wasm=*/false, /*is_module=*/true);

    auto cached_data = std::make_unique<v8::ScriptCompiler::CachedData>(
        cache_data.data(), cache_data.size());
    v8::ScriptCompiler::CachedData* cached_data_ptr = cached_data.get();

    // Source takes ownership of cached_data.
    v8::ScriptCompiler::Source source(module_source, origin,
                                      cached_data.release());
    v8::Local<v8::Module> module;
    EXPECT_TRUE(v8::ScriptCompiler::CompileModule(
                    isolate, &source, v8::ScriptCompiler::kConsumeCodeCache)
                    .ToLocal(&module));
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
  const WebString module_code = WebString::FromUTF8(
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
  const WebString module_code = WebString::FromUTF8(
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
  const WebString module_code = WebString::FromUTF8("export const = 5;");
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
  const WebString module_code = WebString::FromUTF8("");
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
  const WebString module_code = WebString::FromUTF8(
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

}  // namespace blink
