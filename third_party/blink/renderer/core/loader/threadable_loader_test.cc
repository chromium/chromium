// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/threadable_loader.h"

#include <memory>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "services/network/public/mojom/load_timing_info.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink-forward.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/loader/threadable_loader_client.h"
#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_thread_test_helper.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

using testing::_;
using testing::ElementsAre;
using testing::InSequence;
using testing::InvokeWithoutArgs;
using testing::Truly;
using Checkpoint = testing::StrictMock<testing::MockFunction<void(int)>>;

constexpr char kFileName[] = "fox-null-terminated.html";

class MockThreadableLoaderClient final
    : public GarbageCollected<MockThreadableLoaderClient>,
      public ThreadableLoaderClient {
 public:
  MockThreadableLoaderClient() = default;
  MOCK_METHOD2(DidSendData, void(uint64_t, uint64_t));
  MOCK_METHOD2(DidReceiveResponse, void(uint64_t, const ResourceResponse&));
  MOCK_METHOD1(DidReceiveData, void(base::span<const char>));
  MOCK_METHOD1(DidReceiveCachedMetadata, void(mojo_base::BigBuffer));
  MOCK_METHOD1(DidFinishLoading, void(uint64_t));
  MOCK_METHOD2(DidFail, void(uint64_t, const ResourceError&));
  MOCK_METHOD1(DidFailRedirectCheck, void(uint64_t));
  MOCK_METHOD1(DidDownloadData, void(uint64_t));
};

bool IsCancellation(const ResourceError& error) {
  return error.IsCancellation();
}

bool IsNotCancellation(const ResourceError& error) {
  return !error.IsCancellation();
}

KURL SuccessURL() {
  return KURL("http://example.com/success");
}
KURL ErrorURL() {
  return KURL("http://example.com/error");
}
KURL RedirectURL() {
  return KURL("http://example.com/redirect");
}

void SetUpSuccessURL() {
  // TODO(crbug.com/751425): We should use the mock functionality
  // via |dummy_page_holder_|.
  url_test_helpers::RegisterMockedURLLoad(
      SuccessURL(), test::CoreTestDataPath(kFileName), "text/html");
}

void SetUpErrorURL() {
  // TODO(crbug.com/751425): We should use the mock functionality
  // via |dummy_page_holder_|.
  url_test_helpers::RegisterMockedErrorURLLoad(ErrorURL());
}

void SetUpRedirectURL() {
  KURL url = RedirectURL();

  network::mojom::LoadTimingInfoPtr timing =
      network::mojom::LoadTimingInfo::New();

  WebURLResponse response;
  response.SetCurrentRequestUrl(url);
  response.SetHttpStatusCode(301);
  response.SetLoadTiming(*timing);
  response.AddHttpHeaderField("Location", SuccessURL().GetString());
  response.AddHttpHeaderField("Access-Control-Allow-Origin", "http://fake.url");

  // TODO(crbug.com/751425): We should use the mock functionality
  // via |dummy_page_holder_|.
  url_test_helpers::RegisterMockedURLLoadWithCustomResponse(
      url, test::CoreTestDataPath(kFileName), response);
}

void SetUpMockURLs() {
  SetUpSuccessURL();
  SetUpErrorURL();
  SetUpRedirectURL();
}

enum ThreadableLoaderToTest {
  kDocumentThreadableLoaderTest,
  kWorkerThreadableLoaderTest,
};

class ThreadableLoaderTestHelper final {
 public:
  ThreadableLoaderTestHelper()
      : dummy_page_holder_(std::make_unique<DummyPageHolder>(gfx::Size(1, 1))) {
    KURL url("http://fake.url/");
    dummy_page_holder_->GetFrame().Loader().CommitNavigation(
        WebNavigationParams::CreateWithEmptyHTMLForTesting(url),
        nullptr /* extra_data */);
    blink::test::RunPendingTasks();
  }

  void CreateLoader(ThreadableLoaderClient* client) {
    ResourceLoaderOptions resource_loader_options(nullptr /* world */);
    loader_ = MakeGarbageCollected<ThreadableLoader>(
        *dummy_page_holder_->GetFrame().DomWindow(), client,
        resource_loader_options);
  }

  void StartLoader(ResourceRequest request) {
    loader_->Start(std::move(request));
  }

  void CancelLoader() { loader_->Cancel(); }
  void CancelAndClearLoader() {
    loader_->Cancel();
    loader_ = nullptr;
  }
  void ClearLoader() { loader_ = nullptr; }

  Checkpoint& GetCheckpoint() { return checkpoint_; }
  void CallCheckpoint(int n) { checkpoint_.Call(n); }

  void OnSetUp() { SetUpMockURLs(); }

  void OnServeRequests() { url_test_helpers::ServeAsynchronousRequests(); }

  void OnTearDown() {
    if (loader_) {
      loader_->Cancel();
      loader_ = nullptr;
    }
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  Checkpoint checkpoint_;
  Persistent<ThreadableLoader> loader_;
};

class ThreadableLoaderTest : public testing::Test {
 public:
  ThreadableLoaderTest()
      : helper_(std::make_unique<ThreadableLoaderTestHelper>()) {}

  void StartLoader(const KURL& url,
                   network::mojom::RequestMode request_mode =
                       network::mojom::RequestMode::kNoCors) {
    ResourceRequest request(url);
    request.SetRequestContext(mojom::blink::RequestContextType::OBJECT);
    request.SetMode(request_mode);
    request.SetTargetAddressSpace(network::mojom::IPAddressSpace::kUnknown);
    request.SetCredentialsMode(network::mojom::CredentialsMode::kOmit);
    helper_->StartLoader(std::move(request));
  }

