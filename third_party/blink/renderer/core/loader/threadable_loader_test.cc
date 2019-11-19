// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/threadable_loader.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_url_load_timing.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/loader/threadable_loader_client.h"
#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_thread_test_helper.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/loader/testing/web_url_loader_factory_with_mock.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

using testing::_;
using testing::InSequence;
using testing::InvokeWithoutArgs;
using testing::StrEq;
using testing::Truly;
using Checkpoint = testing::StrictMock<testing::MockFunction<void(int)>>;

constexpr char kFileName[] = "fox-null-terminated.html";

class MockThreadableLoaderClient final
    : public GarbageCollected<MockThreadableLoaderClient>,
      public ThreadableLoaderClient {
  USING_GARBAGE_COLLECTED_MIXIN(MockThreadableLoaderClient);

 public:
  MockThreadableLoaderClient() = default;
  MOCK_METHOD2(DidSendData, void(uint64_t, uint64_t));
  MOCK_METHOD2(DidReceiveResponse, void(uint64_t, const ResourceResponse&));
  MOCK_METHOD2(DidReceiveData, void(const char*, unsigned));
  MOCK_METHOD2(DidReceiveCachedMetadata, void(const char*, int));
  MOCK_METHOD1(DidFinishLoading, void(uint64_t));
  MOCK_METHOD1(DidFail, void(const ResourceError&));
  MOCK_METHOD0(DidFailRedirectCheck, void());
  MOCK_METHOD1(DidDownloadData, void(uint64_t));
};

bool IsCancellation(const ResourceError& error) {
  return error.IsCancellation();
}

bool IsNotCancellation(const ResourceError& error) {
  return !error.IsCancellation();
}

KURL SuccessURL() {
  return KURL("http://example.com/success").Copy();
}
KURL ErrorURL() {
  return KURL("http://example.com/error").Copy();
}
KURL RedirectURL() {
  return KURL("http://example.com/redirect").Copy();
}
KURL RedirectLoopURL() {
  return KURL("http://example.com/loop").Copy();
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

  WebURLLoadTiming timing;
  timing.Initialize();

  WebURLResponse response;
  response.SetCurrentRequestUrl(url);
  response.SetHttpStatusCode(301);
  response.SetLoadTiming(timing);
  response.AddHttpHeaderField("Location", SuccessURL().GetString());
  response.AddHttpHeaderField("Access-Control-Allow-Origin", "http://fake.url");

  // TODO(crbug.com/751425): We should use the mock functionality
  // via |dummy_page_holder_|.
  url_test_helpers::RegisterMockedURLLoadWithCustomResponse(
      url, test::CoreTestDataPath(kFileName), response);
}

void SetUpRedirectLoopURL() {
  KURL url = RedirectLoopURL();

  WebURLLoadTiming timing;
  timing.Initialize();

  WebURLResponse response;
  response.SetCurrentRequestUrl(url);
  response.SetHttpStatusCode(301);
  response.SetLoadTiming(timing);
  response.AddHttpHeaderField("Location", RedirectLoopURL().GetString());
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
  SetUpRedirectLoopURL();
}

enum ThreadableLoaderToTest {
  kDocumentThreadableLoaderTest,
  kWorkerThreadableLoaderTest,
};

class ThreadableLoaderTestHelper final {
 public:
  ThreadableLoaderTestHelper()
      : dummy_page_holder_(std::make_unique<DummyPageHolder>(IntSize(1, 1))) {
    KURL url("http://fake.url/");
    dummy_page_holder_->GetFrame().Loader().CommitNavigation(
        WebNavigationParams::CreateWithHTMLBuffer(SharedBuffer::Create(), url),
        nullptr /* extra_data */);
    blink::test::RunPendingTasks();
  }

  void CreateLoader(ThreadableLoaderClient* client) {
    ResourceLoaderOptions resource_loader_options;
    loader_ = MakeGarbageCollected<ThreadableLoader>(GetDocument(), client,
                                                     resource_loader_options);
  }

