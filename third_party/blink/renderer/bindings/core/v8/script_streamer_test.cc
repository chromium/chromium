// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"

#include <memory>
#include <utility>

#include "base/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/script/classic_pending_script.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/script/mock_script_element_base.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/cross_origin_attribute_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class TestResourceClient final : public GarbageCollected<TestResourceClient>,
                                 public ResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(TestResourceClient);

 public:
  TestResourceClient() : finished_(false) {}
  bool Finished() const { return finished_; }

  void DataReceived(Resource*,
                    const char* /* data */,
                    size_t /* length */) override {}
  void NotifyFinished(Resource*) override { finished_ = true; }

  // Name for debugging, e.g. shown in memory-infra.
  String DebugName() const override { return "TestResourceClient"; }

 private:
  bool finished_;
};

// TODO(leszeks): This class has a similar class in resource_loader_test.cc,
// the two should probably share the same class.
class NoopLoaderFactory final : public ResourceFetcher::LoaderFactory {
  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const ResourceRequest& request,
      const ResourceLoaderOptions& options,
      scoped_refptr<base::SingleThreadTaskRunner>) override {
    return std::make_unique<NoopWebURLLoader>();
  }
  std::unique_ptr<CodeCacheLoader> CreateCodeCacheLoader() override {
    return Platform::Current()->CreateCodeCacheLoader();
  }

  class NoopWebURLLoader final : public WebURLLoader {
   public:
    ~NoopWebURLLoader() override = default;
    void LoadSynchronously(const WebURLRequest&,
                           WebURLLoaderClient*,
                           WebURLResponse&,
                           base::Optional<WebURLError>&,
                           WebData&,
                           int64_t& encoded_data_length,
                           int64_t& encoded_body_length,
                           WebBlobInfo& downloaded_blob) override {
      NOTREACHED();
    }
    void LoadAsynchronously(const WebURLRequest&,
                            WebURLLoaderClient*) override {}
    void SetDefersLoading(bool) override {}
    void DidChangePriority(WebURLRequest::Priority, int) override {
      NOTREACHED();
    }
    scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override {
      return base::MakeRefCounted<scheduler::FakeTaskRunner>();
    }
  };
};

class ScriptStreamingTest : public testing::Test {
 public:
  ScriptStreamingTest()
      : url_("http://www.streaming-test.com/"),
        loading_task_runner_(platform_->test_task_runner()) {
    auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
    FetchContext* context = MakeGarbageCollected<MockFetchContext>();
    auto* fetcher = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
        properties->MakeDetachable(), context, loading_task_runner_,
        MakeGarbageCollected<NoopLoaderFactory>()));

    ResourceRequest request(url_);
    request.SetRequestContext(mojom::RequestContextType::SCRIPT);

    resource_client_ = MakeGarbageCollected<TestResourceClient>();
    FetchParameters params(request);
    resource_ = ScriptResource::Fetch(params, fetcher, resource_client_,
                                      ScriptResource::kAllowStreaming);
    resource_->AddClient(resource_client_, loading_task_runner_.get());

    ScriptStreamer::SetSmallScriptThresholdForTesting(0);

    ResourceResponse response(url_);
    response.SetHttpStatusCode(200);
    resource_->SetResponse(response);

    resource_->Loader()->DidReceiveResponse(WrappedResourceResponse(response));
    resource_->Loader()->DidStartLoadingResponseBody(
        std::move(data_pipe_.consumer_handle));
  }

  ScriptSourceCode GetScriptSourceCode() const {
    ScriptStreamer* streamer = resource_->TakeStreamer();
    if (streamer) {
      if (streamer->StreamingSuppressed()) {
        return ScriptSourceCode(nullptr, resource_,
                                streamer->StreamingSuppressedReason());
      }
      return ScriptSourceCode(streamer, resource_, ScriptStreamer::kInvalid);
    }
    return ScriptSourceCode(nullptr, resource_, resource_->NoStreamerReason());
  }

  Settings* GetSettings() const {
    return &dummy_page_holder_->GetPage().GetSettings();
  }

 protected:
  void AppendData(const char* data) {
    uint32_t data_len = strlen(data);
    MojoResult result = data_pipe_.producer_handle->WriteData(
        data, &data_len, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
    EXPECT_EQ(result, MOJO_RESULT_OK);

    // Yield control to the background thread, so that V8 gets a chance to
    // process the data before the main thread adds more. Note that we
    // cannot fully control in what kind of chunks the data is passed to V8
    // (if V8 is not requesting more data between two appendData calls, it
    // will get both chunks together).
    test::YieldCurrentThread();
  }

  void AppendPadding() {
    for (int i = 0; i < 10; ++i) {
      AppendData(
          " /* this is padding to make the script long enough, so "
          "that V8's buffer gets filled and it starts processing "
          "the data */ ");
    }
  }

  void Finish() {
    resource_->Loader()->DidFinishLoading(base::TimeTicks(), 0, 0, 0, false);
    data_pipe_.producer_handle.reset();
    resource_->SetStatus(ResourceStatus::kCached);
  }

  void ProcessTasksUntilStreamingComplete() { platform_->RunUntilIdle(); }

  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;

  KURL url_;
  scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner_;

  Persistent<TestResourceClient> resource_client_;
  Persistent<ScriptResource> resource_;
  mojo::DataPipe data_pipe_;

  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

// TODO(crbug.com/939054): Tests are disabled due to flakiness caused by being
// currently unable to block and wait for the script streaming thread.
TEST_F(ScriptStreamingTest, DISABLED_CompilingStreamedScript) {
  return;

  // Test that we can successfully compile a streamed script.
  V8TestingScope scope;
  resource_->StartStreaming(loading_task_runner_);
  resource_->SetClientIsWaitingForFinished();

  AppendData("function foo() {");
  AppendPadding();
  AppendData("return 5; }");
  AppendPadding();
  AppendData("foo();");
  EXPECT_FALSE(resource_client_->Finished());
  Finish();

  // Process tasks on the main thread until the streaming background thread
  // has completed its tasks.
  ProcessTasksUntilStreamingComplete();
  EXPECT_TRUE(resource_client_->Finished());
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
                  scope.GetScriptState(), source_code,
                  SanitizeScriptErrors::kDoNotSanitize, compile_options,
                  no_cache_reason, ReferrerScriptInfo())
                  .ToLocal(&script));
  EXPECT_FALSE(try_catch.HasCaught());
}

