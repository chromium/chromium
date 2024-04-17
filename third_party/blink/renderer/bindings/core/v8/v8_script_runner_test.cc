// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"

#include "base/location.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_cache_consumer_client.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
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
  WTF::String DifferentCode() const {
    return WTF::String::Format("a = function() { 1 + 12; } // %01000d\n",
                               counter_);
  }
  KURL Url() const {
    return KURL(WTF::String::Format(code_cache_with_hashing_scheme_
                                        ? "codecachewithhashing://bla.com/bla%d"
                                        : "http://bla.com/bla%d",
                                    counter_));
  }
  unsigned TagForCodeCache(CachedMetadataHandler* cache_handler) const {
    return V8CodeCache::TagForCodeCache(cache_handler);
  }
  unsigned TagForTimeStamp(CachedMetadataHandler* cache_handler) const {
    return V8CodeCache::TagForTimeStamp(cache_handler);
  }
  void SetCacheTimeStamp(CodeCacheHost* code_cache_host,
                         CachedMetadataHandler* cache_handler) {
    V8CodeCache::SetCacheTimeStamp(code_cache_host, cache_handler);
  }

  bool CompileScript(v8::Isolate* isolate,
                     ScriptState* script_state,
                     const ClassicScript& classic_script,
                     mojom::blink::V8CacheOptions cache_options) {
    ExecutionContext* execution_context = ExecutionContext::From(script_state);
    if (classic_script.CacheHandler()) {
      classic_script.CacheHandler()->Check(
          ExecutionContext::GetCodeCacheHostFromContext(execution_context),
          classic_script.SourceText());
    }
    v8::ScriptCompiler::CompileOptions compile_options;
    V8CodeCache::ProduceCacheOptions produce_cache_options;
    v8::ScriptCompiler::NoCacheReason no_cache_reason;
    std::tie(compile_options, produce_cache_options, no_cache_reason) =
        V8CodeCache::GetCompileOptions(cache_options, classic_script);
    v8::MaybeLocal<v8::Script> compiled_script = V8ScriptRunner::CompileScript(
        script_state, classic_script,
        classic_script.CreateScriptOrigin(isolate), compile_options,
        no_cache_reason);
    if (compiled_script.IsEmpty()) {
      return false;
    }
    V8CodeCache::ProduceCache(
        isolate,
        ExecutionContext::GetCodeCacheHostFromContext(execution_context),
        compiled_script.ToLocalChecked(), classic_script.CacheHandler(),
        classic_script.SourceText().length(), classic_script.SourceUrl(),
        classic_script.StartPosition(), produce_cache_options);
    return true;
  }

  bool CompileScript(v8::Isolate* isolate,
                     ScriptState* script_state,
                     const ClassicScript& classic_script,
                     v8::ScriptCompiler::CompileOptions compile_options,
                     v8::ScriptCompiler::NoCacheReason no_cache_reason,
                     V8CodeCache::ProduceCacheOptions produce_cache_options) {
    ExecutionContext* execution_context = ExecutionContext::From(script_state);
    if (classic_script.CacheHandler()) {
      classic_script.CacheHandler()->Check(
          ExecutionContext::GetCodeCacheHostFromContext(execution_context),
          classic_script.SourceText());
    }
    v8::MaybeLocal<v8::Script> compiled_script = V8ScriptRunner::CompileScript(
        script_state, classic_script,
        classic_script.CreateScriptOrigin(isolate), compile_options,
        no_cache_reason);
    if (compiled_script.IsEmpty()) {
      return false;
    }
    V8CodeCache::ProduceCache(
        isolate,
        ExecutionContext::GetCodeCacheHostFromContext(execution_context),
        compiled_script.ToLocalChecked(), classic_script.CacheHandler(),
        classic_script.SourceText().length(), classic_script.SourceUrl(),
        classic_script.StartPosition(), produce_cache_options);
    return true;
  }

  ScriptResource* CreateEmptyResource(v8::Isolate* isolate) {
    ScriptResource* resource =
        ScriptResource::CreateForTest(isolate, NullURL(), UTF8Encoding());
    return resource;
  }

  ScriptResource* CreateResource(v8::Isolate* isolate,
                                 const WTF::TextEncoding& encoding,
                                 Vector<uint8_t> serialized_metadata,
                                 std::optional<String> code = {}) {
    return CreateResource(isolate, encoding,
                          base::make_span(serialized_metadata), code);
  }

  ScriptResource* CreateResource(
      v8::Isolate* isolate,
      const WTF::TextEncoding& encoding,
      base::span<const uint8_t> serialized_metadata = {},
      std::optional<String> code = {}) {
    ScriptResource* resource =
        ScriptResource::CreateForTest(isolate, Url(), encoding);
    if (!code)
      code = Code();
    ResourceResponse response(Url());
    response.SetHttpStatusCode(200);
    resource->ResponseReceived(response);
    if (serialized_metadata.size() != 0) {
      resource->SetSerializedCachedMetadata(serialized_metadata);
    }
    StringUTF8Adaptor code_utf8(code.value());
    resource->AppendData(code_utf8);
    resource->FinishForTest();

    return resource;
  }

  ClassicScript* CreateScript(ScriptResource* resource) {
    return ClassicScript::CreateFromResource(resource, ScriptFetchOptions());
  }

  Vector<uint8_t> CreateCachedData() {
    V8TestingScope scope;
    ClassicScript* classic_script =
        CreateScript(CreateResource(scope.GetIsolate(), UTF8Encoding()));
    // Set timestamp to simulate a warm run.
    ScriptCachedMetadataHandler* cache_handler =
        static_cast<ScriptCachedMetadataHandler*>(
            classic_script->CacheHandler());
    ExecutionContext* execution_context =
        ExecutionContext::From(scope.GetScriptState());
    SetCacheTimeStamp(
        ExecutionContext::GetCodeCacheHostFromContext(execution_context),
        cache_handler);

    // Warm run - should produce code cache.
    EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                              *classic_script,
                              mojom::blink::V8CacheOptions::kCode));

    // Check the produced cache is for code cache.
    scoped_refptr<CachedMetadata> cached_metadata =
        cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler));

    // Copy the serialized data to return it at an independent vector.
    base::span<const uint8_t> serialized_data_view =
        cached_metadata->SerializedData();
    Vector<uint8_t> ret;
    ret.AppendRange(serialized_data_view.begin(), serialized_data_view.end());
    return ret;
  }

  // TODO(leszeks): Change this from needing an explicit quit callback to
  // manually flushing the thread pool.
  void RunLoopUntilQuit(base::Location location = base::Location::Current()) {
    run_loop_.Run(location);
  }

 protected:
  static int counter_;
  test::TaskEnvironment task_environment_;
  bool code_cache_with_hashing_scheme_ = false;
  base::test::ScopedFeatureList feature_list_;
  base::RunLoop run_loop_;
};

