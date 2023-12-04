// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_local_compile_hints_consumer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_local_compile_hints_producer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/script/mock_script_element_base.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/cross_origin_attribute_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/testing/mock_context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class TestResourceClient final : public GarbageCollected<TestResourceClient>,
                                 public ResourceClient {
 public:
  explicit TestResourceClient(base::OnceClosure finish_closure)
      : finish_closure_(std::move(finish_closure)) {}

  bool Finished() const { return finished_; }

  bool ErrorOccurred() const { return error_occurred_; }

  void NotifyFinished(Resource* resource) override {
    finished_ = true;
    error_occurred_ = resource->ErrorOccurred();
    std::move(finish_closure_).Run();
  }

  // Name for debugging, e.g. shown in memory-infra.
  String DebugName() const override { return "TestResourceClient"; }

 private:
  bool finished_ = false;
  bool error_occurred_ = false;
  base::OnceClosure finish_closure_;
};

// TODO(leszeks): This class has a similar class in resource_loader_test.cc,
// the two should probably share the same class.
class NoopLoaderFactory final : public ResourceFetcher::LoaderFactory {
  std::unique_ptr<URLLoader> CreateURLLoader(
      const ResourceRequest& request,
      const ResourceLoaderOptions& options,
      scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
      BackForwardCacheLoaderHelper*) override {
    return std::make_unique<NoopURLLoader>(std::move(freezable_task_runner));
  }
  CodeCacheHost* GetCodeCacheHost() override { return nullptr; }

  class NoopURLLoader final : public URLLoader {
   public:
    explicit NoopURLLoader(
        scoped_refptr<base::SingleThreadTaskRunner> task_runner)
        : task_runner_(std::move(task_runner)) {}
    ~NoopURLLoader() override = default;
    void LoadSynchronously(
        std::unique_ptr<network::ResourceRequest> request,
        scoped_refptr<const SecurityOrigin> top_frame_origin,
        bool download_to_blob,
        bool no_mime_sniffing,
        base::TimeDelta timeout_interval,
        URLLoaderClient*,
        WebURLResponse&,
        absl::optional<WebURLError>&,
        scoped_refptr<SharedBuffer>&,
        int64_t& encoded_data_length,
        uint64_t& encoded_body_length,
        scoped_refptr<BlobDataHandle>& downloaded_blob,
        std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
            resource_load_info_notifier_wrapper) override {
      NOTREACHED();
    }
    void LoadAsynchronously(
        std::unique_ptr<network::ResourceRequest> request,
        scoped_refptr<const SecurityOrigin> top_frame_origin,
        bool no_mime_sniffing,
        std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
            resource_load_info_notifier_wrapper,
        CodeCacheHost* code_cache_host,
        URLLoaderClient*) override {}
    void Freeze(LoaderFreezeMode) override {}
    void DidChangePriority(WebURLRequest::Priority, int) override {
      NOTREACHED();
    }
    scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForBodyLoader()
        override {
      return task_runner_;
    }
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  };
};

}  // namespace