// TODO(crbug.com/939054): Tests are disabled due to flakiness caused by being
// currently unable to block and wait for the script streaming thread.
TEST_F(ScriptStreamingTest, DISABLED_CompilingStreamedScriptWithParseError) {
  // Test that scripts with parse errors are handled properly. In those cases,
  // V8 stops reading the network stream: make sure we handle it gracefully.
  V8TestingScope scope;
  resource_->StartStreaming(loading_task_runner_);
  resource_->SetClientIsWaitingForFinished();
  AppendData("function foo() {");
  AppendData("this is the part which will be a parse error");
  // V8 won't realize the parse error until it actually starts parsing the
  // script, and this happens only when its buffer is filled.
  AppendPadding();

  EXPECT_FALSE(resource_client_->Finished());
  Finish();

  // Process tasks on the main thread until the streaming background thread
  // has completed its tasks.
  ProcessTasksUntilStreamingComplete();
  EXPECT_TRUE(resource_client_->Finished());
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
                   scope.GetScriptState(), source_code,
                   SanitizeScriptErrors::kDoNotSanitize, compile_options,
                   no_cache_reason, ReferrerScriptInfo())
                   .ToLocal(&script));
  EXPECT_TRUE(try_catch.HasCaught());
}

// TODO(crbug.com/939054): Tests are disabled due to flakiness caused by being
// currently unable to block and wait for the script streaming thread.
TEST_F(ScriptStreamingTest, DISABLED_CancellingStreaming) {
  // Test that the upper layers (PendingScript and up) can be ramped down
  // while streaming is ongoing, and ScriptStreamer handles it gracefully.
  V8TestingScope scope;
  resource_->StartStreaming(loading_task_runner_);
  resource_->SetClientIsWaitingForFinished();
  AppendData("function foo() {");

  // In general, we cannot control what the background thread is doing
  // (whether it's parsing or waiting for more data). In this test, we have
  // given it so little data that it's surely waiting for more.

  // Simulate cancelling the network load (e.g., because the user navigated
  // away).
  EXPECT_FALSE(resource_client_->Finished());
  resource_ = nullptr;

  // The V8 side will complete too. This should not crash. We don't receive
  // any results from the streaming and the client doesn't get notified.
  ProcessTasksUntilStreamingComplete();
  EXPECT_FALSE(resource_client_->Finished());
}

