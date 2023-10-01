// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"

#include "base/debug/stack_trace.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_factory.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/test_loader_factory.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/testing/mock_context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

using FetchCachedCodeCallback =
    mojom::blink::CodeCacheHost::FetchCachedCodeCallback;
using ProcessCodeCacheRequestCallback =
    base::RepeatingCallback<void(FetchCachedCodeCallback)>;

// A mock URLLoader to know the status of defers flag.
class TestURLLoader final : public URLLoader {
 public:
  explicit TestURLLoader(LoaderFreezeMode* const freeze_mode_ptr)
      : freeze_mode_ptr_(freeze_mode_ptr) {}
  ~TestURLLoader() override = default;

  void LoadSynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      bool pass_response_pipe_to_client,
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
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      bool no_mime_sniffing,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper,
      URLLoaderClient*) override {}

  void Freeze(LoaderFreezeMode mode) override { *freeze_mode_ptr_ = mode; }
  void DidChangePriority(WebURLRequest::Priority, int) override {
    NOTREACHED();
  }
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForBodyLoader()
      override {
    return base::MakeRefCounted<scheduler::FakeTaskRunner>();
  }

 private:
  // Points to |ResourceLoaderDefersLoadingTest::freeze_mode_|.
  const raw_ptr<LoaderFreezeMode, ExperimentalRenderer> freeze_mode_ptr_;
};

class DummyCodeCacheHost final : public mojom::blink::CodeCacheHost {
 public:
  explicit DummyCodeCacheHost(
      ProcessCodeCacheRequestCallback process_code_cache_request_callback)
      : process_code_cache_request_callback_(
            std::move(process_code_cache_request_callback)) {}

  // mojom::blink::CodeCacheHost implementations
  void DidGenerateCacheableMetadata(mojom::blink::CodeCacheType cache_type,
                                    const KURL& url,
                                    base::Time expected_response_time,
                                    mojo_base::BigBuffer data) override {}
  void FetchCachedCode(mojom::blink::CodeCacheType cache_type,
                       const KURL& url,
                       FetchCachedCodeCallback callback) override {
    process_code_cache_request_callback_.Run(std::move(callback));
  }
  void ClearCodeCacheEntry(mojom::blink::CodeCacheType cache_type,
                           const KURL& url) override {}
  void DidGenerateCacheableMetadataInCacheStorage(
      const KURL& url,
      base::Time expected_response_time,
      mojo_base::BigBuffer data,
      const WTF::String& cache_storage_cache_name) override {}

 private:
  ProcessCodeCacheRequestCallback process_code_cache_request_callback_;
};

class DeferTestLoaderFactory final : public ResourceFetcher::LoaderFactory {
 public:
  DeferTestLoaderFactory(
      LoaderFreezeMode* const freeze_mode_ptr,
      ProcessCodeCacheRequestCallback process_code_cache_request_callback)
      : freeze_mode_ptr_(freeze_mode_ptr) {
    mojo::PendingRemote<mojom::blink::CodeCacheHost> pending_remote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<DummyCodeCacheHost>(
            std::move(process_code_cache_request_callback)),
        pending_remote.InitWithNewPipeAndPassReceiver());
    code_cache_host_ = std::make_unique<CodeCacheHost>(
        mojo::Remote<mojom::blink::CodeCacheHost>(std::move(pending_remote)));
  }

  // LoaderFactory implementations
  std::unique_ptr<URLLoader> CreateURLLoader(
      const ResourceRequest& request,
      const ResourceLoaderOptions& options,
      scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
      BackForwardCacheLoaderHelper* back_forward_cache_loader_helper) override {
    return std::make_unique<TestURLLoader>(freeze_mode_ptr_);
  }

  CodeCacheHost* GetCodeCacheHost() override { return code_cache_host_.get(); }

 private:
  // Points to |ResourceLoaderDefersLoadingTest::freeze_mode_|.
  const raw_ptr<LoaderFreezeMode, ExperimentalRenderer> freeze_mode_ptr_;

  std::unique_ptr<CodeCacheHost> code_cache_host_;
};

}  // namespace