class ScriptStreamingTest : public testing::Test {
 public:
  ScriptStreamingTest() : url_("http://www.streaming-test.com/") {
    auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
    FetchContext* context = MakeGarbageCollected<MockFetchContext>();
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        scheduler::GetSingleThreadTaskRunnerForTesting();
    auto* fetcher = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
        properties->MakeDetachable(), context, task_runner, task_runner,
        MakeGarbageCollected<NoopLoaderFactory>(),
        MakeGarbageCollected<MockContextLifecycleNotifier>(),
        nullptr /* back_forward_cache_loader_helper */));

    EXPECT_EQ(mojo::CreateDataPipe(nullptr, producer_handle_, consumer_handle_),
              MOJO_RESULT_OK);

    ResourceRequest request(url_);
    request.SetRequestContext(mojom::blink::RequestContextType::SCRIPT);

    resource_client_ =
        MakeGarbageCollected<TestResourceClient>(run_loop_.QuitClosure());
    FetchParameters params = FetchParameters::CreateForTest(std::move(request));
    constexpr v8_compile_hints::V8CrowdsourcedCompileHintsProducer*
        kNoCompileHintsProducer = nullptr;
    constexpr v8_compile_hints::V8CrowdsourcedCompileHintsConsumer*
        kNoCompileHintsConsumer = nullptr;
    resource_ = ScriptResource::Fetch(
        params, fetcher, resource_client_, ScriptResource::kAllowStreaming,
        kNoCompileHintsProducer, kNoCompileHintsConsumer);
    resource_->AddClient(resource_client_, task_runner.get());

    ResourceResponse response(url_);
    response.SetHttpStatusCode(200);
    resource_->SetResponse(response);

    resource_->Loader()->DidReceiveResponse(WrappedResourceResponse(response),
                                            std::move(consumer_handle_),
                                            /*cached_metadata=*/absl::nullopt);
  }

  ClassicScript* CreateClassicScript() const {
    return ClassicScript::CreateFromResource(resource_, ScriptFetchOptions());
  }

 protected:
  void AppendData(const char* data) {
    uint32_t data_len = base::checked_cast<uint32_t>(strlen(data));
    MojoResult result = producer_handle_->WriteData(
        data, &data_len, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
    EXPECT_EQ(result, MOJO_RESULT_OK);

    // In case the mojo datapipe is being read on the main thread, we need to
    // spin the event loop to allow the watcher to post its "data received"
    // callback back to the main thread.
    //
    // Note that this uses a nested RunLoop -- this is to prevent it from being
    // affected by the QuitClosure of the outer RunLoop.
    base::RunLoop().RunUntilIdle();

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
    producer_handle_.reset();
    resource_->SetStatus(ResourceStatus::kCached);
  }

  void Cancel() { resource_->Loader()->Cancel(); }

  void RunUntilResourceLoaded() { run_loop_.Run(); }

  test::TaskEnvironment task_environment_;
  KURL url_;

  base::RunLoop run_loop_;
  Persistent<TestResourceClient> resource_client_;
  Persistent<ScriptResource> resource_;
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;
};

TEST_F(ScriptStreamingTest, CompilingStreamedScript) {
  // Test that we can successfully compile a streamed script.
  V8TestingScope scope;

  AppendData("function foo() {");
  AppendPadding();
  AppendData("return 5; }");
  AppendPadding();
  AppendData("foo();");
  EXPECT_FALSE(resource_client_->Finished());
  Finish();

  // Process tasks on the main thread until the resource has notified that it
  // has finished loading.
  RunUntilResourceLoaded();
  EXPECT_TRUE(resource_client_->Finished());
  ClassicScript* classic_script = CreateClassicScript();
  EXPECT_TRUE(classic_script->Streamer());
  v8::TryCatch try_catch(scope.GetIsolate());
  v8::Local<v8::Script> script;
  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(mojom::blink::V8CacheOptions::kDefault,
                                     *classic_script);
  EXPECT_TRUE(V8ScriptRunner::CompileScript(
                  scope.GetScriptState(), *classic_script,
                  classic_script->CreateScriptOrigin(scope.GetIsolate()),
                  compile_options, no_cache_reason)
                  .ToLocal(&script));
  EXPECT_FALSE(try_catch.HasCaught());
}

TEST_F(ScriptStreamingTest, CompilingStreamedScriptWithParseError) {
  // Test that scripts with parse errors are handled properly. In those cases,
  // V8 stops reading the network stream: make sure we handle it gracefully.
  V8TestingScope scope;
  AppendData("function foo() {");
  AppendData("this is the part which will be a parse error");
  // V8 won't realize the parse error until it actually starts parsing the
  // script, and this happens only when its buffer is filled.
  AppendPadding();

  EXPECT_FALSE(resource_client_->Finished());
  Finish();

  // Process tasks on the main thread until the resource has notified that it
  // has finished loading.
  RunUntilResourceLoaded();
  EXPECT_TRUE(resource_client_->Finished());
  ClassicScript* classic_script = CreateClassicScript();
  EXPECT_TRUE(classic_script->Streamer());
  v8::TryCatch try_catch(scope.GetIsolate());
  v8::Local<v8::Script> script;
  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(mojom::blink::V8CacheOptions::kDefault,
                                     *classic_script);
  EXPECT_FALSE(V8ScriptRunner::CompileScript(
                   scope.GetScriptState(), *classic_script,
                   classic_script->CreateScriptOrigin(scope.GetIsolate()),
                   compile_options, no_cache_reason)
                   .ToLocal(&script));
  EXPECT_TRUE(try_catch.HasCaught());
}