int V8ScriptRunnerTest::counter_ = 0;

TEST_F(V8ScriptRunnerTest, resourcelessShouldPass) {
  V8TestingScope scope;
  ClassicScript* classic_script =
      ClassicScript::Create(Code(), Url(), Url(), ScriptFetchOptions(),
                            ScriptSourceLocationType::kInternal);
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            *classic_script,
                            mojom::blink::V8CacheOptions::kNone));
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            *classic_script,
                            mojom::blink::V8CacheOptions::kCode));
}

TEST_F(V8ScriptRunnerTest, emptyResourceDoesNotHaveCacheHandler) {
  V8TestingScope scope;
  ScriptResource* resource = CreateEmptyResource(scope.GetIsolate());
  EXPECT_FALSE(resource->CacheHandler());
}

TEST_F(V8ScriptRunnerTest, codeOption) {
  V8TestingScope scope;
  ClassicScript* classic_script =
      CreateScript(CreateResource(scope.GetIsolate(), UTF8Encoding()));
  CachedMetadataHandler* cache_handler = classic_script->CacheHandler();
  ExecutionContext* execution_context =
      ExecutionContext::From(scope.GetScriptState());
  SetCacheTimeStamp(
      ExecutionContext::GetCodeCacheHostFromContext(execution_context),
      cache_handler);

  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            *classic_script,
                            mojom::blink::V8CacheOptions::kCode));

  EXPECT_TRUE(cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));
  // The cached data is associated with the encoding.
  ScriptResource* another_resource =
      CreateResource(scope.GetIsolate(), UTF16LittleEndianEncoding());
  EXPECT_FALSE(cache_handler->GetCachedMetadata(
      TagForCodeCache(another_resource->CacheHandler())));
}