class ResourceLoaderDefersLoadingTest : public testing::Test {
 public:
  ResourceLoaderDefersLoadingTest() {
    SetCodeCacheProcessFunction(base::BindRepeating(
        &ResourceLoaderDefersLoadingTest::SaveCodeCacheCallback,
        base::Unretained(this)));
  }

  void SaveCodeCacheCallback(FetchCachedCodeCallback callback) {
    // Store the callback to send back a response.
    code_cache_response_callback_ = std::move(callback);
    if (save_code_cache_callback_done_closure_) {
      std::move(save_code_cache_callback_done_closure_).Run();
    }
  }

  ResourceFetcher* CreateFetcher() {
    return MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
        MakeGarbageCollected<TestResourceFetcherProperties>()->MakeDetachable(),
        MakeGarbageCollected<MockFetchContext>(),
        base::MakeRefCounted<scheduler::FakeTaskRunner>(),
        base::MakeRefCounted<scheduler::FakeTaskRunner>(),
        MakeGarbageCollected<DeferTestLoaderFactory>(
            &freeze_mode_, process_code_cache_request_callback_),
        MakeGarbageCollected<MockContextLifecycleNotifier>(),
        nullptr /* back_forward_cache_loader_helper */));
  }

  void SetCodeCacheProcessFunction(ProcessCodeCacheRequestCallback callback) {
    process_code_cache_request_callback_ = callback;
  }

  void SetSaveCodeCacheCallbackDoneClosure(base::OnceClosure closure) {
    save_code_cache_callback_done_closure_ = std::move(closure);
  }

  ProcessCodeCacheRequestCallback process_code_cache_request_callback_;
  FetchCachedCodeCallback code_cache_response_callback_;

  // Passed to TestURLLoader (via |platform_|) and updated when its Freeze
  // method is called.
  LoaderFreezeMode freeze_mode_ = LoaderFreezeMode::kNone;
  const KURL test_url_ = KURL("http://example.com/");

  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;

 private:
  base::OnceClosure save_code_cache_callback_done_closure_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(ResourceLoaderDefersLoadingTest, CodeCacheFetchCheckDefers) {
  auto* fetcher = CreateFetcher();

  ResourceRequest request;
  request.SetUrl(test_url_);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);
  FetchParameters fetch_parameters =
      FetchParameters::CreateForTest(std::move(request));

  base::RunLoop run_loop;
  SetSaveCodeCacheCallbackDoneClosure(run_loop.QuitClosure());
  Resource* resource = RawResource::Fetch(fetch_parameters, fetcher, nullptr);

  // After code cache fetch it should have deferred URLLoader.
  DCHECK_EQ(freeze_mode_, LoaderFreezeMode::kStrict);
  DCHECK(resource);

  run_loop.Run();
  std::move(code_cache_response_callback_).Run(base::Time(), {});
  test::RunPendingTasks();
  // Once the response is received it should be reset.
  DCHECK_EQ(freeze_mode_, LoaderFreezeMode::kNone);
}

TEST_F(ResourceLoaderDefersLoadingTest, ChangeDefersToFalse) {
  auto* fetcher = CreateFetcher();

  ResourceRequest request;
  request.SetUrl(test_url_);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);
  FetchParameters fetch_parameters =
      FetchParameters::CreateForTest(std::move(request));

  base::RunLoop run_loop;
  SetSaveCodeCacheCallbackDoneClosure(run_loop.QuitClosure());
  Resource* resource = RawResource::Fetch(fetch_parameters, fetcher, nullptr);
  DCHECK_EQ(freeze_mode_, LoaderFreezeMode::kStrict);

  // Change Defers loading to false. This should not be sent to URLLoader since
  // a code cache request is still pending.
  ResourceLoader* loader = resource->Loader();
  loader->SetDefersLoading(LoaderFreezeMode::kNone);
  DCHECK_EQ(freeze_mode_, LoaderFreezeMode::kStrict);

  run_loop.Run();
  std::move(code_cache_response_callback_).Run(base::Time(), {});
  test::RunPendingTasks();
  // Once the response is received it should be reset.
  DCHECK_EQ(freeze_mode_, LoaderFreezeMode::kNone);
}