// TODO(crbug.com/939054): Tests are disabled due to flakiness caused by being
// currently unable to block and wait for the script streaming thread.
TEST_F(ScriptStreamingTest, DISABLED_DataAfterDisposingPendingScript) {
  // Test that the upper layers (PendingScript and up) can be ramped down
  // before streaming is started, and ScriptStreamer handles it gracefully.
  V8TestingScope scope;
  resource_->StartStreaming(loading_task_runner_);
  resource_->SetClientIsWaitingForFinished();

  // In general, we cannot control what the background thread is doing
  // (whether it's parsing or waiting for more data). In this test, we have
  // given it so little data that it's surely waiting for more.

  EXPECT_FALSE(resource_client_->Finished());

  // Keep the resource alive
  Persistent<ScriptResource> resource = resource_;

  // Simulate cancelling the network load (e.g., because the user navigated
  // away).
  resource_ = nullptr;

  // Make sure the streaming starts.
  AppendData("function foo() {");
  AppendPadding();
  resource.Clear();

  // The V8 side will complete too. This should not crash. We don't receive
  // any results from the streaming and the client doesn't get notified.
  ProcessTasksUntilStreamingComplete();
  EXPECT_FALSE(resource_client_->Finished());
}

// TODO(crbug.com/939054): Tests are disabled due to flakiness caused by being
// currently unable to block and wait for the script streaming thread.
TEST_F(ScriptStreamingTest, DISABLED_SuppressingStreaming) {
  // If we notice before streaming that there is a code cache, streaming
  // is suppressed (V8 doesn't parse while the script is loading), and the
  // upper layer (ScriptResourceClient) should get a notification when the
  // script is loaded.
  V8TestingScope scope;
  resource_->StartStreaming(loading_task_runner_);
  resource_->SetClientIsWaitingForFinished();

  SingleCachedMetadataHandler* cache_handler = resource_->CacheHandler();
  EXPECT_TRUE(cache_handler);
  cache_handler->SetCachedMetadata(V8CodeCache::TagForCodeCache(cache_handler),
                                   reinterpret_cast<const uint8_t*>("X"), 1,
                                   CachedMetadataHandler::kCacheLocally);

  AppendData("function foo() {");
  AppendPadding();
  Finish();
  ProcessTasksUntilStreamingComplete();
  EXPECT_TRUE(resource_client_->Finished());

  ScriptSourceCode source_code = GetScriptSourceCode();
  // ScriptSourceCode doesn't refer to the streamer, since we have suppressed
  // the streaming and resumed the non-streaming code path for script
  // compilation.
  EXPECT_FALSE(source_code.Streamer());
}

// TODO(crbug.com/939054): Tests are disabled due to flakiness caused by being
// currently unable to block and wait for the script streaming thread.
TEST_F(ScriptStreamingTest, DISABLED_EmptyScripts) {
  // Empty scripts should also be streamed properly, that is, the upper layer
  // (ScriptResourceClient) should be notified when an empty script has been
  // loaded.
  V8TestingScope scope;
  resource_->StartStreaming(loading_task_runner_);
  resource_->SetClientIsWaitingForFinished();

  // Finish the script without sending any data.
  Finish();
  ProcessTasksUntilStreamingComplete();
  EXPECT_TRUE(resource_client_->Finished());

  ScriptSourceCode source_code = GetScriptSourceCode();
  EXPECT_FALSE(source_code.Streamer());
}

// TODO(crbug.com/939054): Tests are disabled due to flakiness caused by being
// currently unable to block and wait for the script streaming thread.
TEST_F(ScriptStreamingTest, DISABLED_SmallScripts) {
  // Small scripts shouldn't be streamed.
  V8TestingScope scope;
  ScriptStreamer::SetSmallScriptThresholdForTesting(100);

  resource_->StartStreaming(loading_task_runner_);
  resource_->SetClientIsWaitingForFinished();

  AppendData("function foo() { }");

  Finish();
  ProcessTasksUntilStreamingComplete();
  EXPECT_TRUE(resource_client_->Finished());

  ScriptSourceCode source_code = GetScriptSourceCode();
  EXPECT_FALSE(source_code.Streamer());
}

// TODO(crbug.com/939054): Tests are disabled due to flakiness caused by being
// currently unable to block and wait for the script streaming thread.
TEST_F(ScriptStreamingTest, DISABLED_ScriptsWithSmallFirstChunk) {
  // If a script is long enough, if should be streamed, even if the first data
  // chunk is small.
  V8TestingScope scope;
  ScriptStreamer::SetSmallScriptThresholdForTesting(100);

  resource_->StartStreaming(loading_task_runner_);
  resource_->SetClientIsWaitingForFinished();

  // This is the first data chunk which is small.
  AppendData("function foo() { }");
  AppendPadding();
  AppendPadding();
  AppendPadding();

  Finish();

  ProcessTasksUntilStreamingComplete();
  EXPECT_TRUE(resource_client_->Finished());
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
                  scope.GetScriptState(), source_code,
                  SanitizeScriptErrors::kDoNotSanitize, compile_options,
                  no_cache_reason, ReferrerScriptInfo())
                  .ToLocal(&script));
  EXPECT_FALSE(try_catch.HasCaught());
}