TEST_F(V8ScriptRunnerTest, consumeCodeOption) {
  V8TestingScope scope;
  ClassicScript* classic_script =
      CreateScript(CreateResource(scope.GetIsolate(), UTF8Encoding()));
  // Set timestamp to simulate a warm run.
  CachedMetadataHandler* cache_handler = classic_script->CacheHandler();
  ExecutionContext* execution_context =
      ExecutionContext::From(scope.GetScriptState());
  SetCacheTimeStamp(
      ExecutionContext::GetCodeCacheHostFromContext(execution_context),
      cache_handler);

  // Warm run - should produce code cache.
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            *classic_script,
                            mojom::blink::V8CacheOptions::kCode));

  // Check the produced cache is for code cache.
  EXPECT_TRUE(cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));

  // Hot run - should consume code cache.
  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(mojom::blink::V8CacheOptions::kDefault,
                                     *classic_script);
  EXPECT_EQ(produce_cache_options,
            V8CodeCache::ProduceCacheOptions::kNoProduceCache);
  EXPECT_EQ(compile_options,
            v8::ScriptCompiler::CompileOptions::kConsumeCodeCache);
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            *classic_script, compile_options, no_cache_reason,
                            produce_cache_options));
  EXPECT_TRUE(cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));
}

TEST_F(V8ScriptRunnerTest, produceAndConsumeCodeOption) {
  V8TestingScope scope;
  ClassicScript* classic_script =
      CreateScript(CreateResource(scope.GetIsolate(), UTF8Encoding()));
  CachedMetadataHandler* cache_handler = classic_script->CacheHandler();

  // Cold run - should set the timestamp.
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            *classic_script,
                            mojom::blink::V8CacheOptions::kDefault));
  EXPECT_TRUE(cache_handler->GetCachedMetadata(TagForTimeStamp(cache_handler)));
  EXPECT_FALSE(
      cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));

  // Warm run - should produce code cache.
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            *classic_script,
                            mojom::blink::V8CacheOptions::kDefault));
  EXPECT_TRUE(cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));

  // Hot run - should consume code cache.
  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(mojom::blink::V8CacheOptions::kDefault,
                                     *classic_script);
  EXPECT_EQ(produce_cache_options,
            V8CodeCache::ProduceCacheOptions::kNoProduceCache);
  EXPECT_EQ(compile_options,
            v8::ScriptCompiler::CompileOptions::kConsumeCodeCache);
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            *classic_script, compile_options, no_cache_reason,
                            produce_cache_options));
  EXPECT_TRUE(cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));
}

TEST_F(V8ScriptRunnerTest, cacheDataTypeMismatch) {
  V8TestingScope scope;
  ClassicScript* classic_script =
      CreateScript(CreateResource(scope.GetIsolate(), UTF8Encoding()));
  CachedMetadataHandler* cache_handler = classic_script->CacheHandler();
  EXPECT_FALSE(
      cache_handler->GetCachedMetadata(TagForTimeStamp(cache_handler)));
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            *classic_script,
                            mojom::blink::V8CacheOptions::kDefault));
  EXPECT_TRUE(cache_handler->GetCachedMetadata(TagForTimeStamp(cache_handler)));
  EXPECT_FALSE(
      cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));
}

