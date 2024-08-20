// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints_consumer.h"
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
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/cross_origin_attribute_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
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
      const network::ResourceRequest& request,
      const ResourceLoaderOptions& options,
      scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
      BackForwardCacheLoaderHelper*,
      const std::optional<base::UnguessableToken>&
          service_worker_race_network_request_token,
      bool is_from_origin_dirty_style_sheet) override {
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
        std::optional<WebURLError>&,
        scoped_refptr<SharedBuffer>&,
        int64_t& encoded_data_length,
        uint64_t& encoded_body_length,
        scoped_refptr<BlobDataHandle>& downloaded_blob,
        std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
            resource_load_info_notifier_wrapper) override {
      NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
    }
    scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForBodyLoader()
        override {
      return task_runner_;
    }
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  };
};

void AppendDataToDataPipe(std::string_view data,
                          mojo::ScopedDataPipeProducerHandle& producer_handle) {
  MojoResult result = producer_handle->WriteAllData(base::as_byte_span(data));
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
  // (if V8 is not requesting more data between two AppendDataToDataPipecalls,
  // it will get both chunks together).
  test::YieldCurrentThread();
}

const uint32_t kDataPipeSize = 1024;

}  // namespace

class ScriptStreamingTest : public testing::Test {
 public:
  ScriptStreamingTest()
      : url_(String("http://streaming-test.example.com/foo" +
                    base::NumberToString(url_counter_++))) {}

  void Init(v8::Isolate* isolate, bool use_response_http_scheme = true) {
    auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
    FetchContext* context = MakeGarbageCollected<MockFetchContext>();
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        scheduler::GetSingleThreadTaskRunnerForTesting();
    auto* fetcher = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
        properties->MakeDetachable(), context, task_runner, task_runner,
        MakeGarbageCollected<NoopLoaderFactory>(),
        MakeGarbageCollected<MockContextLifecycleNotifier>(),
        nullptr /* back_forward_cache_loader_helper */));

    EXPECT_EQ(
        mojo::CreateDataPipe(kDataPipeSize, producer_handle_, consumer_handle_),
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
    constexpr bool kNoV8CompileHintsMagicCommentRuntimeEnabled = false;
    resource_ = ScriptResource::Fetch(
        params, fetcher, resource_client_, isolate,
        ScriptResource::kAllowStreaming, kNoCompileHintsProducer,
        kNoCompileHintsConsumer, kNoV8CompileHintsMagicCommentRuntimeEnabled);
    resource_->AddClient(resource_client_, task_runner.get());

    ResourceResponse response(url_);
    response.SetHttpStatusCode(200);

    if (!use_response_http_scheme) {
      response.SetCurrentRequestUrl(KURL("file:///something"));
    }
    resource_->SetResponse(response);

    resource_->Loader()->DidReceiveResponse(WrappedResourceResponse(response),
                                            std::move(consumer_handle_),
                                            /*cached_metadata=*/std::nullopt);
  }

  ClassicScript* CreateClassicScript() const {
    return ClassicScript::CreateFromResource(resource_, ScriptFetchOptions());
  }

 protected:
  void AppendData(std::string_view data) {
    AppendDataToDataPipe(data, producer_handle_);
  }

  void Finish() {
    resource_->Loader()->DidFinishLoading(base::TimeTicks(), 0, 0, 0);
    producer_handle_.reset();
    resource_->SetStatus(ResourceStatus::kCached);
  }

  void Cancel() { resource_->Loader()->Cancel(); }

  void RunUntilResourceLoaded() { run_loop_.Run(); }

  static int url_counter_;

  test::TaskEnvironment task_environment_;
  KURL url_;

  base::RunLoop run_loop_;
  Persistent<TestResourceClient> resource_client_;
  Persistent<ScriptResource> resource_;
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;
};

int ScriptStreamingTest::url_counter_ = 0;

TEST_F(ScriptStreamingTest, CompilingStreamedScript) {
  // Test that we can successfully compile a streamed script.
  V8TestingScope scope;
  Init(scope.GetIsolate());

  AppendData("function foo() {");
  AppendData("return 5; }");
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
  Init(scope.GetIsolate());

  AppendData("function foo() {");
  AppendData("this is the part which will be a parse error");
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
  Init(scope.GetIsolate());

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
  Init(scope.GetIsolate());

  // In general, we cannot control what the background thread is doing
  // (whether it's parsing or waiting for more data). In this test, we have
  // given it so little data that it's surely waiting for more.

  EXPECT_FALSE(resource_client_->Finished());

  // Simulate cancelling the network load (e.g., because the user navigated
  // away).
  Cancel();

  // Append data to the streamer's data pipe.
  AppendData("function foo() {");

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
  Init(scope.GetIsolate());

  CachedMetadataHandler* cache_handler = resource_->CacheHandler();
  EXPECT_TRUE(cache_handler);
  cache_handler->DisableSendToPlatformForTesting();
  // CodeCacheHost can be nullptr since we disabled sending data to
  // GeneratedCodeCacheHost for testing.
  cache_handler->SetCachedMetadata(/*code_cache_host*/ nullptr,
                                   V8CodeCache::TagForCodeCache(cache_handler),
                                   reinterpret_cast<const uint8_t*>("X"), 1);

  AppendData("function foo() {");
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

  // Disable features::kProduceCompileHints2 forcefully, because local compile
  // hints are not used when producing crowdsourced compile hints.
  base::test::ScopedFeatureList features;
  features.InitWithFeatureStates({{features::kLocalCompileHints, true},
                                  {features::kProduceCompileHints2, false}});

  V8TestingScope scope;
  Init(scope.GetIsolate());

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

  // Checks for debugging failures in this test.
  EXPECT_TRUE(V8CodeCache::HasCompileHints(
      cache_handler, CachedMetadataHandler::kAllowUnchecked));
  EXPECT_TRUE(V8CodeCache::HasHotTimestamp(cache_handler));

  AppendData("/*this doesn't matter*/");
  Finish();
  RunUntilResourceLoaded();
  EXPECT_TRUE(resource_client_->Finished());

  ScriptStreamer* script_streamer = std::get<0>(
      ScriptStreamer::TakeFrom(resource_, mojom::blink::ScriptType::kClassic));
  ResourceScriptStreamer* resource_script_streamer =
      reinterpret_cast<ResourceScriptStreamer*>(script_streamer);
  EXPECT_TRUE(resource_script_streamer);

  v8_compile_hints::V8LocalCompileHintsConsumer* local_compile_hints_consumer =
      resource_script_streamer->GetV8LocalCompileHintsConsumerForTest();
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
  Init(scope.GetIsolate());

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
  Init(scope.GetIsolate());

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
  Init(scope.GetIsolate());

  // This is the first data chunk which is small enough to not start streaming
  // (it is less than 4 bytes, so smaller than a UTF-8 BOM).
  AppendData("{}");
  EXPECT_TRUE(resource_->HasStreamer());
  EXPECT_FALSE(resource_->HasRunningStreamer());

  // Now add more data so that streaming does start.
  AppendData("/*------*/");
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
  Init(scope.GetIsolate());

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
  Init(scope.GetIsolate());

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
  Init(scope.GetIsolate());

  EXPECT_FALSE(resource_client_->Finished());

  resource_ = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
}