TEST_F(ResourceLoaderDefersLoadingTest, ChangeDefersToTrue) {
  auto* fetcher = CreateFetcher();

  ResourceRequest request;
  request.SetUrl(test_url_);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);
  FetchParameters fetch_parameters =
      FetchParameters::CreateForTest(std::move(request));

  base::RunLoop run_loop;
  SetSaveCodeCacheCallbackDoneClosure(run_loop.QuitClosure());
  Resource* resource = RawResource::Fetch(fetch_parameters, fetcher, nullptr);
  DCHECK_EQ(freeze_mode_, LoaderFreezeMode::kStrict);

  ResourceLoader* loader = resource->Loader();
  loader->SetDefersLoading(LoaderFreezeMode::kStrict);
  DCHECK_EQ(freeze_mode_, LoaderFreezeMode::kStrict);

  run_loop.Run();
  std::move(code_cache_response_callback_).Run(base::Time(), {});
  test::RunPendingTasks();
  // Since it was requested to be deferred, it should be reset to the
  // correct value.
  DCHECK_EQ(freeze_mode_, LoaderFreezeMode::kStrict);
}

TEST_F(ResourceLoaderDefersLoadingTest, ChangeDefersToBfcacheDefer) {
  auto* fetcher = CreateFetcher();

  ResourceRequest request;
  request.SetUrl(test_url_);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);
  FetchParameters fetch_parameters =
      FetchParameters::CreateForTest(std::move(request));

  base::RunLoop run_loop;
  SetSaveCodeCacheCallbackDoneClosure(run_loop.QuitClosure());
  Resource* resource = RawResource::Fetch(fetch_parameters, fetcher, nullptr);
  DCHECK_EQ(freeze_mode_, LoaderFreezeMode::kStrict);

  ResourceLoader* loader = resource->Loader();
  loader->SetDefersLoading(LoaderFreezeMode::kBufferIncoming);
  DCHECK_EQ(freeze_mode_, LoaderFreezeMode::kStrict);

  run_loop.Run();
  std::move(code_cache_response_callback_).Run(base::Time(), {});
  test::RunPendingTasks();
  // Since it was requested to be deferred, it should be reset to the
  // correct value.
  DCHECK_EQ(freeze_mode_, LoaderFreezeMode::kBufferIncoming);
}

TEST_F(ResourceLoaderDefersLoadingTest, ChangeDefersMultipleTimes) {
  auto* fetcher = CreateFetcher();

  ResourceRequest request;
  request.SetUrl(test_url_);
  request.SetRequestContext(mojom::blink::RequestContextType::FETCH);

  FetchParameters fetch_parameters =
      FetchParameters::CreateForTest(std::move(request));
  base::RunLoop run_loop;
  SetSaveCodeCacheCallbackDoneClosure(run_loop.QuitClosure());
  Resource* resource = RawResource::Fetch(fetch_parameters, fetcher, nullptr);
  DCHECK_EQ(freeze_mode_, LoaderFreezeMode::kStrict);

  ResourceLoader* loader = resource->Loader();
  loader->SetDefersLoading(LoaderFreezeMode::kStrict);
  DCHECK_EQ(freeze_mode_, LoaderFreezeMode::kStrict);

  loader->SetDefersLoading(LoaderFreezeMode::kNone);
  DCHECK_EQ(freeze_mode_, LoaderFreezeMode::kStrict);

  run_loop.Run();
  std::move(code_cache_response_callback_).Run(base::Time(), {});
  test::RunPendingTasks();
  DCHECK_EQ(freeze_mode_, LoaderFreezeMode::kNone);
}

}  // namespace blink