TEST_F(V8ScriptRunnerTest, successfulCodeCacheWithHashing) {
  V8TestingScope scope;
#if DCHECK_IS_ON()
  // TODO(crbug.com/1329535): Remove if threaded preload scanner doesn't launch.
  // This is needed because the preload scanner creates a thread when loading a
  // page.
  WTF::SetIsBeforeThreadCreatedForTest();
#endif
  SchemeRegistry::RegisterURLSchemeAsCodeCacheWithHashing(
      "codecachewithhashing");
  code_cache_with_hashing_scheme_ = true;
  ClassicScript* classic_script =
      CreateScript(CreateResource(scope.GetIsolate(), UTF8Encoding()));
  CachedMetadataHandler* cache_handler = classic_script->CacheHandler();
  EXPECT_TRUE(cache_handler->HashRequired());

  // Cold run - should set the timestamp.
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            *classic_script,
                            mojom::blink::V8CacheOptions::kDefault));
  EXPECT_TRUE(cache_handler->GetCachedMetadata(TagForTimeStamp(cache_handler)));
  EXPECT_FALSE(
      cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));

  // Warm run - should produce code cache.
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            *classic_script,
                            mojom::blink::V8CacheOptions::kDefault));
  EXPECT_TRUE(cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));

  // Hot run - should consume code cache.
  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(mojom::blink::V8CacheOptions::kDefault,
                                     *classic_script);
  EXPECT_EQ(produce_cache_options,
            V8CodeCache::ProduceCacheOptions::kNoProduceCache);
  EXPECT_EQ(compile_options,
            v8::ScriptCompiler::CompileOptions::kConsumeCodeCache);
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            *classic_script, compile_options, no_cache_reason,
                            produce_cache_options));
  EXPECT_TRUE(cache_handler->GetCachedMetadata(TagForCodeCache(cache_handler)));
}

TEST_F(V8ScriptRunnerTest, codeCacheWithFailedHashCheck) {
  V8TestingScope scope;
#if DCHECK_IS_ON()
  // TODO(crbug.com/1329535): Remove if threaded preload scanner doesn't launch.
  // This is needed because the preload scanner creates a thread when loading a
  // page.
  WTF::SetIsBeforeThreadCreatedForTest();
#endif
  SchemeRegistry::RegisterURLSchemeAsCodeCacheWithHashing(
      "codecachewithhashing");
  code_cache_with_hashing_scheme_ = true;

  ClassicScript* classic_script_1 =
      CreateScript(CreateResource(scope.GetIsolate(), UTF8Encoding()));
  ScriptCachedMetadataHandlerWithHashing* cache_handler_1 =
      static_cast<ScriptCachedMetadataHandlerWithHashing*>(
          classic_script_1->CacheHandler());
  EXPECT_TRUE(cache_handler_1->HashRequired());

  // Cold run - should set the timestamp.
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            *classic_script_1,
                            mojom::blink::V8CacheOptions::kDefault));
  EXPECT_TRUE(
      cache_handler_1->GetCachedMetadata(TagForTimeStamp(cache_handler_1)));
  EXPECT_FALSE(
      cache_handler_1->GetCachedMetadata(TagForCodeCache(cache_handler_1)));

  // A second script with matching script text, using the state of
  // the ScriptCachedMetadataHandler from the first script.
  ClassicScript* classic_script_2 = CreateScript(
      CreateResource(scope.GetIsolate(), UTF8Encoding(),
                     cache_handler_1->GetSerializedCachedMetadata()));
  ScriptCachedMetadataHandlerWithHashing* cache_handler_2 =
      static_cast<ScriptCachedMetadataHandlerWithHashing*>(
          classic_script_2->CacheHandler());
  EXPECT_TRUE(cache_handler_2->HashRequired());

  // Warm run - should produce code cache.
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            *classic_script_2,
                            mojom::blink::V8CacheOptions::kDefault));
  EXPECT_TRUE(
      cache_handler_2->GetCachedMetadata(TagForCodeCache(cache_handler_2)));

  // A third script with different script text, using the state of
  // the ScriptCachedMetadataHandler from the second script.
  ClassicScript* classic_script_3 = CreateScript(CreateResource(
      scope.GetIsolate(), UTF8Encoding(),
      cache_handler_2->GetSerializedCachedMetadata(), DifferentCode()));
  ScriptCachedMetadataHandlerWithHashing* cache_handler_3 =
      static_cast<ScriptCachedMetadataHandlerWithHashing*>(
          classic_script_3->CacheHandler());
  EXPECT_TRUE(cache_handler_3->HashRequired());

  // Since the third script's text doesn't match the first two, the hash check
  // should reject the existing code cache data and the cache entry should
  // be updated back to a timestamp like it would during a cold run.
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            *classic_script_3,
                            mojom::blink::V8CacheOptions::kDefault));
  EXPECT_TRUE(
      cache_handler_3->GetCachedMetadata(TagForTimeStamp(cache_handler_3)));
  EXPECT_FALSE(
      cache_handler_3->GetCachedMetadata(TagForCodeCache(cache_handler_3)));

  // A fourth script with matching script text, using the state of
  // the ScriptCachedMetadataHandler from the third script.
  ClassicScript* classic_script_4 = CreateScript(
      CreateResource(scope.GetIsolate(), UTF8Encoding(),
                     cache_handler_3->GetSerializedCachedMetadata()));
  ScriptCachedMetadataHandlerWithHashing* cache_handler_4 =
      static_cast<ScriptCachedMetadataHandlerWithHashing*>(
          classic_script_4->CacheHandler());
  EXPECT_TRUE(cache_handler_4->HashRequired());

  // Running the original script again once again sets the timestamp since the
  // content has changed again.
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            *classic_script_4,
                            mojom::blink::V8CacheOptions::kDefault));
  EXPECT_TRUE(
      cache_handler_4->GetCachedMetadata(TagForTimeStamp(cache_handler_4)));
  EXPECT_FALSE(
      cache_handler_4->GetCachedMetadata(TagForCodeCache(cache_handler_4)));
}