  void StartLoader(const ResourceRequest& request) { loader_->Start(request); }

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
  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

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
    request.SetRequestContext(mojom::RequestContextType::OBJECT);
    request.SetMode(request_mode);
    request.SetCredentialsMode(network::mojom::CredentialsMode::kOmit);
    helper_->StartLoader(request);
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
  EXPECT_CALL(*Client(), DidFail(Truly(IsCancellation)));
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
  EXPECT_CALL(*Client(), DidFail(Truly(IsCancellation)));
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
  EXPECT_CALL(*Client(), DidFail(Truly(IsCancellation)));

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
  EXPECT_CALL(*Client(), DidFail(Truly(IsCancellation)));

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
  EXPECT_CALL(*Client(), DidReceiveData(_, _))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelLoader));
  EXPECT_CALL(*Client(), DidFail(Truly(IsCancellation)));

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
  EXPECT_CALL(*Client(), DidReceiveData(_, _))
      .WillOnce(
          InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelAndClearLoader));
  EXPECT_CALL(*Client(), DidFail(Truly(IsCancellation)));

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
  EXPECT_CALL(*Client(), DidReceiveData(StrEq("fox"), 4));
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
  EXPECT_CALL(*Client(), DidReceiveData(_, _));
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
  EXPECT_CALL(*Client(), DidReceiveData(_, _));
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
  EXPECT_CALL(*Client(), DidFail(Truly(IsNotCancellation)));

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
  EXPECT_CALL(*Client(), DidFail(_))
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
  EXPECT_CALL(*Client(), DidFail(_))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::ClearLoader));

  StartLoader(ErrorURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_F(ThreadableLoaderTest, DidFailInStart) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(
      *Client(),
      DidFail(ResourceError(
          ErrorURL(), network::CorsErrorStatus(
                          network::mojom::CorsError::kDisallowedByMode))));
  EXPECT_CALL(GetCheckpoint(), Call(2));

  StartLoader(ErrorURL(), network::mojom::RequestMode::kSameOrigin);
  CallCheckpoint(2);
  ServeRequests();
}

TEST_F(ThreadableLoaderTest, CancelInDidFailInStart) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(*Client(), DidFail(_))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelLoader));
  EXPECT_CALL(GetCheckpoint(), Call(2));

  StartLoader(ErrorURL(), network::mojom::RequestMode::kSameOrigin);
  CallCheckpoint(2);
  ServeRequests();
}

TEST_F(ThreadableLoaderTest, ClearInDidFailInStart) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(*Client(), DidFail(_))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::ClearLoader));
  EXPECT_CALL(GetCheckpoint(), Call(2));

  StartLoader(ErrorURL(), network::mojom::RequestMode::kSameOrigin);
  CallCheckpoint(2);
  ServeRequests();
}

TEST_F(ThreadableLoaderTest, DidFailAccessControlCheck) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(),
              DidFail(ResourceError(
                  SuccessURL(),
                  network::CorsErrorStatus(
                      network::mojom::CorsError::kMissingAllowOriginHeader))));

  StartLoader(SuccessURL(), network::mojom::RequestMode::kCors);
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
  EXPECT_CALL(*Client(), DidReceiveData(StrEq("fox"), 4));
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
  EXPECT_CALL(*Client(), DidReceiveData(StrEq("fox"), 4));
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
  EXPECT_CALL(*Client(), DidReceiveData(StrEq("fox"), 4));
  EXPECT_CALL(*Client(), DidFinishLoading(_))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::ClearLoader));

  StartLoader(RedirectURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_F(ThreadableLoaderTest, DidFailRedirectCheck) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidFailRedirectCheck());

  StartLoader(RedirectLoopURL(), network::mojom::RequestMode::kCors);
  CallCheckpoint(2);
  ServeRequests();
}

TEST_F(ThreadableLoaderTest, CancelInDidFailRedirectCheck) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidFailRedirectCheck())
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelLoader));

  StartLoader(RedirectLoopURL(), network::mojom::RequestMode::kCors);
  CallCheckpoint(2);
  ServeRequests();
}

