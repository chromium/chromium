// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class V8ScriptRunnerTest : public testing::Test {
 public:
  V8ScriptRunnerTest() = default;
  ~V8ScriptRunnerTest() override = default;

  void SetUp() override {
    // To trick various layers of caching, increment a counter for each
    // test and use it in Code() and Url().
    counter_++;
  }

  WTF::String Code() const {
    // Simple function for testing. Note:
    // - Add counter to trick V8 code cache.
    // - Pad counter to 1000 digits, to trick minimal cacheability threshold.
    return WTF::String::Format("a = function() { 1 + 1; } // %01000d\n",
                               counter_);
  }
  KURL Url() const {
    return KURL(WTF::String::Format("http://bla.com/bla%d", counter_));
  }
  unsigned TagForCodeCache(SingleCachedMetadataHandler* cache_handler) const {
    return V8CodeCache::TagForCodeCache(cache_handler);
  }
  unsigned TagForTimeStamp(SingleCachedMetadataHandler* cache_handler) const {
    return V8CodeCache::TagForTimeStamp(cache_handler);
  }
  void SetCacheTimeStamp(SingleCachedMetadataHandler* cache_handler) {
    V8CodeCache::SetCacheTimeStamp(cache_handler);
  }

  bool CompileScript(v8::Isolate* isolate,
                     ScriptState* script_state,
                     const ScriptSourceCode& source_code,
                     V8CacheOptions cache_options) {
    v8::ScriptCompiler::CompileOptions compile_options;
    V8CodeCache::ProduceCacheOptions produce_cache_options;
    v8::ScriptCompiler::NoCacheReason no_cache_reason;
    std::tie(compile_options, produce_cache_options, no_cache_reason) =
        V8CodeCache::GetCompileOptions(cache_options, source_code);
    v8::MaybeLocal<v8::Script> compiled_script = V8ScriptRunner::CompileScript(
        script_state, source_code, SanitizeScriptErrors::kSanitize,
        compile_options, no_cache_reason, ReferrerScriptInfo());
    if (compiled_script.IsEmpty()) {
      return false;
    }
    V8CodeCache::ProduceCache(isolate, compiled_script.ToLocalChecked(),
                              source_code, produce_cache_options);
    return true;
  }

  bool CompileScript(v8::Isolate* isolate,
                     ScriptState* script_state,
                     const ScriptSourceCode& source_code,
                     v8::ScriptCompiler::CompileOptions compile_options,
                     v8::ScriptCompiler::NoCacheReason no_cache_reason,
                     V8CodeCache::ProduceCacheOptions produce_cache_options) {
    v8::MaybeLocal<v8::Script> compiled_script = V8ScriptRunner::CompileScript(
        script_state, source_code, SanitizeScriptErrors::kSanitize,
        compile_options, no_cache_reason, ReferrerScriptInfo());
    if (compiled_script.IsEmpty()) {
      return false;
    }
    V8CodeCache::ProduceCache(isolate, compiled_script.ToLocalChecked(),
                              source_code, produce_cache_options);
    return true;
  }

  ScriptResource* CreateEmptyResource() {
    ScriptResource* resource =
        ScriptResource::CreateForTest(NullURL(), UTF8Encoding());
    return resource;
  }

  ScriptResource* CreateResource(const WTF::TextEncoding& encoding) {
    ScriptResource* resource = ScriptResource::CreateForTest(Url(), encoding);
    String code = Code();
    ResourceResponse response(Url());
    response.SetHttpStatusCode(200);
    resource->SetResponse(response);
    StringUTF8Adaptor code_utf8(code);
    resource->AppendData(code_utf8.data(), code_utf8.size());
    resource->FinishForTest();
    return resource;
  }

 protected:
  static int counter_;
  base::test::ScopedFeatureList feature_list_;
};

int V8ScriptRunnerTest::counter_ = 0;

class HistogramCounter {
 public:
  explicit HistogramCounter(const base::HistogramTester& tester)
      : tester_(tester) {}

  int32_t GetTotal() { return GetSamples()->TotalCount(); }

  int32_t GetPresent() { return GetCount(StateOnGet::kPresent); }

  int32_t GetDataTypeMismatch() {
    return GetCount(StateOnGet::kDataTypeMismatch);
  }

  int32_t GetWasNeverPresent() {
    return GetCount(StateOnGet::kWasNeverPresent);
  }

  int32_t GetDiscarded() { return GetCount(StateOnGet::kWasDiscarded); }

 private:
  std::unique_ptr<base::HistogramSamples> GetSamples() {
    return tester_.GetHistogramSamplesSinceCreation(
        "Memory.Renderer.BlinkCachedMetadataGetResult");
  }