TEST_F(ScriptStreamingTest, ResourceSetRevalidatingRequest) {
  V8TestingScope scope;
  Init(scope.GetIsolate());

  // Kick the streaming off.
  AppendData("function foo() {");
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
  Init(scope.GetIsolate());

  String source = "function foo() {return 5;} foo();";
  if (GetParam().first)
    source.Ensure16Bit();
  auto streamer = base::MakeRefCounted<BackgroundInlineScriptStreamer>(
      scope.GetIsolate(), source, GetParam().second);
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
  Init(scope.GetIsolate());

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

TEST_F(ScriptStreamingTest, NullCacheHandler) {
  V8TestingScope scope;
  // Use setting the responses URL to something else than HTTP(S) to trigger the
  // "streaming but no cache handler" corner case.
  Init(scope.GetIsolate(), /*use_response_http_scheme=*/false);
  EXPECT_FALSE(resource_->CacheHandler());

  AppendData("/*this doesn't matter*/");
  Finish();
  RunUntilResourceLoaded();
  EXPECT_TRUE(resource_client_->Finished());

  ScriptStreamer* script_streamer = std::get<0>(
      ScriptStreamer::TakeFrom(resource_, mojom::blink::ScriptType::kClassic));
  ResourceScriptStreamer* resource_script_streamer =
      reinterpret_cast<ResourceScriptStreamer*>(script_streamer);
  EXPECT_TRUE(resource_script_streamer);
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

namespace {

// This is small enough to not start streaming (it is less　than 4 bytes, so
// smaller than a UTF-8 BOM).
const char kTooSmallScript[] = "//";
// This script is large enough to start streaming (it is larger than 4 bytes, so
// larger than a UTF-8 BOM).
const char kLargeEnoughScript[] = "function foo() { return 5; }";

// \xef\xbb\xbf is the UTF-8 byte order mark. \xec\x92\x81 are the raw bytes
// for \uc481.
const char kScriptWithBOM[] =
    "\xef\xbb\xbf function foo() { var foob\xec\x92\x81r = 13; return "
    "foob\xec\x92\x81r; } foo();";

class DummyLoaderFactory final : public ResourceFetcher::LoaderFactory {
 public:
  DummyLoaderFactory() = default;
  ~DummyLoaderFactory() override = default;

  // ResourceFetcher::LoaderFactory implementation:
  std::unique_ptr<URLLoader> CreateURLLoader(
      const network::ResourceRequest& request,
      const ResourceLoaderOptions& options,
      scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
      BackForwardCacheLoaderHelper*,
      const std::optional<base::UnguessableToken>&
          service_worker_race_network_request_token,
      bool is_from_origin_dirty_style_sheet) override {
    return std::make_unique<DummyURLLoader>(this,
                                            std::move(freezable_task_runner));
  }
  CodeCacheHost* GetCodeCacheHost() override { return nullptr; }

  bool load_started() const { return load_started_; }
  std::unique_ptr<BackgroundResponseProcessorFactory>
  TakeBackgroundResponseProcessorFactory() {
    return std::move(background_response_processor_factory_);
  }

 private:
  class DummyURLLoader final : public URLLoader {
   public:
    explicit DummyURLLoader(
        DummyLoaderFactory* factory,
        scoped_refptr<base::SingleThreadTaskRunner> task_runner)
        : factory_(factory), task_runner_(std::move(task_runner)) {}
    ~DummyURLLoader() override = default;

    // URLLoader implementation:
    void LoadSynchronously(
        std::unique_ptr<network::ResourceRequest> request,
        scoped_refptr<const SecurityOrigin> top_frame_origin,
        bool download_to_blob,
        bool no_mime_sniffing,
        base::TimeDelta timeout_interval,
        URLLoaderClient*,
        WebURLResponse&,
        std::optional<WebURLError>&,
        scoped_refptr<SharedBuffer>&,
        int64_t& encoded_data_length,
        uint64_t& encoded_body_length,
        scoped_refptr<BlobDataHandle>& downloaded_blob,
        std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
            resource_load_info_notifier_wrapper) override {
      NOTREACHED_IN_MIGRATION();
    }
    void LoadAsynchronously(
        std::unique_ptr<network::ResourceRequest> request,
        scoped_refptr<const SecurityOrigin> top_frame_origin,
        bool no_mime_sniffing,
        std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
            resource_load_info_notifier_wrapper,
        CodeCacheHost* code_cache_host,
        URLLoaderClient* client) override {
      factory_->load_started_ = true;
    }
    void Freeze(LoaderFreezeMode) override {}
    void DidChangePriority(WebURLRequest::Priority, int) override {
      NOTREACHED_IN_MIGRATION();
    }
    bool CanHandleResponseOnBackground() override { return true; }
    void SetBackgroundResponseProcessorFactory(
        std::unique_ptr<BackgroundResponseProcessorFactory>
            background_response_processor_factory) override {
      factory_->background_response_processor_factory_ =
          std::move(background_response_processor_factory);
    }
    scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForBodyLoader()
        override {
      return task_runner_;
    }
    Persistent<DummyLoaderFactory> factory_;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  };

  bool load_started_ = false;
  std::unique_ptr<BackgroundResponseProcessorFactory>
      background_response_processor_factory_;
};

class DummyBackgroundResponseProcessorClient
    : public BackgroundResponseProcessor::Client {
 public:
  DummyBackgroundResponseProcessorClient()
      : main_thread_task_runner_(
            scheduler::GetSingleThreadTaskRunnerForTesting()) {}

  ~DummyBackgroundResponseProcessorClient() override = default;

  void DidFinishBackgroundResponseProcessor(
      network::mojom::URLResponseHeadPtr head,
      BackgroundResponseProcessor::BodyVariant body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {
    head_ = std::move(head);
    body_ = std::move(body);
    cached_metadata_ = std::move(cached_metadata);
    run_loop_.Quit();
  }
  void PostTaskToMainThread(CrossThreadOnceClosure task) override {
    PostCrossThreadTask(*main_thread_task_runner_, FROM_HERE, std::move(task));
  }

  void WaitUntilFinished() { run_loop_.Run(); }

  void CheckResultOfFinishCallback(
      base::span<const char> expected_body,
      std::optional<base::span<const uint8_t>> expected_cached_metadata) {
    EXPECT_TRUE(head_);
    if (absl::holds_alternative<SegmentedBuffer>(body_)) {
      const SegmentedBuffer& raw_body = absl::get<SegmentedBuffer>(body_);
      const Vector<char> concatenated_body = raw_body.CopyAs<Vector<char>>();
      EXPECT_THAT(concatenated_body, testing::ElementsAreArray(expected_body));
    } else {
      CHECK(absl::holds_alternative<mojo::ScopedDataPipeConsumerHandle>(body_));
      mojo::ScopedDataPipeConsumerHandle& handle =
          absl::get<mojo::ScopedDataPipeConsumerHandle>(body_);
      std::string text;
      EXPECT_TRUE(mojo::BlockingCopyToString(std::move(handle), &text));
      EXPECT_THAT(text, testing::ElementsAreArray(expected_body));
    }
    ASSERT_EQ(expected_cached_metadata, cached_metadata_);
    if (expected_cached_metadata) {
      EXPECT_THAT(*cached_metadata_,
                  testing::ElementsAreArray(*expected_cached_metadata));
    }
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  base::RunLoop run_loop_;
  network::mojom::URLResponseHeadPtr head_;
  BackgroundResponseProcessor::BodyVariant body_;
  std::optional<mojo_base::BigBuffer> cached_metadata_;
};

class DummyCachedMetadataSender : public CachedMetadataSender {
 public:
  DummyCachedMetadataSender() = default;
  void Send(CodeCacheHost*, base::span<const uint8_t>) override {}
  bool IsServedFromCacheStorage() override { return false; }
};

mojo_base::BigBuffer CreateDummyCodeCacheData() {
  ScriptCachedMetadataHandler* cache_handler =
      MakeGarbageCollected<ScriptCachedMetadataHandler>(
          UTF8Encoding(), std::make_unique<DummyCachedMetadataSender>());
  uint32_t data_type_id = V8CodeCache::TagForCodeCache(cache_handler);
  cache_handler->SetCachedMetadata(
      /*code_cache_host=*/nullptr, data_type_id,
      reinterpret_cast<const uint8_t*>("X"), 1);
  scoped_refptr<CachedMetadata> cached_metadata =
      cache_handler->GetCachedMetadata(data_type_id);
  mojo_base::BigBuffer cached_metadata_buffer =
      mojo_base::BigBuffer(cached_metadata->SerializedData());
  return cached_metadata_buffer;
}

mojo_base::BigBuffer CreateDummyTimeStampData() {
  ScriptCachedMetadataHandler* cache_handler =
      MakeGarbageCollected<ScriptCachedMetadataHandler>(
          UTF8Encoding(), std::make_unique<DummyCachedMetadataSender>());
  uint32_t data_type_id = V8CodeCache::TagForTimeStamp(cache_handler);
  uint64_t now_ms = 11111;
  cache_handler->SetCachedMetadata(
      /*code_cache_host=*/nullptr, data_type_id,
      reinterpret_cast<uint8_t*>(&now_ms), sizeof(now_ms));
  scoped_refptr<CachedMetadata> cached_metadata =
      cache_handler->GetCachedMetadata(data_type_id);
  mojo_base::BigBuffer cached_metadata_buffer =
      mojo_base::BigBuffer(cached_metadata->SerializedData());
  return cached_metadata_buffer;
}

network::mojom::URLResponseHeadPtr CreateURLResponseHead(
    const std::string& content_type = "text/javascript") {
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(base::StrCat(
          {"HTTP/1.1 200 OK\n", "Content-Type: ", content_type, "\n\n"})));
  return head;
}

}  // namespace

class BackgroundResourceScriptStreamerTest : public testing::Test {
 public:
  explicit BackgroundResourceScriptStreamerTest(
      bool enable_background_code_cache_decode_start = false)
      : url_(String("http://streaming-test.example.com/foo" +
                    base::NumberToString(url_counter_++))) {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kBackgroundResourceFetch,
          {{"background-script-response-processor", "true"},
           {"background-code-cache-decoder-start",
            enable_background_code_cache_decode_start ? "true" : "false"}}}},
        {});
  }
  ~BackgroundResourceScriptStreamerTest() override = default;

  void TearDown() override {
    RunInBackgroundThred(base::BindLambdaForTesting(
        [&]() { background_response_processor_.reset(); }));
  }

 protected:
  void Init(v8::Isolate* isolate,
            bool is_module_script = false,
            std::optional<WTF::TextEncoding> charset = std::nullopt,
            v8_compile_hints::V8CrowdsourcedCompileHintsConsumer*
                v8_compile_hints_consumer = nullptr) {
    auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
    FetchContext* context = MakeGarbageCollected<MockFetchContext>();
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner =
        scheduler::GetSingleThreadTaskRunnerForTesting();
    DummyLoaderFactory* dummy_loader_factory =
        MakeGarbageCollected<DummyLoaderFactory>();
    auto* fetcher = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
        properties->MakeDetachable(), context, main_thread_task_runner,
        main_thread_task_runner, dummy_loader_factory,
        MakeGarbageCollected<MockContextLifecycleNotifier>(),
        nullptr /* back_forward_cache_loader_helper */));

    EXPECT_EQ(
        mojo::CreateDataPipe(kDataPipeSize, producer_handle_, consumer_handle_),
        MOJO_RESULT_OK);

    ResourceRequest request(url_);
    request.SetRequestContext(mojom::blink::RequestContextType::SCRIPT);

    resource_client_ =
        MakeGarbageCollected<TestResourceClient>(run_loop_.QuitClosure());
    FetchParameters params = FetchParameters::CreateForTest(std::move(request));
    if (is_module_script) {
      params.SetModuleScript();
    }
    if (charset) {
      params.SetCharset(*charset);
    }
    constexpr v8_compile_hints::V8CrowdsourcedCompileHintsProducer*
        kNoCompileHintsProducer = nullptr;
    constexpr bool kNoV8CompileHintsMagicCommentRuntimeEnabled = false;
    resource_ = ScriptResource::Fetch(
        params, fetcher, resource_client_, isolate,
        ScriptResource::kAllowStreaming, kNoCompileHintsProducer,
        v8_compile_hints_consumer, kNoV8CompileHintsMagicCommentRuntimeEnabled);
    resource_->AddClient(resource_client_, main_thread_task_runner.get());

    CHECK(dummy_loader_factory->load_started());
    background_resource_fetch_task_runner_ =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::TaskPriority::USER_BLOCKING});

    RunInBackgroundThred(base::BindLambdaForTesting([&]() {
      std::unique_ptr<BackgroundResponseProcessorFactory> factory =
          dummy_loader_factory->TakeBackgroundResponseProcessorFactory();
      background_response_processor_ = std::move(*factory).Create();
    }));
  }

  ClassicScript* CreateClassicScript() const {
    return ClassicScript::CreateFromResource(resource_, ScriptFetchOptions());
  }

 protected:
  void AppendData(std::string_view data) {
    AppendDataToDataPipe(data, producer_handle_);
  }

  void Finish() {
    ResourceResponse response(url_);
    response.SetHttpStatusCode(200);
    resource_->Loader()->DidReceiveResponse(WrappedResourceResponse(response),
                                            std::move(consumer_handle_),
                                            /*cached_metadata=*/std::nullopt);
    producer_handle_.reset();
    resource_->Loader()->DidFinishLoading(base::TimeTicks(), 0, 0, 0);
  }

  void Cancel() { resource_->Loader()->Cancel(); }

  void RunUntilResourceLoaded() { run_loop_.Run(); }

  void RunInBackgroundThred(base::OnceClosure closuer) {
    base::RunLoop loop;
    background_resource_fetch_task_runner_->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          std::move(closuer).Run();
          loop.Quit();
        }));
    loop.Run();
  }

  void CheckNotStreamingReason(
      ScriptStreamer::NotStreamingReason expected_not_streamed_reason,
      mojom::blink::ScriptType script_type =
          mojom::blink::ScriptType::kClassic) {
    ScriptStreamer* streamer;
    ScriptStreamer::NotStreamingReason not_streamed_reason;
    std::tie(streamer, not_streamed_reason) =
        ScriptStreamer::TakeFrom(resource_, script_type);
    EXPECT_EQ(expected_not_streamed_reason, not_streamed_reason);
    EXPECT_EQ(nullptr, streamer);
  }

  void CheckScriptStreamer(mojom::blink::ScriptType script_type =
                               mojom::blink::ScriptType::kClassic) {
    ScriptStreamer* streamer;
    ScriptStreamer::NotStreamingReason not_streamed_reason;
    std::tie(streamer, not_streamed_reason) =
        ScriptStreamer::TakeFrom(resource_, script_type);
    EXPECT_EQ(ScriptStreamer::NotStreamingReason::kInvalid,
              not_streamed_reason);
    EXPECT_NE(nullptr, streamer);
  }

  static int url_counter_;

  test::TaskEnvironment task_environment_;
  KURL url_;

  base::RunLoop run_loop_;
  Persistent<TestResourceClient> resource_client_;
  Persistent<ScriptResource> resource_;
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;
  std::unique_ptr<BackgroundResponseProcessor> background_response_processor_;
  DummyBackgroundResponseProcessorClient background_response_processor_client_;

  scoped_refptr<base::SequencedTaskRunner>
      background_resource_fetch_task_runner_;
  base::test::ScopedFeatureList feature_list_;
};
int BackgroundResourceScriptStreamerTest::url_counter_ = 0;