TEST_F(ScriptStreamingTest, CancellingStreaming) {
  // Test that the upper layers (PendingScript and up) can be ramped down
  // while streaming is ongoing, and ScriptStreamer handles it gracefully.
  V8TestingScope scope;
  AppendData("function foo() {");

  // In general, we cannot control what the background thread is doing
  // (whether it's parsing or waiting for more data). In this test, we have
  // given it so little data that it's surely waiting for more.

  // Simulate cancelling the network load (e.g., because the user navigated
  // away).
  EXPECT_FALSE(resource_client_->Finished());
  Cancel();

  // The V8 side will complete too. This should not crash. We don't receive
  // any results from the streaming and the resource client should finish with
  // an error.
  RunUntilResourceLoaded();
  EXPECT_TRUE(resource_client_->Finished());
  EXPECT_TRUE(resource_client_->ErrorOccurred());
  EXPECT_FALSE(resource_->HasStreamer());
}

TEST_F(ScriptStreamingTest, DataAfterCancelling) {
  // Test that the upper layers (PendingScript and up) can be ramped down
  // before streaming is started, and ScriptStreamer handles it gracefully.
  V8TestingScope scope;

  // In general, we cannot control what the background thread is doing
  // (whether it's parsing or waiting for more data). In this test, we have
  // given it so little data that it's surely waiting for more.

  EXPECT_FALSE(resource_client_->Finished());

  // Simulate cancelling the network load (e.g., because the user navigated
  // away).
  Cancel();

  // Append data to the streamer's data pipe.
  AppendData("function foo() {");
  AppendPadding();

  // The V8 side will complete too. This should not crash. We don't receive
  // any results from the streaming and the resource client should finish with
  // an error.
  RunUntilResourceLoaded();
  EXPECT_TRUE(resource_client_->Finished());
  EXPECT_TRUE(resource_client_->ErrorOccurred());
  EXPECT_FALSE(resource_->HasStreamer());
}

TEST_F(ScriptStreamingTest, SuppressingStreaming) {
  // If we notice before streaming that there is a code cache, streaming
  // is suppressed (V8 doesn't parse while the script is loading), and the
  // upper layer (ScriptResourceClient) should get a notification when the
  // script is loaded.
  V8TestingScope scope;

  CachedMetadataHandler* cache_handler = resource_->CacheHandler();
  EXPECT_TRUE(cache_handler);
  cache_handler->DisableSendToPlatformForTesting();
  // CodeCacheHost can be nullptr since we disabled sending data to
  // GeneratedCodeCacheHost for testing.
  cache_handler->SetCachedMetadata(/*code_cache_host*/ nullptr,
                                   V8CodeCache::TagForCodeCache(cache_handler),
                                   reinterpret_cast<const uint8_t*>("X"), 1);

  AppendData("function foo() {");
  AppendPadding();
  Finish();
  RunUntilResourceLoaded();
  EXPECT_TRUE(resource_client_->Finished());

  ClassicScript* classic_script = CreateClassicScript();
  // ClassicScript doesn't refer to the streamer, since we have suppressed
  // the streaming and resumed the non-streaming code path for script
  // compilation.
  EXPECT_FALSE(classic_script->Streamer());
}