namespace {

class StubScriptCacheConsumerClient final
    : public GarbageCollected<StubScriptCacheConsumerClient>,
      public ScriptCacheConsumerClient {
 public:
  explicit StubScriptCacheConsumerClient(base::OnceClosure finish_closure)
      : finish_closure_(std::move(finish_closure)) {}

  void NotifyCacheConsumeFinished() override {
    cache_consume_finished_ = true;
    std::move(finish_closure_).Run();
  }

  bool cache_consume_finished() { return cache_consume_finished_; }

 private:
  base::OnceClosure finish_closure_;
  bool cache_consume_finished_ = false;
};

}  // namespace

TEST_F(V8ScriptRunnerTest, successfulOffThreadCodeCache) {
  feature_list_.InitAndEnableFeature(
      blink::features::kConsumeCodeCacheOffThread);

  Vector<uint8_t> cached_data = CreateCachedData();
  EXPECT_GT(cached_data.size(), 0u);

  V8TestingScope scope;

  // Hot run - should start an off-thread code cache consumption.
  ScriptResource* resource =
      CreateResource(scope.GetIsolate(), UTF8Encoding(), cached_data);
  EXPECT_TRUE(V8CodeCache::HasCodeCache(resource->CacheHandler()));
  ClassicScript* classic_script = CreateScript(resource);
  EXPECT_NE(classic_script->CacheConsumer(), nullptr);
  auto* consumer_client = MakeGarbageCollected<StubScriptCacheConsumerClient>(
      run_loop_.QuitClosure());
  classic_script->CacheConsumer()->NotifyClientWaiting(
      consumer_client, classic_script,
      scheduler::GetSingleThreadTaskRunnerForTesting());

  // Wait until the ScriptCacheConsumer completes. ScriptCacheConsumer will
  // post a task for the client to signal that it has completed, which will
  // post a QuitClosure to this RunLoop.
  RunLoopUntilQuit();

  EXPECT_TRUE(consumer_client->cache_consume_finished());

  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(mojom::blink::V8CacheOptions::kDefault,
                                     *classic_script);
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            *classic_script, compile_options, no_cache_reason,
                            produce_cache_options));
}