TEST_F(BackgroundResourceScriptStreamerTest, UnsupportedModuleMimeType) {
  V8TestingScope scope;
  Init(scope.GetIsolate(), /*is_module_script=*/true);
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    // "text/plain" is not a valid mime type for module scripts.
    network::mojom::URLResponseHeadPtr head =
        CreateURLResponseHead("text/plain");
    std::optional<mojo_base::BigBuffer> cached_metadata;
    EXPECT_FALSE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
    EXPECT_TRUE(head);
    EXPECT_TRUE(consumer_handle_);
    EXPECT_FALSE(cached_metadata);
  }));
  Finish();
  RunUntilResourceLoaded();
  CheckNotStreamingReason(
      ScriptStreamer::NotStreamingReason::kNonJavascriptModuleBackground,
      mojom::blink::ScriptType::kModule);
}

TEST_F(BackgroundResourceScriptStreamerTest, HasCodeCache) {
  V8TestingScope scope;
  Init(scope.GetIsolate());
  mojo_base::BigBuffer code_cache_data = CreateDummyCodeCacheData();
  const std::vector<uint8_t> code_cache_data_copy(
      code_cache_data.data(), code_cache_data.data() + code_cache_data.size());
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    network::mojom::URLResponseHeadPtr head = CreateURLResponseHead();
    // Set charset to make the code cache valid.
    head->charset = "utf-8";
    // Set a dummy code cache data.
    std::optional<mojo_base::BigBuffer> cached_metadata =
        std::move(code_cache_data);
    EXPECT_FALSE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
    EXPECT_TRUE(head);
    EXPECT_TRUE(consumer_handle_);
    ASSERT_TRUE(cached_metadata);
    EXPECT_THAT(*cached_metadata,
                testing::ElementsAreArray(code_cache_data_copy));
  }));
  Finish();
  RunUntilResourceLoaded();
  // When there is a code cache, we should not stream the script.
  CheckNotStreamingReason(
      ScriptStreamer::NotStreamingReason::kHasCodeCacheBackground);
}