TEST_F(ScriptStreamingTest, ConsumeLocalCompileHints) {
  // If we notice before streaming that there is a compile hints cache, we use
  // it for eager compilation.
  base::test::ScopedFeatureList flag_on(features::kLocalCompileHints);
  V8TestingScope scope;

  CachedMetadataHandler* cache_handler = resource_->CacheHandler();
  EXPECT_TRUE(cache_handler);
  cache_handler->DisableSendToPlatformForTesting();
  // CodeCacheHost can be nullptr since we disabled sending data to
  // GeneratedCodeCacheHost for testing.

  // Create fake compile hints (what the real compile hints are is internal to
  // v8).
  std::vector<int> compile_hints = {200, 230};
  uint64_t timestamp = V8CodeCache::GetTimestamp();

  std::unique_ptr<v8::ScriptCompiler::CachedData> cached_data(
      v8_compile_hints::V8LocalCompileHintsProducer::
          CreateCompileHintsCachedDataForScript(compile_hints, timestamp));

  cache_handler->SetCachedMetadata(
      /*code_cache_host*/ nullptr,
      V8CodeCache::TagForCompileHints(cache_handler), cached_data->data,
      cached_data->length);

  AppendData("/*this doesn't matter*/");
  Finish();
  RunUntilResourceLoaded();
  EXPECT_TRUE(resource_client_->Finished());

  ResourceScriptStreamer* resource_script_streamer =
      std::get<0>(ResourceScriptStreamer::TakeFrom(
          resource_, mojom::blink::ScriptType::kClassic));
  EXPECT_TRUE(resource_script_streamer);

  v8_compile_hints::V8LocalCompileHintsConsumer* local_compile_hints_consumer =
      resource_script_streamer->GetV8LocalCompileHintsConsumer();
  EXPECT_TRUE(local_compile_hints_consumer);

  EXPECT_TRUE(local_compile_hints_consumer->GetCompileHint(200));
  EXPECT_FALSE(local_compile_hints_consumer->GetCompileHint(210));
  EXPECT_TRUE(local_compile_hints_consumer->GetCompileHint(230));
  EXPECT_FALSE(local_compile_hints_consumer->GetCompileHint(240));
}

TEST_F(ScriptStreamingTest, EmptyScripts) {
  // Empty scripts should also be streamed properly, that is, the upper layer
  // (ScriptResourceClient) should be notified when an empty script has been
  // loaded.
  V8TestingScope scope;

  // Finish the script without sending any data.
  Finish();
  RunUntilResourceLoaded();
  EXPECT_TRUE(resource_client_->Finished());

  ClassicScript* classic_script = CreateClassicScript();
  EXPECT_FALSE(classic_script->Streamer());
}

TEST_F(ScriptStreamingTest, SmallScripts) {
  // Small scripts shouldn't be streamed.
  V8TestingScope scope;

  // This is the data chunk is small enough to not start streaming (it is less
  // than 4 bytes, so smaller than a UTF-8 BOM).
  AppendData("{}");
  EXPECT_TRUE(resource_->HasStreamer());
  EXPECT_FALSE(resource_->HasRunningStreamer());

  Finish();
  RunUntilResourceLoaded();
  EXPECT_TRUE(resource_client_->Finished());

  ClassicScript* classic_script = CreateClassicScript();
  EXPECT_FALSE(classic_script->Streamer());
}

TEST_F(ScriptStreamingTest, ScriptsWithSmallFirstChunk) {
  // If a script is long enough, if should be streamed, even if the first data
  // chunk is small.
  V8TestingScope scope;

  // This is the first data chunk which is small enough to not start streaming
  // (it is less than 4 bytes, so smaller than a UTF-8 BOM).
  AppendData("{}");
  EXPECT_TRUE(resource_->HasStreamer());
  EXPECT_FALSE(resource_->HasRunningStreamer());

  // Now add more padding so that streaming does start.
  AppendPadding();
  AppendPadding();
  AppendPadding();
  EXPECT_TRUE(resource_->HasRunningStreamer());

  Finish();
  RunUntilResourceLoaded();
  EXPECT_TRUE(resource_client_->Finished());
  ClassicScript* classic_script = CreateClassicScript();
  EXPECT_TRUE(classic_script->Streamer());
  v8::TryCatch try_catch(scope.GetIsolate());
  v8::Local<v8::Script> script;
  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(mojom::blink::V8CacheOptions::kDefault,
                                     *classic_script);
  EXPECT_TRUE(V8ScriptRunner::CompileScript(
                  scope.GetScriptState(), *classic_script,
                  classic_script->CreateScriptOrigin(scope.GetIsolate()),
                  compile_options, no_cache_reason)
                  .ToLocal(&script));
  EXPECT_FALSE(try_catch.HasCaught());
}

