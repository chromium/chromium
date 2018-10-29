// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"

#include <memory>

#include "base/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/script_streamer_thread.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/script/classic_pending_script.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/script/mock_script_element_base.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/cross_origin_attribute_value.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class ScriptStreamingTest : public testing::Test {
 public:
  ScriptStreamingTest()
      : loading_task_runner_(scheduler::GetSingleThreadTaskRunnerForTesting()),
        dummy_page_holder_(DummyPageHolder::Create(IntSize(800, 600))) {
    dummy_page_holder_->GetPage().GetSettings().SetScriptEnabled(true);
    MockScriptElementBase* element = MockScriptElementBase::Create();
    // Basically we are not interested in ScriptElementBase* calls, just making
    // the method(s) to return default values.
    EXPECT_CALL(*element, IntegrityAttributeValue())
        .WillRepeatedly(testing::Return(String()));
    EXPECT_CALL(*element, GetDocument())
        .WillRepeatedly(testing::ReturnRef(dummy_page_holder_->GetDocument()));
    EXPECT_CALL(*element, Loader()).WillRepeatedly(testing::Return(nullptr));

    KURL url("http://www.streaming-test.com/");
    Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
        url, WrappedResourceResponse(ResourceResponse()), "");
    pending_script_ = ClassicPendingScript::Fetch(
        url, dummy_page_holder_->GetDocument(), ScriptFetchOptions(),
        kCrossOriginAttributeNotSet, UTF8Encoding(), element,
        FetchParameters::kNoDefer);
    pending_script_->SetSchedulingType(ScriptSchedulingType::kParserBlocking);
    ScriptStreamer::SetSmallScriptThresholdForTesting(0);
    Platform::Current()->GetURLLoaderMockFactory()->UnregisterURL(url);

    ResourceResponse response(url);
    response.SetHTTPStatusCode(200);
    GetResource()->SetResponse(response);
  }

  ~ScriptStreamingTest() override {
    if (pending_script_)
      pending_script_->Dispose();
  }

  ClassicPendingScript* GetPendingScript() const {
    return pending_script_.Get();
  }

  ScriptSourceCode GetScriptSourceCode() const {
    ClassicScript* classic_script = GetPendingScript()->GetSource(NullURL());
    DCHECK(classic_script);
    return classic_script->GetScriptSourceCode();
  }

  Settings* GetSettings() const {
    return &dummy_page_holder_->GetPage().GetSettings();
  }

  ScriptResource* GetResource() const {
    return ToScriptResource(pending_script_->GetResource());
  }

 protected:
  void AppendData(ScriptResource* resource, const char* data) {
    resource->AppendData(data, strlen(data));
    // Yield control to the background thread, so that V8 gets a chance to
    // process the data before the main thread adds more. Note that we
    // cannot fully control in what kind of chunks the data is passed to V8
    // (if V8 is not requesting more data between two appendData calls, it
    // will get both chunks together).
    test::YieldCurrentThread();
  }

  void AppendData(const char* data) { AppendData(GetResource(), data); }

  void AppendPadding(ScriptResource* resource) {
    for (int i = 0; i < 10; ++i) {
      AppendData(resource,
                 " /* this is padding to make the script long enough, so "
                 "that V8's buffer gets filled and it starts processing "
                 "the data */ ");
    }
  }

  void AppendPadding() { AppendPadding(GetResource()); }

  void Finish() {
    GetResource()->Loader()->DidFinishLoading(
        TimeTicks(), 0, 0, 0, false,
        std::vector<network::cors::PreflightTimingInfo>());
    GetResource()->SetStatus(ResourceStatus::kCached);
  }

  void ProcessTasksUntilStreamingComplete() {
    if (!RuntimeEnabledFeatures::ScheduledScriptStreamingEnabled()) {
      while (ScriptStreamerThread::Shared()->IsRunningTask()) {
        test::RunPendingTasks();
      }
    }
    // Once more, because the "streaming complete" notification might only
    // now be in the task queue.
    test::RunPendingTasks();
  }

  scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner_;
  // The PendingScript where we stream from. These don't really
  // fetch any data outside the test; the test controls the data by calling
  // ScriptResource::AppendData.
  Persistent<ClassicPendingScript> pending_script_;

  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

class TestPendingScriptClient final
    : public GarbageCollectedFinalized<TestPendingScriptClient>,
      public PendingScriptClient {
  USING_GARBAGE_COLLECTED_MIXIN(TestPendingScriptClient);

 public:
  TestPendingScriptClient() : finished_(false) {}
  void PendingScriptFinished(PendingScript*) override { finished_ = true; }
  bool Finished() const { return finished_; }

 private:
  bool finished_;
};