class BackgroundResourceScriptStreamerCodeCacheDecodeStartTest
    : public BackgroundResourceScriptStreamerTest {
 public:
  BackgroundResourceScriptStreamerCodeCacheDecodeStartTest()
      : BackgroundResourceScriptStreamerTest(
            /*enable_background_code_cache_decode_start=*/true) {}
  ~BackgroundResourceScriptStreamerCodeCacheDecodeStartTest() override =
      default;
};

TEST_F(BackgroundResourceScriptStreamerCodeCacheDecodeStartTest, HasCodeCache) {
  V8TestingScope scope;
  Init(scope.GetIsolate());
  mojo_base::BigBuffer code_cache_data = CreateDummyCodeCacheData();
  const std::vector<uint8_t> code_cache_data_copy(
      code_cache_data.data(), code_cache_data.data() + code_cache_data.size());
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    network::mojom::URLResponseHeadPtr head = CreateURLResponseHead();
    // Set charset to make the code cache valid.
    head->charset = "utf-8";
    // Set a dummy code cache data.
    std::optional<mojo_base::BigBuffer> cached_metadata =
        std::move(code_cache_data);
    EXPECT_TRUE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
    EXPECT_FALSE(head);
    EXPECT_FALSE(consumer_handle_);
    ASSERT_TRUE(cached_metadata);
    EXPECT_EQ(cached_metadata->size(), 0u);
  }));
  AppendData(kLargeEnoughScript);
  producer_handle_.reset();
  background_response_processor_client_.WaitUntilFinished();
  // Checking that the code cache data is passed to the finish callback.
  background_response_processor_client_.CheckResultOfFinishCallback(
      /*expected_body=*/base::make_span(kLargeEnoughScript,
                                        sizeof(kLargeEnoughScript) - 1),
      /*expected_cached_metadata=*/code_cache_data_copy);
  Finish();
  RunUntilResourceLoaded();
  // When there is a code cache, we should not stream the script.
  CheckNotStreamingReason(
      ScriptStreamer::NotStreamingReason::kHasCodeCacheBackground);
}