TEST_F(ScriptStreamingTest, EncodingChanges) {
  // It's possible that the encoding of the Resource changes after we start
  // loading it.
  V8TestingScope scope;
  resource_->SetEncodingForTest("windows-1252");

  resource_->SetEncodingForTest("UTF-8");
  // \xec\x92\x81 are the raw bytes for \uc481.
  AppendData(
      "function foo() { var foob\xec\x92\x81r = 13; return foob\xec\x92\x81r; "
      "} foo();");

  Finish();

  RunUntilResourceLoaded();
  EXPECT_TRUE(resource_client_->Finished());
  ClassicScript* classic_script = CreateClassicScript();
  EXPECT_TRUE(classic_script->Streamer());
  v8::TryCatch try_catch(scope.GetIsolate());
  v8::Local<v8::Script> script;
  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(mojom::blink::V8CacheOptions::kDefault,
                                     *classic_script);
  EXPECT_TRUE(V8ScriptRunner::CompileScript(
                  scope.GetScriptState(), *classic_script,
                  classic_script->CreateScriptOrigin(scope.GetIsolate()),
                  compile_options, no_cache_reason)
                  .ToLocal(&script));
  EXPECT_FALSE(try_catch.HasCaught());
}

TEST_F(ScriptStreamingTest, EncodingFromBOM) {
  // Byte order marks should be removed before giving the data to V8. They
  // will also affect encoding detection.
  V8TestingScope scope;

  // This encoding is wrong on purpose.
  resource_->SetEncodingForTest("windows-1252");

  // \xef\xbb\xbf is the UTF-8 byte order mark. \xec\x92\x81 are the raw bytes
  // for \uc481.
  AppendData(
      "\xef\xbb\xbf function foo() { var foob\xec\x92\x81r = 13; return "
      "foob\xec\x92\x81r; } foo();");

  Finish();
  RunUntilResourceLoaded();
  EXPECT_TRUE(resource_client_->Finished());
  ClassicScript* classic_script = CreateClassicScript();
  EXPECT_TRUE(classic_script->Streamer());
  v8::TryCatch try_catch(scope.GetIsolate());
  v8::Local<v8::Script> script;
  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(mojom::blink::V8CacheOptions::kDefault,
                                     *classic_script);
  EXPECT_TRUE(V8ScriptRunner::CompileScript(
                  scope.GetScriptState(), *classic_script,
                  classic_script->CreateScriptOrigin(scope.GetIsolate()),
                  compile_options, no_cache_reason)
                  .ToLocal(&script));
  EXPECT_FALSE(try_catch.HasCaught());
}

// A test for crbug.com/711703. Should not crash.
TEST_F(ScriptStreamingTest, GarbageCollectDuringStreaming) {
  V8TestingScope scope;

  EXPECT_FALSE(resource_client_->Finished());

  resource_ = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
}

TEST_F(ScriptStreamingTest, ResourceSetRevalidatingRequest) {
  V8TestingScope scope;

  // Kick the streaming off.
  AppendData("function foo() {");
  AppendPadding();
  AppendData("}");
  Finish();
  RunUntilResourceLoaded();

  // Should be done streaming by now.
  EXPECT_TRUE(resource_->HasFinishedStreamer());

  ResourceRequest request(resource_->Url());
  resource_->SetRevalidatingRequest(request);

  // Now there shouldn't be a streamer at all, and the reason should be
  // "kRevalidate".
  EXPECT_FALSE(resource_->HasStreamer());
  EXPECT_EQ(resource_->NoStreamerReason(),
            ScriptStreamer::NotStreamingReason::kRevalidate);
}