// TODO(crbug.com/939054): Tests are disabled due to flakiness caused by being
// currently unable to block and wait for the script streaming thread.
TEST_F(ScriptStreamingTest, DISABLED_EncodingChanges) {
  // It's possible that the encoding of the Resource changes after we start
  // loading it.
  V8TestingScope scope;
  resource_->SetEncodingForTest("windows-1252");

  resource_->StartStreaming(loading_task_runner_);
  resource_->SetClientIsWaitingForFinished();

  resource_->SetEncodingForTest("UTF-8");
  // \xec\x92\x81 are the raw bytes for \uc481.
  AppendData(
      "function foo() { var foob\xec\x92\x81r = 13; return foob\xec\x92\x81r; "
      "} foo();");

  Finish();

  ProcessTasksUntilStreamingComplete();
  EXPECT_TRUE(resource_client_->Finished());
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
                  scope.GetScriptState(), source_code,
                  SanitizeScriptErrors::kDoNotSanitize, compile_options,
                  no_cache_reason, ReferrerScriptInfo())
                  .ToLocal(&script));
  EXPECT_FALSE(try_catch.HasCaught());
}

// TODO(crbug.com/939054): Tests are disabled due to flakiness caused by being
// currently unable to block and wait for the script streaming thread.
TEST_F(ScriptStreamingTest, DISABLED_EncodingFromBOM) {
  // Byte order marks should be removed before giving the data to V8. They
  // will also affect encoding detection.
  V8TestingScope scope;

  // This encoding is wrong on purpose.
  resource_->SetEncodingForTest("windows-1252");

  resource_->StartStreaming(loading_task_runner_);
  resource_->SetClientIsWaitingForFinished();

  // \xef\xbb\xbf is the UTF-8 byte order mark. \xec\x92\x81 are the raw bytes
  // for \uc481.
  AppendData(
      "\xef\xbb\xbf function foo() { var foob\xec\x92\x81r = 13; return "
      "foob\xec\x92\x81r; } foo();");

  Finish();
  ProcessTasksUntilStreamingComplete();
  EXPECT_TRUE(resource_client_->Finished());
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
                  scope.GetScriptState(), source_code,
                  SanitizeScriptErrors::kDoNotSanitize, compile_options,
                  no_cache_reason, ReferrerScriptInfo())
                  .ToLocal(&script));
  EXPECT_FALSE(try_catch.HasCaught());
}

// TODO(crbug.com/939054): Tests are disabled due to flakiness caused by being
// currently unable to block and wait for the script streaming thread.
// A test for crbug.com/711703. Should not crash.
TEST_F(ScriptStreamingTest, DISABLED_GarbageCollectDuringStreaming) {
  V8TestingScope scope;
  resource_->StartStreaming(loading_task_runner_);

  resource_->SetClientIsWaitingForFinished();
  EXPECT_FALSE(resource_client_->Finished());

  resource_ = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting(
      BlinkGC::kNoHeapPointersOnStack);
}

// TODO(crbug.com/939054): Tests are disabled due to flakiness caused by being
// currently unable to block and wait for the script streaming thread.
TEST_F(ScriptStreamingTest, DISABLED_ResourceSetRevalidatingRequest) {
  V8TestingScope scope;
  resource_->StartStreaming(loading_task_runner_);

  resource_->SetClientIsWaitingForFinished();

  // Kick the streaming off.
  AppendData("function foo() {");
  AppendPadding();
  AppendData("}");
  Finish();
  ProcessTasksUntilStreamingComplete();

  // Second start streaming should fail.
  resource_->StartStreaming(loading_task_runner_);
  EXPECT_FALSE(resource_->HasRunningStreamer());

  ResourceRequest request(resource_->Url());
  resource_->SetRevalidatingRequest(request);

  // The next streaming should still fail, but the reason should be
  // "kRevalidate".
  resource_->StartStreaming(loading_task_runner_);
  EXPECT_FALSE(resource_->HasRunningStreamer());
  EXPECT_EQ(resource_->NoStreamerReason(), ScriptStreamer::kRevalidate);
}

}  // namespace

}  // namespace blink