TEST_F(BackgroundResourceScriptStreamerTest, HasTimeStampData) {
  V8TestingScope scope;
  Init(scope.GetIsolate());
  mojo_base::BigBuffer time_stamp_data = CreateDummyTimeStampData();
  const std::vector<uint8_t> time_stamp_data_copy(
      time_stamp_data.data(), time_stamp_data.data() + time_stamp_data.size());
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    network::mojom::URLResponseHeadPtr head = CreateURLResponseHead();
    // Set a dummy time stamp data.
    std::optional<mojo_base::BigBuffer> cached_metadata =
        std::move(time_stamp_data);
    EXPECT_TRUE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
    EXPECT_FALSE(head);
    EXPECT_FALSE(consumer_handle_);
    ASSERT_TRUE(cached_metadata);
    EXPECT_EQ(cached_metadata->storage_type(),
              mojo_base::BigBuffer::StorageType::kInvalidBuffer);
  }));
  AppendData(kLargeEnoughScript);
  producer_handle_.reset();
  background_response_processor_client_.WaitUntilFinished();
  // Checking that the dummy time stamp data is passed to the finish callback.
  background_response_processor_client_.CheckResultOfFinishCallback(
      /*expected_body=*/base::make_span(kLargeEnoughScript,
                                        sizeof(kLargeEnoughScript) - 1),
      /*expected_cached_metadata=*/time_stamp_data_copy);
  Finish();
  RunUntilResourceLoaded();
  // ScriptStreamer must have been created.
  CheckScriptStreamer();
}

TEST_F(BackgroundResourceScriptStreamerTest, InvalidCachedMetadata) {
  uint8_t kInvalidCachedMetadata[] = {0x00, 0x00};
  V8TestingScope scope;
  Init(scope.GetIsolate());
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    network::mojom::URLResponseHeadPtr head = CreateURLResponseHead();
    // Set an invalid cached metadata.
    std::optional<mojo_base::BigBuffer> cached_metadata =
        mojo_base::BigBuffer(base::make_span(kInvalidCachedMetadata));
    EXPECT_TRUE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
    EXPECT_FALSE(head);
    EXPECT_FALSE(consumer_handle_);
    ASSERT_TRUE(cached_metadata);
    EXPECT_EQ(cached_metadata->storage_type(),
              mojo_base::BigBuffer::StorageType::kInvalidBuffer);
  }));
  AppendData(kLargeEnoughScript);
  producer_handle_.reset();
  background_response_processor_client_.WaitUntilFinished();
  // Checking that the dummy metadata is passed to the finish callback.
  background_response_processor_client_.CheckResultOfFinishCallback(
      /*expected_body=*/base::make_span(kLargeEnoughScript,
                                        sizeof(kLargeEnoughScript) - 1),
      /*expected_cached_metadata=*/kInvalidCachedMetadata);
  Finish();
  RunUntilResourceLoaded();
  // ScriptStreamer must have been created.
  CheckScriptStreamer();
}

TEST_F(BackgroundResourceScriptStreamerTest, SmallScript) {
  V8TestingScope scope;
  Init(scope.GetIsolate());
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    network::mojom::URLResponseHeadPtr head = CreateURLResponseHead();
    std::optional<mojo_base::BigBuffer> cached_metadata;
    EXPECT_TRUE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
    EXPECT_FALSE(head);
    EXPECT_FALSE(consumer_handle_);
    EXPECT_FALSE(cached_metadata);
  }));
  // Append small data and close the data pipe not to trigger streaming.
  AppendData(kTooSmallScript);
  producer_handle_.reset();
  background_response_processor_client_.WaitUntilFinished();
  background_response_processor_client_.CheckResultOfFinishCallback(
      /*expected_body=*/base::make_span(kTooSmallScript,
                                        sizeof(kTooSmallScript) - 1),
      /*expected_cached_metadata=*/std::nullopt);
  Finish();
  RunUntilResourceLoaded();
  // When the script is too small, we should not stream the script.
  CheckNotStreamingReason(
      ScriptStreamer::NotStreamingReason::kScriptTooSmallBackground);
}