TEST_F(ScriptStreamingTest, CompilingStreamedScript) {
  // Test that we can successfully compile a streamed script.
  V8TestingScope scope;
  ScriptStreamer::NotStreamingReason reason;
  ScriptStreamer::StartStreaming(GetPendingScript(), loading_task_runner_,
                                 &reason);
  GetPendingScript()->SetNotStreamingReasonForTest(reason);
  TestPendingScriptClient* client = new TestPendingScriptClient;
  GetPendingScript()->WatchForLoad(client);

  AppendData("function foo() {");
  AppendPadding();
  AppendData("return 5; }");
  AppendPadding();
  AppendData("foo();");
  EXPECT_FALSE(client->Finished());
  Finish();

  // Process tasks on the main thread until the streaming background thread
  // has completed its tasks.
  ProcessTasksUntilStreamingComplete();
  EXPECT_TRUE(client->Finished());
  ScriptSourceCode source_code = GetScriptSourceCode();
  EXPECT_TRUE(source_code.Streamer());
  v8::TryCatch try_catch(scope.GetIsolate());
  v8::Local<v8::Script> script;
  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(kV8CacheOptionsDefault, source_code);
  EXPECT_TRUE(V8ScriptRunner::CompileScript(
                  scope.GetScriptState(), source_code, kSharableCrossOrigin,
                  compile_options, no_cache_reason, ReferrerScriptInfo())
                  .ToLocal(&script));
  EXPECT_FALSE(try_catch.HasCaught());
}

TEST_F(ScriptStreamingTest, CompilingStreamedScriptWithParseError) {
  // Test that scripts with parse errors are handled properly. In those cases,
  // the V8 side typically finished before loading finishes: make sure we
  // handle it gracefully.
  V8TestingScope scope;
  ScriptStreamer::NotStreamingReason reason;
  ScriptStreamer::StartStreaming(GetPendingScript(), loading_task_runner_,
                                 &reason);
  GetPendingScript()->SetNotStreamingReasonForTest(reason);
  TestPendingScriptClient* client = new TestPendingScriptClient;
  GetPendingScript()->WatchForLoad(client);
  AppendData("function foo() {");
  AppendData("this is the part which will be a parse error");
  // V8 won't realize the parse error until it actually starts parsing the
  // script, and this happens only when its buffer is filled.
  AppendPadding();

  EXPECT_FALSE(client->Finished());

  // Force the V8 side to finish before the loading.
  ProcessTasksUntilStreamingComplete();
  EXPECT_FALSE(client->Finished());

  Finish();
  EXPECT_TRUE(client->Finished());

  ScriptSourceCode source_code = GetScriptSourceCode();
  EXPECT_TRUE(source_code.Streamer());
  v8::TryCatch try_catch(scope.GetIsolate());
  v8::Local<v8::Script> script;
  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(kV8CacheOptionsDefault, source_code);
  EXPECT_FALSE(V8ScriptRunner::CompileScript(
                   scope.GetScriptState(), source_code, kSharableCrossOrigin,
                   compile_options, no_cache_reason, ReferrerScriptInfo())
                   .ToLocal(&script));
  EXPECT_TRUE(try_catch.HasCaught());
}

TEST_F(ScriptStreamingTest, CancellingStreaming) {
  // Test that the upper layers (PendingScript and up) can be ramped down
  // while streaming is ongoing, and ScriptStreamer handles it gracefully.
  V8TestingScope scope;
  ScriptStreamer::NotStreamingReason reason;
  ScriptStreamer::StartStreaming(GetPendingScript(), loading_task_runner_,
                                 &reason);
  GetPendingScript()->SetNotStreamingReasonForTest(reason);
  TestPendingScriptClient* client = new TestPendingScriptClient;
  GetPendingScript()->WatchForLoad(client);
  AppendData("function foo() {");

  // In general, we cannot control what the background thread is doing
  // (whether it's parsing or waiting for more data). In this test, we have
  // given it so little data that it's surely waiting for more.

  // Simulate cancelling the network load (e.g., because the user navigated
  // away).
  EXPECT_FALSE(client->Finished());
  GetPendingScript()->Dispose();
  pending_script_ = nullptr;  // This will destroy m_resource.

  // The V8 side will complete too. This should not crash. We don't receive
  // any results from the streaming and the client doesn't get notified.
  ProcessTasksUntilStreamingComplete();
  EXPECT_FALSE(client->Finished());
}