TEST_F(V8ScriptRunnerTest, discardOffThreadCodeCacheWithDifferentSource) {
  feature_list_.InitAndEnableFeature(
      blink::features::kConsumeCodeCacheOffThread);

  Vector<uint8_t> cached_data = CreateCachedData();
  EXPECT_GT(cached_data.size(), 0u);

  V8TestingScope scope;

  // Hot run - should start an off-thread code cache consumption.
  ScriptResource* resource = CreateResource(scope.GetIsolate(), UTF8Encoding(),
                                            cached_data, DifferentCode());
  ClassicScript* classic_script = CreateScript(resource);
  EXPECT_NE(classic_script->CacheConsumer(), nullptr);
  auto* consumer_client = MakeGarbageCollected<StubScriptCacheConsumerClient>(
      run_loop_.QuitClosure());
  classic_script->CacheConsumer()->NotifyClientWaiting(
      consumer_client, classic_script,
      scheduler::GetSingleThreadTaskRunnerForTesting());

  // Wait until the ScriptCacheConsumer completes. ScriptCacheConsumer will
  // post a task for the client to signal that it has completed, which will
  // post a QuitClosure to this RunLoop.
  RunLoopUntilQuit();

  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(mojom::blink::V8CacheOptions::kDefault,
                                     *classic_script);
  EXPECT_EQ(produce_cache_options,
            V8CodeCache::ProduceCacheOptions::kNoProduceCache);
  EXPECT_EQ(compile_options,
            v8::ScriptCompiler::CompileOptions::kConsumeCodeCache);
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            *classic_script, compile_options, no_cache_reason,
                            produce_cache_options));
  // Code cache should have been cleared after being rejected.
  EXPECT_FALSE(V8CodeCache::HasCodeCache(resource->CacheHandler()));
}

TEST_F(V8ScriptRunnerTest, discardOffThreadCodeCacheWithBitCorruption) {
  feature_list_.InitAndEnableFeature(
      blink::features::kConsumeCodeCacheOffThread);

  Vector<uint8_t> cached_data = CreateCachedData();
  EXPECT_GT(cached_data.size(), 0u);

  V8TestingScope scope;

  // Corrupt the cached data.
  Vector<uint8_t> corrupted_data = cached_data;
  corrupted_data[sizeof(CachedMetadataHeader) + 2] ^= 0x1;

  // Hot run - should start an off-thread code cache consumption.
  ScriptResource* resource =
      CreateResource(scope.GetIsolate(), UTF8Encoding(), corrupted_data);
  ClassicScript* classic_script = CreateScript(resource);
  EXPECT_NE(classic_script->CacheConsumer(), nullptr);
  auto* consumer_client = MakeGarbageCollected<StubScriptCacheConsumerClient>(
      run_loop_.QuitClosure());
  classic_script->CacheConsumer()->NotifyClientWaiting(
      consumer_client, classic_script,
      scheduler::GetSingleThreadTaskRunnerForTesting());

  // Wait until the ScriptCacheConsumer completes. ScriptCacheConsumer will
  // post a task for the client to signal that it has completed, which will
  // post a QuitClosure to this RunLoop.
  RunLoopUntilQuit();

  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(mojom::blink::V8CacheOptions::kDefault,
                                     *classic_script);
  EXPECT_EQ(produce_cache_options,
            V8CodeCache::ProduceCacheOptions::kNoProduceCache);
  EXPECT_EQ(compile_options,
            v8::ScriptCompiler::CompileOptions::kConsumeCodeCache);
  EXPECT_TRUE(CompileScript(scope.GetIsolate(), scope.GetScriptState(),
                            *classic_script, compile_options, no_cache_reason,
                            produce_cache_options));
  // Code cache should have been cleared after being rejected.
  EXPECT_FALSE(V8CodeCache::HasCodeCache(resource->CacheHandler()));
}

}  // namespace

}  // namespace blink