TEST_F(BackgroundResourceScriptStreamerTest, SmallScriptInFirstChunk) {
  V8TestingScope scope;
  Init(scope.GetIsolate());
  // Append the data chunk to the producer handle here, so the
  // MaybeStartProcessingResponse() can synchronously read the data chunk in the
  // data pipe.
  AppendData(kTooSmallScript);
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    network::mojom::URLResponseHeadPtr head = CreateURLResponseHead();
    std::optional<mojo_base::BigBuffer> cached_metadata;
    EXPECT_TRUE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
    EXPECT_FALSE(head);
    EXPECT_FALSE(consumer_handle_);
    EXPECT_FALSE(cached_metadata);
  }));
  producer_handle_.reset();
  background_response_processor_client_.WaitUntilFinished();
  background_response_processor_client_.CheckResultOfFinishCallback(
      /*expected_body=*/base::make_span(kTooSmallScript,
                                        sizeof(kTooSmallScript) - 1),
      /*expected_cached_metadata=*/std::nullopt);
  Finish();
  RunUntilResourceLoaded();
  // When the script is too small, we should not stream the script.
  CheckNotStreamingReason(
      ScriptStreamer::NotStreamingReason::kScriptTooSmallBackground);
}

TEST_F(BackgroundResourceScriptStreamerTest, EmptyScript) {
  V8TestingScope scope;
  Init(scope.GetIsolate());
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    network::mojom::URLResponseHeadPtr head = CreateURLResponseHead();
    std::optional<mojo_base::BigBuffer> cached_metadata;
    EXPECT_TRUE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
    EXPECT_FALSE(head);
    EXPECT_FALSE(consumer_handle_);
    EXPECT_FALSE(cached_metadata);
  }));
  // Close the data pipe without any data.
  producer_handle_.reset();
  background_response_processor_client_.WaitUntilFinished();
  background_response_processor_client_.CheckResultOfFinishCallback(
      /*expected_body=*/{},
      /*expected_cached_metadata=*/std::nullopt);
  Finish();
  RunUntilResourceLoaded();
  // When the script is too small (empty), we should not stream the script.
  CheckNotStreamingReason(
      ScriptStreamer::NotStreamingReason::kScriptTooSmallBackground);
}

TEST_F(BackgroundResourceScriptStreamerTest, EmptyScriptSyncCheckable) {
  V8TestingScope scope;
  Init(scope.GetIsolate());
  // Close the data pipe here, so the MaybeStartProcessingResponse() can
  // synchronously know that the script is empty.
  producer_handle_.reset();
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    network::mojom::URLResponseHeadPtr head = CreateURLResponseHead();
    std::optional<mojo_base::BigBuffer> cached_metadata;
    // MaybeStartProcessingResponse() can synchronously know that the script is
    // empty. So it returns false.
    EXPECT_FALSE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
    EXPECT_TRUE(head);
    EXPECT_TRUE(consumer_handle_);
    EXPECT_FALSE(cached_metadata);
  }));
  Finish();
  RunUntilResourceLoaded();
  // When the script is too small, we should not stream the script.
  CheckNotStreamingReason(
      ScriptStreamer::NotStreamingReason::kScriptTooSmallBackground);
}

TEST_F(BackgroundResourceScriptStreamerTest, EnoughData) {
  V8TestingScope scope;
  Init(scope.GetIsolate());
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    network::mojom::URLResponseHeadPtr head = CreateURLResponseHead();
    std::optional<mojo_base::BigBuffer> cached_metadata;
    EXPECT_TRUE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
    EXPECT_FALSE(head);
    EXPECT_FALSE(consumer_handle_);
    EXPECT_FALSE(cached_metadata);
  }));
  // Append enough data to start streaming.
  AppendData(kLargeEnoughScript);
  producer_handle_.reset();
  background_response_processor_client_.WaitUntilFinished();
  background_response_processor_client_.CheckResultOfFinishCallback(
      /*expected_body=*/base::make_span(kLargeEnoughScript,
                                        sizeof(kLargeEnoughScript) - 1),
      /*expected_cached_metadata=*/std::nullopt);
  Finish();
  RunUntilResourceLoaded();
  // ScriptStreamer must have been created.
  CheckScriptStreamer();
}

TEST_F(BackgroundResourceScriptStreamerTest, EnoughDataInFirstChunk) {
  V8TestingScope scope;
  Init(scope.GetIsolate());
  // Append the data chunk to the producer handle before
  // MaybeStartProcessingResponse(), so that MaybeStartProcessingResponse() can
  // synchronously read the data chunk in the data pipe.
  AppendData(kLargeEnoughScript);
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    network::mojom::URLResponseHeadPtr head = CreateURLResponseHead();
    std::optional<mojo_base::BigBuffer> cached_metadata;
    EXPECT_TRUE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
    EXPECT_FALSE(head);
    EXPECT_FALSE(consumer_handle_);
    EXPECT_FALSE(cached_metadata);
  }));
  producer_handle_.reset();
  background_response_processor_client_.WaitUntilFinished();
  background_response_processor_client_.CheckResultOfFinishCallback(
      /*expected_body=*/base::make_span(kLargeEnoughScript,
                                        sizeof(kLargeEnoughScript) - 1),
      /*expected_cached_metadata=*/std::nullopt);
  Finish();
  RunUntilResourceLoaded();
  // ScriptStreamer must have been created.
  CheckScriptStreamer();
}

TEST_F(BackgroundResourceScriptStreamerTest, EnoughDataModuleScript) {
  V8TestingScope scope;
  Init(scope.GetIsolate(), /*is_module_script=*/true);
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    network::mojom::URLResponseHeadPtr head = CreateURLResponseHead();
    std::optional<mojo_base::BigBuffer> cached_metadata;
    EXPECT_TRUE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
    EXPECT_FALSE(head);
    EXPECT_FALSE(consumer_handle_);
    EXPECT_FALSE(cached_metadata);
  }));
  // Append enough data to start streaming.
  AppendData(kLargeEnoughScript);
  producer_handle_.reset();
  background_response_processor_client_.WaitUntilFinished();
  background_response_processor_client_.CheckResultOfFinishCallback(
      /*expected_body=*/base::make_span(kLargeEnoughScript,
                                        sizeof(kLargeEnoughScript) - 1),
      /*expected_cached_metadata=*/std::nullopt);
  Finish();
  RunUntilResourceLoaded();
  // ScriptStreamer must have been created.
  CheckScriptStreamer(mojom::blink::ScriptType::kModule);
}