TEST_F(ScriptStreamingTest, DataAfterCancellingStreaming) {
  // Test that the upper layers (PendingScript and up) can be ramped down
  // before streaming is started, and ScriptStreamer handles it gracefully.
  V8TestingScope scope;
  ScriptStreamer::NotStreamingReason reason;
  ScriptStreamer::StartStreaming(GetPendingScript(), loading_task_runner_,
                                 &reason);
  GetPendingScript()->SetNotStreamingReasonForTest(reason);
  TestPendingScriptClient* client = new TestPendingScriptClient;
  GetPendingScript()->WatchForLoad(client);

  // In general, we cannot control what the background thread is doing
  // (whether it's parsing or waiting for more data). In this test, we have
  // given it so little data that it's surely waiting for more.

  EXPECT_FALSE(client->Finished());

  // Keep the resource alive
  Persistent<ScriptResource> resource = GetResource();

  // Simulate cancelling the network load (e.g., because the user navigated
  // away).
  GetPendingScript()->Dispose();
  pending_script_ = nullptr;  // This would destroy m_resource, but we are still
                              // holding on to it here.

  // Make sure the streaming starts.
  AppendPadding(resource);
  resource.Clear();

  // The V8 side will complete too. This should not crash. We don't receive
  // any results from the streaming and the client doesn't get notified.
  ProcessTasksUntilStreamingComplete();
  EXPECT_FALSE(client->Finished());
}

TEST_F(ScriptStreamingTest, SuppressingStreaming) {
  // If we notice during streaming that there is a code cache, streaming
  // is suppressed (V8 doesn't parse while the script is loading), and the
  // upper layer (ScriptResourceClient) should get a notification when the
  // script is loaded.
  V8TestingScope scope;
  ScriptStreamer::NotStreamingReason reason;
  ScriptStreamer::StartStreaming(GetPendingScript(), loading_task_runner_,
                                 &reason);
  GetPendingScript()->SetNotStreamingReasonForTest(reason);
  TestPendingScriptClient* client = new TestPendingScriptClient;
  GetPendingScript()->WatchForLoad(client);
  AppendData("function foo() {");
  AppendPadding();

  SingleCachedMetadataHandler* cache_handler = GetResource()->CacheHandler();
  EXPECT_TRUE(cache_handler);
  cache_handler->SetCachedMetadata(V8CodeCache::TagForCodeCache(cache_handler),
                                   "X", 1,
                                   CachedMetadataHandler::kCacheLocally);

  AppendPadding();
  Finish();
  ProcessTasksUntilStreamingComplete();
  EXPECT_TRUE(client->Finished());

  ScriptSourceCode source_code = GetScriptSourceCode();
  // ScriptSourceCode doesn't refer to the streamer, since we have suppressed
  // the streaming and resumed the non-streaming code path for script
  // compilation.
  EXPECT_FALSE(source_code.Streamer());
}

TEST_F(ScriptStreamingTest, EmptyScripts) {
  // Empty scripts should also be streamed properly, that is, the upper layer
  // (ScriptResourceClient) should be notified when an empty script has been
  // loaded.
  V8TestingScope scope;
  ScriptStreamer::NotStreamingReason reason;
  ScriptStreamer::StartStreaming(GetPendingScript(), loading_task_runner_,
                                 &reason);
  GetPendingScript()->SetNotStreamingReasonForTest(reason);
  TestPendingScriptClient* client = new TestPendingScriptClient;
  GetPendingScript()->WatchForLoad(client);

  // Finish the script without sending any data.
  Finish();
  // The finished notification should arrive immediately and not be cycled
  // through a background thread.
  EXPECT_TRUE(client->Finished());

  ScriptSourceCode source_code = GetScriptSourceCode();
  EXPECT_FALSE(source_code.Streamer());
}

TEST_F(ScriptStreamingTest, SmallScripts) {
  // Small scripts shouldn't be streamed.
  V8TestingScope scope;
  ScriptStreamer::SetSmallScriptThresholdForTesting(100);

  ScriptStreamer::NotStreamingReason reason;
  ScriptStreamer::StartStreaming(GetPendingScript(), loading_task_runner_,
                                 &reason);
  GetPendingScript()->SetNotStreamingReasonForTest(reason);
  TestPendingScriptClient* client = new TestPendingScriptClient;
  GetPendingScript()->WatchForLoad(client);

  AppendData("function foo() { }");

  Finish();

  // The finished notification should arrive immediately and not be cycled
  // through a background thread.
  EXPECT_TRUE(client->Finished());

  ScriptSourceCode source_code = GetScriptSourceCode();
  EXPECT_FALSE(source_code.Streamer());
}