  int32_t GetCount(StateOnGet state) {
    static_assert((std::is_same<int, base::HistogramBase::Sample>::value), "");
    return GetSamples()->GetCount(static_cast<int>(state));
  }

  const base::HistogramTester& tester_;
};

TEST_F(V8ScriptRunnerTest, resourcelessShouldPass) {
  V8TestingScope scope;
  ScriptSourceCode source_code(Code(), ScriptSourceLocationType::kInternal,
                               nullptr /* cache_handler */, Url());
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            source_code, kV8CacheOptionsNone));
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            source_code, kV8CacheOptionsCode));
}

TEST_F(V8ScriptRunnerTest, emptyResourceDoesNotHaveCacheHandler) {
  ScriptResource* resource = CreateEmptyResource();
  EXPECT_FALSE(resource->CacheHandler());
}

TEST_F(V8ScriptRunnerTest, codeOption) {
  V8TestingScope scope;
  ScriptSourceCode source_code(
      nullptr, CreateResource(UTF8Encoding()),
      ScriptStreamer::NotStreamingReason::kScriptTooSmall);
  SingleCachedMetadataHandler* cache_handler = source_code.CacheHandler();
  SetCacheTimeStamp(cache_handler);

  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            source_code, kV8CacheOptionsCode));

  EXPECT_TRUE(cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));
  // The cached data is associated with the encoding.
  ScriptResource* another_resource =
      CreateResource(UTF16LittleEndianEncoding());
  EXPECT_FALSE(cache_handler->GetCachedMetadata(
      TagForCodeCache(another_resource->CacheHandler())));
}

TEST_F(V8ScriptRunnerTest, consumeCodeOptionWithoutDiscarding) {
  feature_list_.InitAndDisableFeature(
      blink::features::kDiscardCodeCacheAfterFirstUse);
  V8TestingScope scope;
  ScriptSourceCode source_code(
      nullptr, CreateResource(UTF8Encoding()),
      ScriptStreamer::NotStreamingReason::kScriptTooSmall);
  // Set timestamp to simulate a warm run.
  SingleCachedMetadataHandler* cache_handler = source_code.CacheHandler();
  SetCacheTimeStamp(cache_handler);

  // Warm run - should produce code cache.
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            source_code, kV8CacheOptionsCode));

  // Check the produced cache is for code cache.
  EXPECT_TRUE(cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));

  // Hot run - should consume code cache.
  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(kV8CacheOptionsDefault, source_code);
  EXPECT_EQ(produce_cache_options,
            V8CodeCache::ProduceCacheOptions::kNoProduceCache);
  EXPECT_EQ(compile_options,
            v8::ScriptCompiler::CompileOptions::kConsumeCodeCache);
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            source_code, compile_options, no_cache_reason,
                            produce_cache_options));
  EXPECT_TRUE(cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));
}

TEST_F(V8ScriptRunnerTest, consumeCodeOptionWithDiscarding) {
  feature_list_.InitAndEnableFeature(
      blink::features::kDiscardCodeCacheAfterFirstUse);
  V8TestingScope scope;
  ScriptSourceCode source_code(
      nullptr, CreateResource(UTF8Encoding()),
      ScriptStreamer::NotStreamingReason::kScriptTooSmall);
  // Set timestamp to simulate a warm run.
  SingleCachedMetadataHandler* cache_handler = source_code.CacheHandler();
  SetCacheTimeStamp(cache_handler);

  // Warm run - should produce code cache.
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            source_code, kV8CacheOptionsCode));

  // Check the produced cache is for code cache.
  EXPECT_TRUE(cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));

  // Hot run - should consume code cache.
  base::HistogramTester tester;
  HistogramCounter counter(tester);
  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(kV8CacheOptionsDefault, source_code);
  EXPECT_EQ(1, counter.GetTotal());
  EXPECT_EQ(1, counter.GetPresent());
  EXPECT_EQ(produce_cache_options,
            V8CodeCache::ProduceCacheOptions::kNoProduceCache);
  EXPECT_EQ(compile_options,
            v8::ScriptCompiler::CompileOptions::kConsumeCodeCache);
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            source_code, compile_options, no_cache_reason,
                            produce_cache_options));
  EXPECT_EQ(2, counter.GetTotal());
  EXPECT_EQ(2, counter.GetPresent());
  EXPECT_FALSE(
      cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));
  EXPECT_EQ(3, counter.GetTotal());
  EXPECT_EQ(1, counter.GetDiscarded());
}