TEST_F(BackgroundResourceScriptStreamerTest, EncodingNotSupported) {
  V8TestingScope scope;
  // Intentionally using unsupported encoding "EUC-JP".
  Init(scope.GetIsolate(), /*is_module_script=*/false,
       WTF::TextEncoding("EUC-JP"));
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    network::mojom::URLResponseHeadPtr head = CreateURLResponseHead();
    std::optional<mojo_base::BigBuffer> cached_metadata;
    EXPECT_TRUE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
    EXPECT_FALSE(head);
    EXPECT_FALSE(consumer_handle_);
    EXPECT_FALSE(cached_metadata);
  }));
  // Append enough data to start streaming.
  AppendData(kLargeEnoughScript);
  producer_handle_.reset();
  background_response_processor_client_.WaitUntilFinished();
  background_response_processor_client_.CheckResultOfFinishCallback(
      /*expected_body=*/base::make_span(kLargeEnoughScript,
                                        sizeof(kLargeEnoughScript) - 1),
      /*expected_cached_metadata=*/std::nullopt);
  Finish();
  RunUntilResourceLoaded();
  // The encoding of the script is not supported.
  CheckNotStreamingReason(
      ScriptStreamer::NotStreamingReason::kEncodingNotSupportedBackground);
}

TEST_F(BackgroundResourceScriptStreamerTest, EncodingFromBOM) {
  V8TestingScope scope;
  // Intentionally using unsupported encoding "EUC-JP".
  Init(scope.GetIsolate(), /*is_module_script=*/false,
       WTF::TextEncoding("EUC-JP"));
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    network::mojom::URLResponseHeadPtr head = CreateURLResponseHead();
    std::optional<mojo_base::BigBuffer> cached_metadata;
    EXPECT_TRUE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
    EXPECT_FALSE(head);
    EXPECT_FALSE(consumer_handle_);
    EXPECT_FALSE(cached_metadata);
  }));
  // Append script with BOM
  AppendData(kScriptWithBOM);
  producer_handle_.reset();
  background_response_processor_client_.WaitUntilFinished();
  background_response_processor_client_.CheckResultOfFinishCallback(
      /*expected_body=*/base::make_span(kScriptWithBOM,
                                        sizeof(kScriptWithBOM) - 1),
      /*expected_cached_metadata=*/std::nullopt);
  Finish();
  RunUntilResourceLoaded();
  // ScriptStreamer must have been created.
  CheckScriptStreamer();
}

TEST_F(BackgroundResourceScriptStreamerTest, ScriptTypeMismatch) {
  V8TestingScope scope;
  Init(scope.GetIsolate(), /*is_module_script=*/true);
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    network::mojom::URLResponseHeadPtr head = CreateURLResponseHead();
    std::optional<mojo_base::BigBuffer> cached_metadata;
    EXPECT_TRUE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
    EXPECT_FALSE(head);
    EXPECT_FALSE(consumer_handle_);
    EXPECT_FALSE(cached_metadata);
  }));
  // Append enough data to start streaming.
  AppendData(kLargeEnoughScript);
  producer_handle_.reset();
  background_response_processor_client_.WaitUntilFinished();
  background_response_processor_client_.CheckResultOfFinishCallback(
      /*expected_body=*/base::make_span(kLargeEnoughScript,
                                        sizeof(kLargeEnoughScript) - 1),
      /*expected_cached_metadata=*/std::nullopt);
  Finish();
  RunUntilResourceLoaded();
  // Taking ScriptStreamer as a classic script shouold fail with
  // kErrorScriptTypeMismatch, because the script is a module script.
  CheckNotStreamingReason(
      ScriptStreamer::NotStreamingReason::kErrorScriptTypeMismatch,
      mojom::blink::ScriptType::kClassic);
}

TEST_F(BackgroundResourceScriptStreamerTest, CancelWhileWaitingForDataPipe) {
  V8TestingScope scope;
  Init(scope.GetIsolate());
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    network::mojom::URLResponseHeadPtr head = CreateURLResponseHead();
    std::optional<mojo_base::BigBuffer> cached_metadata;
    EXPECT_TRUE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
    EXPECT_FALSE(head);
    EXPECT_FALSE(consumer_handle_);
    EXPECT_FALSE(cached_metadata);
  }));
  Cancel();
  RunInBackgroundThred(base::BindLambdaForTesting(
      [&]() { background_response_processor_.reset(); }));
  producer_handle_.reset();
  // Cancelling the background response processor while waiting for data pipe
  // should not cause any crash.
  task_environment_.RunUntilIdle();
}

TEST_F(BackgroundResourceScriptStreamerTest, CancelBeforeReceiveResponse) {
  V8TestingScope scope;
  Init(scope.GetIsolate());
  Cancel();
  RunInBackgroundThred(base::BindLambdaForTesting(
      [&]() { background_response_processor_.reset(); }));
  // Cancelling the background response processor before receiving response
  // should not cause any crash.
  task_environment_.RunUntilIdle();
}

TEST_F(BackgroundResourceScriptStreamerTest, CancelWhileRuningStreamingTask) {
  V8TestingScope scope;
  Init(scope.GetIsolate());
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    network::mojom::URLResponseHeadPtr head = CreateURLResponseHead();
    std::optional<mojo_base::BigBuffer> cached_metadata;
    EXPECT_TRUE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
    EXPECT_FALSE(head);
    EXPECT_FALSE(consumer_handle_);
    EXPECT_FALSE(cached_metadata);
  }));
  // Append enough data to start streaming.
  AppendData(kLargeEnoughScript);
  Cancel();
  RunInBackgroundThred(base::BindLambdaForTesting(
      [&]() { background_response_processor_.reset(); }));
  producer_handle_.reset();
  // Cancelling the background response processor while running streaming task
  // should not cause any crash.
  task_environment_.RunUntilIdle();
}