class InlineScriptStreamingTest
    : public ScriptStreamingTest,
      public ::testing::WithParamInterface<
          std::pair<bool /* 16 bit source */,
                    v8::ScriptCompiler::CompileOptions>> {};

TEST_P(InlineScriptStreamingTest, InlineScript) {
  // Test that we can successfully compile an inline script.
  V8TestingScope scope;

  String source = "function foo() {return 5;} foo();";
  if (GetParam().first)
    source.Ensure16Bit();
  auto streamer = base::MakeRefCounted<BackgroundInlineScriptStreamer>(
      source, GetParam().second);
  worker_pool::PostTask(
      FROM_HERE, {},
      CrossThreadBindOnce(&BackgroundInlineScriptStreamer::Run, streamer));

  ClassicScript* classic_script = ClassicScript::Create(
      source, KURL(), KURL(), ScriptFetchOptions(),
      ScriptSourceLocationType::kUnknown, SanitizeScriptErrors::kSanitize,
      nullptr, TextPosition::MinimumPosition(),
      ScriptStreamer::NotStreamingReason::kInvalid,
      InlineScriptStreamer::From(streamer));

  DummyPageHolder holder;
  ScriptEvaluationResult result = classic_script->RunScriptAndReturnValue(
      holder.GetFrame().DomWindow(),
      ExecuteScriptPolicy::kExecuteScriptWhenScriptsDisabled);
  EXPECT_EQ(result.GetResultType(),
            ScriptEvaluationResult::ResultType::kSuccess);
  EXPECT_EQ(
      5, result.GetSuccessValue()->Int32Value(scope.GetContext()).FromJust());
}

TEST_F(ScriptStreamingTest, ProduceLocalCompileHintsForStreamedScript) {
  // Test that we can produce local compile hints when a script is streamed.
  base::test::ScopedFeatureList flag_on(features::kLocalCompileHints);
  V8TestingScope scope;

  AppendData("function foo() { return 5; }");
  AppendData("foo();");
  EXPECT_FALSE(resource_client_->Finished());
  Finish();

  // Process tasks on the main thread until the resource has notified that it
  // has finished loading.
  RunUntilResourceLoaded();
  EXPECT_TRUE(resource_client_->Finished());
  ClassicScript* classic_script = CreateClassicScript();
  EXPECT_TRUE(classic_script->Streamer());
  v8::TryCatch try_catch(scope.GetIsolate());
  v8::Local<v8::Script> script;
  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(mojom::blink::V8CacheOptions::kDefault,
                                     *classic_script);
  EXPECT_TRUE(V8ScriptRunner::CompileScript(
                  scope.GetScriptState(), *classic_script,
                  classic_script->CreateScriptOrigin(scope.GetIsolate()),
                  compile_options, no_cache_reason)
                  .ToLocal(&script));
  EXPECT_FALSE(try_catch.HasCaught());

  v8::Local<v8::Value> return_value;
  EXPECT_TRUE(script->Run(scope.GetContext()).ToLocal(&return_value));

  // Expect that we got a compile hint for the function which was run. Don't
  // assert what it is (that's internal to V8).
  std::vector<int> compile_hints = script->GetProducedCompileHints();
  EXPECT_EQ(1UL, compile_hints.size());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    InlineScriptStreamingTest,
    testing::ValuesIn(
        {std::make_pair(true,
                        v8::ScriptCompiler::CompileOptions::kNoCompileOptions),
         std::make_pair(false,
                        v8::ScriptCompiler::CompileOptions::kNoCompileOptions),
         std::make_pair(true,
                        v8::ScriptCompiler::CompileOptions::kEagerCompile),
         std::make_pair(false,
                        v8::ScriptCompiler::CompileOptions::kEagerCompile)}));

}  // namespace blink