TEST_F(V8ScriptRunnerTest, produceAndConsumeCodeOptionWithoutDiscarding) {
  feature_list_.InitAndDisableFeature(
      blink::features::kDiscardCodeCacheAfterFirstUse);
  V8TestingScope scope;
  ScriptSourceCode source_code(
      nullptr, CreateResource(UTF8Encoding()),
      ScriptStreamer::NotStreamingReason::kScriptTooSmall);
  SingleCachedMetadataHandler* cache_handler = source_code.CacheHandler();

  // Cold run - should set the timestamp.
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            source_code, kV8CacheOptionsDefault));
  EXPECT_TRUE(cache_handler->GetCachedMetadata(TagForTimeStamp(cache_handler)));
  EXPECT_FALSE(
      cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));

  // Warm run - should produce code cache.
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            source_code, kV8CacheOptionsDefault));
  EXPECT_TRUE(cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));

  // Hot run - should consume code cache.
  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(kV8CacheOptionsDefault, source_code);
  EXPECT_EQ(produce_cache_options,
            V8CodeCache::ProduceCacheOptions::kNoProduceCache);
  EXPECT_EQ(compile_options,
            v8::ScriptCompiler::CompileOptions::kConsumeCodeCache);
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            source_code, compile_options, no_cache_reason,
                            produce_cache_options));
  EXPECT_TRUE(cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));
}

TEST_F(V8ScriptRunnerTest, produceAndConsumeCodeOptionWithDiscarding) {
  feature_list_.InitAndEnableFeature(
      blink::features::kDiscardCodeCacheAfterFirstUse);
  V8TestingScope scope;
  ScriptSourceCode source_code(
      nullptr, CreateResource(UTF8Encoding()),
      ScriptStreamer::NotStreamingReason::kScriptTooSmall);
  SingleCachedMetadataHandler* cache_handler = source_code.CacheHandler();

  // Cold run - should set the timestamp.
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            source_code, kV8CacheOptionsDefault));
  EXPECT_TRUE(cache_handler->GetCachedMetadata(TagForTimeStamp(cache_handler)));
  EXPECT_FALSE(
      cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));

  // Warm run - should produce code cache.
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            source_code, kV8CacheOptionsDefault));
  EXPECT_TRUE(cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));

  // Hot run - should consume code cache.
  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(kV8CacheOptionsDefault, source_code);
  EXPECT_EQ(produce_cache_options,
            V8CodeCache::ProduceCacheOptions::kNoProduceCache);
  EXPECT_EQ(compile_options,
            v8::ScriptCompiler::CompileOptions::kConsumeCodeCache);
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            source_code, compile_options, no_cache_reason,
                            produce_cache_options));
  EXPECT_FALSE(
      cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));
}

TEST_F(V8ScriptRunnerTest, cacheRequestedBeforeProduced) {
  feature_list_.InitAndEnableFeature(
      blink::features::kDiscardCodeCacheAfterFirstUse);
  V8TestingScope scope;
  ScriptSourceCode source_code(
      nullptr, CreateResource(UTF8Encoding()),
      ScriptStreamer::NotStreamingReason::kScriptTooSmall);
  SingleCachedMetadataHandler* cache_handler = source_code.CacheHandler();
  base::HistogramTester tester;
  HistogramCounter counter(tester);
  EXPECT_FALSE(
      cache_handler->GetCachedMetadata(TagForTimeStamp(cache_handler)));
  EXPECT_EQ(1, counter.GetTotal());
  EXPECT_EQ(1, counter.GetWasNeverPresent());
}

TEST_F(V8ScriptRunnerTest, cacheDataTypeMismatch) {
  feature_list_.InitAndEnableFeature(
      blink::features::kDiscardCodeCacheAfterFirstUse);
  V8TestingScope scope;
  ScriptSourceCode source_code(
      nullptr, CreateResource(UTF8Encoding()),
      ScriptStreamer::NotStreamingReason::kScriptTooSmall);
  SingleCachedMetadataHandler* cache_handler = source_code.CacheHandler();
  EXPECT_FALSE(
      cache_handler->GetCachedMetadata(TagForTimeStamp(cache_handler)));
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            source_code, kV8CacheOptionsDefault));
  EXPECT_TRUE(cache_handler->GetCachedMetadata(TagForTimeStamp(cache_handler)));
  base::HistogramTester tester;
  HistogramCounter counter(tester);
  EXPECT_FALSE(
      cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));
  EXPECT_EQ(1, counter.GetTotal());
  EXPECT_EQ(1, counter.GetDataTypeMismatch());
}

}  // namespace

}  // namespace blink