TEST_F(BackgroundResourceScriptStreamerTest, CompilingStreamedScript) {
  V8TestingScope scope;
  Init(scope.GetIsolate());
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    network::mojom::URLResponseHeadPtr head = CreateURLResponseHead();
    std::optional<mojo_base::BigBuffer> cached_metadata;
    EXPECT_TRUE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
    EXPECT_FALSE(head);
    EXPECT_FALSE(consumer_handle_);
    EXPECT_FALSE(cached_metadata);
  }));
  // Append enough data to start streaming.
  AppendData(kLargeEnoughScript);
  producer_handle_.reset();
  background_response_processor_client_.WaitUntilFinished();
  background_response_processor_client_.CheckResultOfFinishCallback(
      /*expected_body=*/base::make_span(kLargeEnoughScript,
                                        sizeof(kLargeEnoughScript) - 1),
      /*expected_cached_metadata=*/std::nullopt);
  Finish();
  RunUntilResourceLoaded();

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

TEST_F(BackgroundResourceScriptStreamerTest,
       CompilingStreamedScriptWithParseError) {
  V8TestingScope scope;
  Init(scope.GetIsolate());
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    network::mojom::URLResponseHeadPtr head = CreateURLResponseHead();
    std::optional<mojo_base::BigBuffer> cached_metadata;
    EXPECT_TRUE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
    EXPECT_FALSE(head);
    EXPECT_FALSE(consumer_handle_);
    EXPECT_FALSE(cached_metadata);
  }));
  const char kInvalidScript[] =
      "This is an invalid script which cause a parse error";
  AppendData(kInvalidScript);
  producer_handle_.reset();
  background_response_processor_client_.WaitUntilFinished();
  background_response_processor_client_.CheckResultOfFinishCallback(
      /*expected_body=*/base::make_span(kInvalidScript,
                                        sizeof(kInvalidScript) - 1),
      /*expected_cached_metadata=*/std::nullopt);
  Finish();
  RunUntilResourceLoaded();

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

// Regression test for https://crbug.com/337998760.
TEST_F(BackgroundResourceScriptStreamerTest, DataPipeReadableAfterGC) {
  V8TestingScope scope;
  Init(scope.GetIsolate());
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    network::mojom::URLResponseHeadPtr head = CreateURLResponseHead();
    std::optional<mojo_base::BigBuffer> cached_metadata;
    EXPECT_TRUE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
    EXPECT_FALSE(head);
    EXPECT_FALSE(consumer_handle_);
    EXPECT_FALSE(cached_metadata);
  }));

  // Start blocking the background thread.
  base::WaitableEvent waitable_event;
  background_resource_fetch_task_runner_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
        waitable_event.Wait();
      }));

  // Resetting `producer_handle_` will triggers OnDataPipeReadable() on the
  // background thread. But the background thread is still blocked by the
  // `waitable_event`.
  producer_handle_.reset();

  Cancel();

  resource_ = nullptr;
  resource_client_ = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();

  // Unblock the background thread.
  waitable_event.Signal();

  task_environment_.RunUntilIdle();
}

TEST_F(BackgroundResourceScriptStreamerTest,
       DataPipeReadableAfterProcessorIsDeleted) {
  V8TestingScope scope;
  Init(scope.GetIsolate());
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    network::mojom::URLResponseHeadPtr head = CreateURLResponseHead();
    std::optional<mojo_base::BigBuffer> cached_metadata;
    EXPECT_TRUE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
    EXPECT_FALSE(head);
    EXPECT_FALSE(consumer_handle_);
    EXPECT_FALSE(cached_metadata);
  }));

  // Start blocking the background thread.
  base::WaitableEvent waitable_event;
  background_resource_fetch_task_runner_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
        waitable_event.Wait();
        // Delete `background_response_processor_` before SimpleWatcher calls
        // OnDataPipeReadable().
        background_response_processor_.reset();
      }));

  // Resetting `producer_handle_` will triggers SimpleWatcher's callback on the
  // background thread. But the background thread is still blocked by the
  // `waitable_event`.
  producer_handle_.reset();

  Cancel();

  resource_ = nullptr;
  resource_client_ = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();

  // Unblock the background thread.
  waitable_event.Signal();

  task_environment_.RunUntilIdle();
}

// Regression test for https://crbug.com/341473518.
TEST_F(BackgroundResourceScriptStreamerTest,
       DeletingBackgroundProcessorWhileParsingShouldNotCrash) {
  V8TestingScope scope;
  v8_compile_hints::V8CrowdsourcedCompileHintsConsumer*
      v8_compile_hints_consumer = MakeGarbageCollected<
          v8_compile_hints::V8CrowdsourcedCompileHintsConsumer>();
  Vector<int64_t> dummy_data(v8_compile_hints::kBloomFilterInt32Count / 2);
  v8_compile_hints_consumer->SetData(dummy_data.data(), dummy_data.size());

  Init(scope.GetIsolate(), /*is_module_script=*/false, /*charset=*/std::nullopt,
       v8_compile_hints_consumer);
  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    network::mojom::URLResponseHeadPtr head = CreateURLResponseHead();
    std::optional<mojo_base::BigBuffer> cached_metadata;
    EXPECT_TRUE(background_response_processor_->MaybeStartProcessingResponse(
        head, consumer_handle_, cached_metadata,
        background_resource_fetch_task_runner_,
        &background_response_processor_client_));
  }));

  std::string comment_line =
      base::StrCat({std::string(kDataPipeSize - 1, '/'), "\n"});
  AppendData(comment_line);

  RunInBackgroundThred(base::BindLambdaForTesting([&]() {
    // Call YieldCurrentThread() until the parser thread reads the
    // `comment_line` form the data pipe.
    while (!producer_handle_->QuerySignalsState().writable()) {
      test::YieldCurrentThread();
    }
    const std::string kFunctionScript = "function a() {console.log('');}";
    const std::string function_line = base::StrCat(
        {kFunctionScript,
         std::string(kDataPipeSize - kFunctionScript.size(), '/')});
    MojoResult result =
        producer_handle_->WriteAllData(base::as_byte_span(function_line));
    EXPECT_EQ(result, MOJO_RESULT_OK);
    // Busyloop until the parser thread reads the `function_line` form the data
    // pipe.
    while (!producer_handle_->QuerySignalsState().writable()) {
    }
    // Delete the BackgroundProcessor. This is intended to make sure that
    // deleting the BackgroundProcessor while the parser thread is parsing the
    // script should not cause a crash.
    background_response_processor_.reset();
  }));

  producer_handle_.reset();

  task_environment_.RunUntilIdle();
}

}  // namespace blink