TEST_F(ThreadableLoaderTest, ClearInDidFailRedirectCheck) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidFailRedirectCheck())
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::ClearLoader));

  StartLoader(RedirectLoopURL(), network::mojom::RequestMode::kCors);
  CallCheckpoint(2);
  ServeRequests();
}

// This test case checks blink doesn't crash even when the response arrives
// synchronously.
TEST_F(ThreadableLoaderTest, GetResponseSynchronously) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(*Client(), DidFail(_));
  EXPECT_CALL(GetCheckpoint(), Call(2));

  // Currently didFailAccessControlCheck is dispatched synchronously. This
  // test is not saying that didFailAccessControlCheck should be dispatched
  // synchronously, but is saying that even when a response is served
  // synchronously it should not lead to a crash.
  StartLoader(KURL("about:blank"), network::mojom::RequestMode::kCors);
  CallCheckpoint(2);
}

TEST(ThreadableLoaderCreatePreflightRequestTest, LexicographicalOrder) {
  ResourceRequest request;
  request.AddHttpHeaderField("Orange", "Orange");
  request.AddHttpHeaderField("Apple", "Red");
  request.AddHttpHeaderField("Kiwifruit", "Green");
  request.AddHttpHeaderField("Content-Type", "application/octet-stream");
  request.AddHttpHeaderField("Strawberry", "Red");

  std::unique_ptr<ResourceRequest> preflight =
      ThreadableLoader::CreateAccessControlPreflightRequestForTesting(request);

  EXPECT_EQ("apple,content-type,kiwifruit,orange,strawberry",
            preflight->HttpHeaderField("Access-Control-Request-Headers"));
}

TEST(ThreadableLoaderCreatePreflightRequestTest, ExcludeSimpleHeaders) {
  ResourceRequest request;
  request.AddHttpHeaderField("Accept", "everything");
  request.AddHttpHeaderField("Accept-Language", "everything");
  request.AddHttpHeaderField("Content-Language", "everything");
  request.AddHttpHeaderField("Save-Data", "on");

  std::unique_ptr<ResourceRequest> preflight =
      ThreadableLoader::CreateAccessControlPreflightRequestForTesting(request);

  // Do not emit empty-valued headers; an empty list of non-"CORS safelisted"
  // request headers should cause "Access-Control-Request-Headers:" to be
  // left out in the preflight request.
  EXPECT_EQ(g_null_atom,
            preflight->HttpHeaderField("Access-Control-Request-Headers"));
}

TEST(ThreadableLoaderCreatePreflightRequestTest,
     ExcludeSimpleContentTypeHeader) {
  ResourceRequest request;
  request.AddHttpHeaderField("Content-Type", "text/plain");

  std::unique_ptr<ResourceRequest> preflight =
      ThreadableLoader::CreateAccessControlPreflightRequestForTesting(request);

  // Empty list also; see comment in test above.
  EXPECT_EQ(g_null_atom,
            preflight->HttpHeaderField("Access-Control-Request-Headers"));
}

TEST(ThreadableLoaderCreatePreflightRequestTest, IncludeNonSimpleHeader) {
  ResourceRequest request;
  request.AddHttpHeaderField("X-Custom-Header", "foobar");

  std::unique_ptr<ResourceRequest> preflight =
      ThreadableLoader::CreateAccessControlPreflightRequestForTesting(request);

  EXPECT_EQ("x-custom-header",
            preflight->HttpHeaderField("Access-Control-Request-Headers"));
}

TEST(ThreadableLoaderCreatePreflightRequestTest,
     IncludeNonSimpleContentTypeHeader) {
  ResourceRequest request;
  request.AddHttpHeaderField("Content-Type", "application/octet-stream");

  std::unique_ptr<ResourceRequest> preflight =
      ThreadableLoader::CreateAccessControlPreflightRequestForTesting(request);

  EXPECT_EQ("content-type",
            preflight->HttpHeaderField("Access-Control-Request-Headers"));
}

}  // namespace

}  // namespace blink