TEST_F(ScriptStreamingTest, ScriptsWithSmallFirstChunk) {
  // If a script is long enough, if should be streamed, even if the first data
  // chunk is small.
  V8TestingScope scope;
  ScriptStreamer::SetSmallScriptThresholdForTesting(100);

  ScriptStreamer::NotStreamingReason reason;
  ScriptStreamer::StartStreaming(GetPendingScript(), loading_task_runner_,
                                 &reason);
  GetPendingScript()->SetNotStreamingReasonForTest(reason);
  TestPendingScriptClient* client = new TestPendingScriptClient;
  GetPendingScript()->WatchForLoad(client);

  // This is the first data chunk which is small.
  AppendData("function foo() { }");
  AppendPadding();
  AppendPadding();
  AppendPadding();

  Finish();

  ProcessTasksUntilStreamingComplete();
  EXPECT_TRUE(client->Finished());
  ScriptSourceCode source_code = GetScriptSourceCode();
  EXPECT_TRUE(source_code.Streamer());
  v8::TryCatch try_catch(scope.GetIsolate());
  v8::Local<v8::Script> script;
  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(kV8CacheOptionsDefault, source_code);
  EXPECT_TRUE(V8ScriptRunner::CompileScript(
                  scope.GetScriptState(), source_code, kSharableCrossOrigin,
                  compile_options, no_cache_reason, ReferrerScriptInfo())
                  .ToLocal(&script));
  EXPECT_FALSE(try_catch.HasCaught());
}

TEST_F(ScriptStreamingTest, EncodingChanges) {
  // It's possible that the encoding of the Resource changes after we start
  // loading it.
  V8TestingScope scope;
  GetResource()->SetEncodingForTest("windows-1252");

  ScriptStreamer::NotStreamingReason reason;
  ScriptStreamer::StartStreaming(GetPendingScript(), loading_task_runner_,
                                 &reason);
  GetPendingScript()->SetNotStreamingReasonForTest(reason);
  TestPendingScriptClient* client = new TestPendingScriptClient;
  GetPendingScript()->WatchForLoad(client);

  GetResource()->SetEncodingForTest("UTF-8");
  // \xec\x92\x81 are the raw bytes for \uc481.
  AppendData(
      "function foo() { var foob\xec\x92\x81r = 13; return foob\xec\x92\x81r; "
      "} foo();");

  Finish();

  ProcessTasksUntilStreamingComplete();
  EXPECT_TRUE(client->Finished());
  ScriptSourceCode source_code = GetScriptSourceCode();
  EXPECT_TRUE(source_code.Streamer());
  v8::TryCatch try_catch(scope.GetIsolate());
  v8::Local<v8::Script> script;
  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(kV8CacheOptionsDefault, source_code);
  EXPECT_TRUE(V8ScriptRunner::CompileScript(
                  scope.GetScriptState(), source_code, kSharableCrossOrigin,
                  compile_options, no_cache_reason, ReferrerScriptInfo())
                  .ToLocal(&script));
  EXPECT_FALSE(try_catch.HasCaught());
}

TEST_F(ScriptStreamingTest, EncodingFromBOM) {
  // Byte order marks should be removed before giving the data to V8. They
  // will also affect encoding detection.
  V8TestingScope scope;

  // This encoding is wrong on purpose.
  GetResource()->SetEncodingForTest("windows-1252");

  ScriptStreamer::NotStreamingReason reason;
  ScriptStreamer::StartStreaming(GetPendingScript(), loading_task_runner_,
                                 &reason);
  GetPendingScript()->SetNotStreamingReasonForTest(reason);
  TestPendingScriptClient* client = new TestPendingScriptClient;
  GetPendingScript()->WatchForLoad(client);

  // \xef\xbb\xbf is the UTF-8 byte order mark. \xec\x92\x81 are the raw bytes
  // for \uc481.
  AppendData(
      "\xef\xbb\xbf function foo() { var foob\xec\x92\x81r = 13; return "
      "foob\xec\x92\x81r; } foo();");

  Finish();
  ProcessTasksUntilStreamingComplete();
  EXPECT_TRUE(client->Finished());
  ScriptSourceCode source_code = GetScriptSourceCode();
  EXPECT_TRUE(source_code.Streamer());
  v8::TryCatch try_catch(scope.GetIsolate());
  v8::Local<v8::Script> script;
  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(kV8CacheOptionsDefault, source_code);
  EXPECT_TRUE(V8ScriptRunner::CompileScript(
                  scope.GetScriptState(), source_code, kSharableCrossOrigin,
                  compile_options, no_cache_reason, ReferrerScriptInfo())
                  .ToLocal(&script));
  EXPECT_FALSE(try_catch.HasCaught());
}

// A test for crbug.com/711703. Should not crash.
TEST_F(ScriptStreamingTest, GarbageCollectDuringStreaming) {
  V8TestingScope scope;
  ScriptStreamer::NotStreamingReason reason;
  ScriptStreamer::StartStreaming(GetPendingScript(), loading_task_runner_,
                                 &reason);
  GetPendingScript()->SetNotStreamingReasonForTest(reason);

  TestPendingScriptClient* client = new TestPendingScriptClient;
  GetPendingScript()->WatchForLoad(client);
  EXPECT_FALSE(client->Finished());

  pending_script_ = nullptr;
  ThreadState::Current()->CollectGarbage(
      BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
      BlinkGC::kEagerSweeping, BlinkGC::GCReason::kForcedGC);
}

}  // namespace

}  // namespace blink