  void CancelLoader() { helper_->CancelLoader(); }
  void CancelAndClearLoader() { helper_->CancelAndClearLoader(); }
  void ClearLoader() { helper_->ClearLoader(); }

  Checkpoint& GetCheckpoint() { return helper_->GetCheckpoint(); }
  void CallCheckpoint(int n) { helper_->CallCheckpoint(n); }

  void ServeRequests() { helper_->OnServeRequests(); }

  void CreateLoader() { helper_->CreateLoader(Client()); }

  MockThreadableLoaderClient* Client() const { return client_.Get(); }

 private:
  void SetUp() override {
    client_ = MakeGarbageCollected<MockThreadableLoaderClient>();
    helper_->OnSetUp();
  }

  void TearDown() override {
    helper_->OnTearDown();
    client_ = nullptr;
    // We need GC here to avoid gmock flakiness.
    ThreadState::Current()->CollectAllGarbageForTesting();
  }
  Persistent<MockThreadableLoaderClient> client_;
  std::unique_ptr<ThreadableLoaderTestHelper> helper_;
};

TEST_F(ThreadableLoaderTest, StartAndStop) {}

TEST_F(ThreadableLoaderTest, CancelAfterStart) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelLoader));
  EXPECT_CALL(*Client(), DidFail(_, Truly(IsCancellation)));
  EXPECT_CALL(GetCheckpoint(), Call(3));

  StartLoader(SuccessURL());
  CallCheckpoint(2);
  CallCheckpoint(3);
  ServeRequests();
}

TEST_F(ThreadableLoaderTest, CancelAndClearAfterStart) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2))
      .WillOnce(
          InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelAndClearLoader));
  EXPECT_CALL(*Client(), DidFail(_, Truly(IsCancellation)));
  EXPECT_CALL(GetCheckpoint(), Call(3));

  StartLoader(SuccessURL());
  CallCheckpoint(2);
  CallCheckpoint(3);
  ServeRequests();
}

TEST_F(ThreadableLoaderTest, CancelInDidReceiveResponse) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponse(_, _))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelLoader));
  EXPECT_CALL(*Client(), DidFail(_, Truly(IsCancellation)));

  StartLoader(SuccessURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_F(ThreadableLoaderTest, CancelAndClearInDidReceiveResponse) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponse(_, _))
      .WillOnce(
          InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelAndClearLoader));
  EXPECT_CALL(*Client(), DidFail(_, Truly(IsCancellation)));

  StartLoader(SuccessURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_F(ThreadableLoaderTest, CancelInDidReceiveData) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponse(_, _));
  EXPECT_CALL(*Client(), DidReceiveData(_))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelLoader));
  EXPECT_CALL(*Client(), DidFail(_, Truly(IsCancellation)));

  StartLoader(SuccessURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_F(ThreadableLoaderTest, CancelAndClearInDidReceiveData) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponse(_, _));
  EXPECT_CALL(*Client(), DidReceiveData(_))
      .WillOnce(
          InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelAndClearLoader));
  EXPECT_CALL(*Client(), DidFail(_, Truly(IsCancellation)));

  StartLoader(SuccessURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_F(ThreadableLoaderTest, DidFinishLoading) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponse(_, _));
  EXPECT_CALL(*Client(), DidReceiveData(ElementsAre('f', 'o', 'x', '\0')));
  EXPECT_CALL(*Client(), DidFinishLoading(_));

  StartLoader(SuccessURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_F(ThreadableLoaderTest, CancelInDidFinishLoading) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponse(_, _));
  EXPECT_CALL(*Client(), DidReceiveData(_));
  EXPECT_CALL(*Client(), DidFinishLoading(_))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelLoader));

  StartLoader(SuccessURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_F(ThreadableLoaderTest, ClearInDidFinishLoading) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponse(_, _));
  EXPECT_CALL(*Client(), DidReceiveData(_));
  EXPECT_CALL(*Client(), DidFinishLoading(_))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::ClearLoader));

  StartLoader(SuccessURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_F(ThreadableLoaderTest, DidFail) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidFail(_, Truly(IsNotCancellation)));

  StartLoader(ErrorURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_F(ThreadableLoaderTest, CancelInDidFail) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidFail(_, _))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelLoader));

  StartLoader(ErrorURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_F(ThreadableLoaderTest, ClearInDidFail) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidFail(_, _))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::ClearLoader));

  StartLoader(ErrorURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_F(ThreadableLoaderTest, RedirectDidFinishLoading) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponse(_, _));
  EXPECT_CALL(*Client(), DidReceiveData(ElementsAre('f', 'o', 'x', '\0')));
  EXPECT_CALL(*Client(), DidFinishLoading(_));

  StartLoader(RedirectURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_F(ThreadableLoaderTest, CancelInRedirectDidFinishLoading) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponse(_, _));
  EXPECT_CALL(*Client(), DidReceiveData(ElementsAre('f', 'o', 'x', '\0')));
  EXPECT_CALL(*Client(), DidFinishLoading(_))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelLoader));

  StartLoader(RedirectURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_F(ThreadableLoaderTest, ClearInRedirectDidFinishLoading) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponse(_, _));
  EXPECT_CALL(*Client(), DidReceiveData(ElementsAre('f', 'o', 'x', '\0')));
  EXPECT_CALL(*Client(), DidFinishLoading(_))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::ClearLoader));

  StartLoader(RedirectURL());
  CallCheckpoint(2);
  ServeRequests();
}

// TODO(crbug.com/1356128): Add unit tests to cover histogram logging.

}  // namespace

}  // namespace blink
