// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/throttling_url_loader.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace blink {
namespace {

GURL request_url = GURL("http://example.org");
GURL redirect_url = GURL("http://example.com");

class TestURLLoaderFactory : public network::mojom::URLLoaderFactory,
                             public network::mojom::URLLoader {
 public:
  TestURLLoaderFactory() {
    receiver_.Bind(factory_remote_.BindNewPipeAndPassReceiver());
    shared_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            factory_remote_.get());
  }
  TestURLLoaderFactory(const TestURLLoaderFactory&) = delete;
  TestURLLoaderFactory& operator=(const TestURLLoaderFactory&) = delete;

  ~TestURLLoaderFactory() override { shared_factory_->Detach(); }

  mojo::Remote<network::mojom::URLLoaderFactory>& factory_remote() {
    return factory_remote_;
  }
  mojo::Receiver<network::mojom::URLLoader>& url_loader_receiver() {
    return url_loader_receiver_;
  }
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory() {
    return shared_factory_;
  }

  size_t create_loader_and_start_called() const {
    return create_loader_and_start_called_;
  }

  const std::vector<std::string>& headers_removed_on_redirect() const {
    return headers_removed_on_redirect_;
  }

  const net::HttpRequestHeaders& headers_modified_on_redirect() const {
    return headers_modified_on_redirect_;
  }

  const net::HttpRequestHeaders& cors_exempt_headers_modified_on_redirect()
      const {
    return cors_exempt_headers_modified_on_redirect_;
  }

  size_t pause_reading_body_from_net_called() const {
    return pause_reading_body_from_net_called_;
  }

  size_t resume_reading_body_from_net_called() const {
    return resume_reading_body_from_net_called_;
  }

  void NotifyClientOnReceiveResponse() {
    client_remote_->OnReceiveResponse(network::mojom::URLResponseHead::New(),
                                      mojo::ScopedDataPipeConsumerHandle(),
                                      absl::nullopt);
  }

  void NotifyClientOnReceiveRedirect() {
    net::RedirectInfo info;
    info.new_url = redirect_url;
    client_remote_->OnReceiveRedirect(info,
                                      network::mojom::URLResponseHead::New());
  }

  void NotifyClientOnComplete(int error_code) {
    network::URLLoaderCompletionStatus data;
    data.error_code = error_code;
    client_remote_->OnComplete(data);
  }

  void CloseClientPipe() { client_remote_.reset(); }

  using OnCreateLoaderAndStartCallback = base::RepeatingCallback<void(
      const network::ResourceRequest& url_request)>;
  void set_on_create_loader_and_start(
      const OnCreateLoaderAndStartCallback& callback) {
    on_create_loader_and_start_callback_ = callback;
  }

 private:
  // network::mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    create_loader_and_start_called_++;

    url_loader_receiver_.reset();
    url_loader_receiver_.Bind(std::move(receiver));
    client_remote_.reset();
    client_remote_.Bind(std::move(client));

    if (on_create_loader_and_start_callback_)
      on_create_loader_and_start_callback_.Run(url_request);
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    NOTREACHED();
  }

  // network::mojom::URLLoader implementation.
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override {
    headers_removed_on_redirect_ = removed_headers;
    headers_modified_on_redirect_ = modified_headers;
    cors_exempt_headers_modified_on_redirect_ = modified_cors_exempt_headers;
  }

  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}

  void PauseReadingBodyFromNet() override {
    pause_reading_body_from_net_called_++;
  }

  void ResumeReadingBodyFromNet() override {
    resume_reading_body_from_net_called_++;
  }

  size_t create_loader_and_start_called_ = 0;
  std::vector<std::string> headers_removed_on_redirect_;
  net::HttpRequestHeaders headers_modified_on_redirect_;
  net::HttpRequestHeaders cors_exempt_headers_modified_on_redirect_;
  size_t pause_reading_body_from_net_called_ = 0;
  size_t resume_reading_body_from_net_called_ = 0;

  mojo::Receiver<network::mojom::URLLoaderFactory> receiver_{this};
  mojo::Receiver<network::mojom::URLLoader> url_loader_receiver_{this};
  mojo::Remote<network::mojom::URLLoaderFactory> factory_remote_;
  mojo::Remote<network::mojom::URLLoaderClient> client_remote_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory> shared_factory_;
  OnCreateLoaderAndStartCallback on_create_loader_and_start_callback_;
};

class TestURLLoaderClient : public network::mojom::URLLoaderClient {
 public:
  TestURLLoaderClient() = default;
  TestURLLoaderClient(const TestURLLoaderClient&) = delete;
  TestURLLoaderClient& operator=(const TestURLLoaderClient&) = delete;

  size_t on_received_response_called() const {
    return on_received_response_called_;
  }

  size_t on_received_redirect_called() const {
    return on_received_redirect_called_;
  }

  size_t on_complete_called() const { return on_complete_called_; }

  void set_on_received_redirect_callback(
      const base::RepeatingClosure& callback) {
    on_received_redirect_callback_ = callback;
  }

  void set_on_received_response_callback(base::OnceClosure callback) {
    on_received_response_callback_ = std::move(callback);
  }

  using OnCompleteCallback = base::OnceCallback<void(int error_code)>;
  void set_on_complete_callback(OnCompleteCallback callback) {
    on_complete_callback_ = std::move(callback);
  }

 private:
  // network::mojom::URLLoaderClient implementation:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
  }

  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle body,
      absl::optional<mojo_base::BigBuffer> cached_metadata) override {
    on_received_response_called_++;
    if (on_received_response_callback_)
      std::move(on_received_response_callback_).Run();
  }
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override {
    on_received_redirect_called_++;
    if (on_received_redirect_callback_)
      on_received_redirect_callback_.Run();
  }
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {}
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {}
  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    on_complete_called_++;
    if (on_complete_callback_)
      std::move(on_complete_callback_).Run(status.error_code);
  }

  size_t on_received_response_called_ = 0;
  size_t on_received_redirect_called_ = 0;
  size_t on_complete_called_ = 0;

  base::RepeatingClosure on_received_redirect_callback_;
  base::OnceClosure on_received_response_callback_;
  OnCompleteCallback on_complete_callback_;
};

class TestURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  TestURLLoaderThrottle() = default;
  explicit TestURLLoaderThrottle(base::OnceClosure destruction_notifier)
      : destruction_notifier_(std::move(destruction_notifier)) {}

  TestURLLoaderThrottle(const TestURLLoaderThrottle&) = delete;
  TestURLLoaderThrottle& operator=(const TestURLLoaderThrottle&) = delete;

  ~TestURLLoaderThrottle() override {
    if (destruction_notifier_)
      std::move(destruction_notifier_).Run();
  }

  using ThrottleCallback =
      base::RepeatingCallback<void(URLLoaderThrottle::Delegate* delegate,
                                   bool* defer)>;
  using ThrottleRedirectCallback = base::OnceCallback<void(
      blink::URLLoaderThrottle::Delegate* delegate,
      bool* defer,
      std::vector<std::string>* removed_headers,
      net::HttpRequestHeaders* modified_headers,
      net::HttpRequestHeaders* modified_cors_exempt_headers)>;

  size_t will_start_request_called() const {
    return will_start_request_called_;
  }
  size_t will_redirect_request_called() const {
    return will_redirect_request_called_;
  }
  size_t will_process_response_called() const {
    return will_process_response_called_;
  }
  size_t before_will_process_response_called() const {
    return before_will_process_response_called_;
  }

  size_t before_will_redirect_request_called() const {
    return before_will_redirect_request_called_;
  }

  GURL observed_response_url() const { return response_url_; }

  void set_will_start_request_callback(const ThrottleCallback& callback) {
    will_start_request_callback_ = callback;
  }

  void set_will_redirect_request_callback(ThrottleRedirectCallback callback) {
    will_redirect_request_callback_ = std::move(callback);
  }

  void set_will_process_response_callback(const ThrottleCallback& callback) {
    will_process_response_callback_ = callback;
  }

  void set_before_will_process_response_callback(
      const ThrottleCallback& callback) {
    before_will_process_response_callback_ = callback;
  }

  void set_before_will_redirect_request_callback(
      ThrottleRedirectCallback callback) {
    before_will_redirect_request_callback_ = std::move(callback);
  }

  void set_modify_url_in_will_start(const GURL& url) {
    modify_url_in_will_start_ = url;
  }

  Delegate* delegate() const { return delegate_; }

 private:
  // blink::URLLoaderThrottle implementation.
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override {
    will_start_request_called_++;
    if (!modify_url_in_will_start_.is_empty())
      request->url = modify_url_in_will_start_;

    if (will_start_request_callback_)
      will_start_request_callback_.Run(delegate_.get(), defer);
  }

  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* removed_headers,
      net::HttpRequestHeaders* modified_headers,
      net::HttpRequestHeaders* modified_cors_exempt_headers) override {
    will_redirect_request_called_++;
    if (will_redirect_request_callback_) {
      std::move(will_redirect_request_callback_)
          .Run(delegate_.get(), defer, removed_headers, modified_headers,
               modified_cors_exempt_headers);
    }
  }

  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override {
    will_process_response_called_++;
    if (will_process_response_callback_)
      will_process_response_callback_.Run(delegate_.get(), defer);
    response_url_ = response_url;
  }

  void BeforeWillProcessResponse(
      const GURL& response_url,
      const network::mojom::URLResponseHead& response_head,
      bool* defer) override {
    before_will_process_response_called_++;
    if (before_will_process_response_callback_)
      before_will_process_response_callback_.Run(delegate_.get(), defer);
  }

  void BeforeWillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* removed_headers,
      net::HttpRequestHeaders* modified_headers,
      net::HttpRequestHeaders* modified_cors_exempt_headers) override {
    before_will_redirect_request_called_++;
    if (before_will_redirect_request_callback_) {
      std::move(before_will_redirect_request_callback_)
          .Run(delegate_.get(), defer, removed_headers, modified_headers,
               modified_cors_exempt_headers);
    }
  }

  size_t will_start_request_called_ = 0;
  size_t will_redirect_request_called_ = 0;
  size_t will_process_response_called_ = 0;
  size_t before_will_process_response_called_ = 0;
  size_t before_will_redirect_request_called_ = 0;

  GURL response_url_;

  ThrottleCallback will_start_request_callback_;
  ThrottleRedirectCallback will_redirect_request_callback_;
  ThrottleCallback will_process_response_callback_;
  ThrottleCallback before_will_process_response_callback_;
  ThrottleRedirectCallback before_will_redirect_request_callback_;

  GURL modify_url_in_will_start_;

  base::OnceClosure destruction_notifier_;
};

class ThrottlingURLLoaderTest : public testing::Test {
 public:
  ThrottlingURLLoaderTest() = default;
  ThrottlingURLLoaderTest(const ThrottlingURLLoaderTest&) = delete;
  ThrottlingURLLoaderTest& operator=(const ThrottlingURLLoaderTest&) = delete;

  std::unique_ptr<ThrottlingURLLoader>& loader() { return loader_; }
  TestURLLoaderThrottle* throttle() const { return throttle_; }

 protected:
  // testing::Test implementation.
  void SetUp() override {
    auto throttle = std::make_unique<TestURLLoaderThrottle>(
        base::BindOnce(&ThrottlingURLLoaderTest::ResetThrottleRawPointer,
                       weak_factory_.GetWeakPtr()));

    throttle_ = throttle.get();

    throttles_.push_back(std::move(throttle));
  }

  void CreateLoaderAndStart(
      absl::optional<network::ResourceRequest::TrustedParams> trusted_params =
          absl::nullopt) {
    network::ResourceRequest request;
    request.url = request_url;
    request.trusted_params = std::move(trusted_params);
    loader_ = ThrottlingURLLoader::CreateLoaderAndStart(
        factory_.shared_factory(), std::move(throttles_), /*request_id=*/0,
        /*options=*/0, &request, &client_, TRAFFIC_ANNOTATION_FOR_TESTS,
        base::SingleThreadTaskRunner::GetCurrentDefault());
    factory_.factory_remote().FlushForTesting();
  }

  void ResetThrottleRawPointer() { throttle_ = nullptr; }

  // Be the first member so it is destroyed last.
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<ThrottlingURLLoader> loader_;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles_;

  TestURLLoaderFactory factory_;
  TestURLLoaderClient client_;

  // Owned by |throttles_| or |loader_|.
  raw_ptr<TestURLLoaderThrottle> throttle_ = nullptr;

  base::WeakPtrFactory<ThrottlingURLLoaderTest> weak_factory_{this};
};

TEST_F(ThrottlingURLLoaderTest, CancelBeforeStart) {
  throttle_->set_will_start_request_callback(base::BindLambdaForTesting(
      [](blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
        delegate->CancelWithError(net::ERR_ACCESS_DENIED);
      }));

  base::RunLoop run_loop;
  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop](int error) {
        EXPECT_EQ(net::ERR_ACCESS_DENIED, error);
        run_loop.Quit();
      }));

  CreateLoaderAndStart();
  run_loop.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  EXPECT_EQ(0u, factory_.create_loader_and_start_called());

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, DeferBeforeStart) {
  throttle_->set_will_start_request_callback(base::BindLambdaForTesting(
      [](blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
        *defer = true;
      }));

  base::RunLoop run_loop;
  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop](int error) {
        EXPECT_EQ(net::OK, error);
        run_loop.Quit();
      }));

  CreateLoaderAndStart();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  EXPECT_EQ(0u, factory_.create_loader_and_start_called());

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(0u, client_.on_complete_called());

  throttle_->delegate()->Resume();
  factory_.factory_remote().FlushForTesting();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());

  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());

  EXPECT_TRUE(
      throttle_->observed_response_url().EqualsIgnoringRef(request_url));

  EXPECT_EQ(1u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, ModifyHeaderInResumeBeforeStart) {
  throttle_->set_will_start_request_callback(
      base::BindRepeating([](blink::URLLoaderThrottle::Delegate* delegate,
                             bool* defer) { *defer = true; }));

  CreateLoaderAndStart();

  base::RunLoop run_loop;
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ("X-Test-Header-1: Foo\r\n\r\n",
                  url_request.headers.ToString());
        EXPECT_EQ("X-Test-Header-2: Bar\r\n\r\n",
                  url_request.cors_exempt_headers.ToString());
        quit_closure.Run();
      },
      run_loop.QuitClosure()));

  net::HttpRequestHeaders modified_headers;
  net::HttpRequestHeaders modified_cors_exempt_headers;
  modified_headers.SetHeader("X-Test-Header-1", "Foo");
  modified_cors_exempt_headers.SetHeader("X-Test-Header-2", "Bar");
  throttle_->delegate()->UpdateDeferredRequestHeaders(
      modified_headers, modified_cors_exempt_headers);
  throttle_->delegate()->Resume();

  run_loop.Run();
}

TEST_F(ThrottlingURLLoaderTest, ModifyURLBeforeStart) {
  throttle_->set_modify_url_in_will_start(GURL("http://example.org/foo"));

  CreateLoaderAndStart();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle_->will_redirect_request_called());
}

TEST_F(ThrottlingURLLoaderTest,
       CrossOriginRedirectBeforeStartWithIsolationInfo) {
  const GURL modified_url = GURL("https://example.org");

  throttle_->set_modify_url_in_will_start(modified_url);

  network::ResourceRequest::TrustedParams trusted_params;
  trusted_params.isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame,
      url::Origin::Create(request_url), url::Origin::Create(request_url),
      net::SiteForCookies());

  const auto expected_redirected_isolation_info =
      trusted_params.isolation_info.CreateForRedirect(
          url::Origin::Create(modified_url));
  ASSERT_FALSE(trusted_params.isolation_info.IsEqualForTesting(
      expected_redirected_isolation_info));

  CreateLoaderAndStart(std::move(trusted_params));

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, factory_.create_loader_and_start_called());

  base::RunLoop run_loop;
  factory_.set_on_create_loader_and_start(base::BindLambdaForTesting(
      [&](const network::ResourceRequest& url_request) {
        run_loop.Quit();

        ASSERT_TRUE(url_request.trusted_params);
        EXPECT_TRUE(
            url_request.trusted_params->isolation_info.IsEqualForTesting(
                expected_redirected_isolation_info));
      }));

  loader_->FollowRedirect({}, {}, {});

  run_loop.Run();
}

// Regression test for crbug.com/933538
TEST_F(ThrottlingURLLoaderTest, ModifyURLAndDeferRedirect) {
  throttle_->set_modify_url_in_will_start(GURL("http://example.org/foo"));
  throttle_->set_will_start_request_callback(
      base::BindRepeating([](blink::URLLoaderThrottle::Delegate* /* delegate */,
                             bool* defer) { *defer = true; }));
  base::RunLoop run_loop;
  throttle_->set_will_redirect_request_callback(base::BindLambdaForTesting(
      [&](blink::URLLoaderThrottle::Delegate* /* delegate */, bool* defer,
          std::vector<std::string>* /* removed_headers */,
          net::HttpRequestHeaders* /* modified_headers */,
          net::HttpRequestHeaders* /* modified_cors_exempt_headers */) {
        *defer = true;
        run_loop.Quit();
      }));

  CreateLoaderAndStart();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());

  throttle_->delegate()->Resume();
  run_loop.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());

  throttle_->delegate()->Resume();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());
  EXPECT_EQ(0u, factory_.create_loader_and_start_called());
  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(1u, client_.on_received_redirect_called());
  EXPECT_EQ(0u, client_.on_complete_called());
}

// Regression test for crbug.com/1053700.
TEST_F(ThrottlingURLLoaderTest,
       RedirectCallbackShouldNotBeCalledAfterDestruction) {
  throttle_->set_modify_url_in_will_start(GURL("http://example.org/foo"));
  base::RunLoop run_loop;
  bool called = false;
  throttle_->set_will_redirect_request_callback(base::BindLambdaForTesting(
      [&](blink::URLLoaderThrottle::Delegate* /* delegate */, bool* defer,
          std::vector<std::string>* /* removed_headers */,
          net::HttpRequestHeaders* /* modified_headers */,
          net::HttpRequestHeaders* /* modified_cors_exempt_headers */) {
        *defer = true;
        called = true;
      }));

  // We don't use CreateLoaderAndStart because we don't want to call
  // FlushForTesting().
  network::ResourceRequest request;
  request.url = request_url;
  loader_ = ThrottlingURLLoader::CreateLoaderAndStart(
      factory_.shared_factory(), std::move(throttles_), 0, 0, &request,
      &client_, TRAFFIC_ANNOTATION_FOR_TESTS,
      base::SingleThreadTaskRunner::GetCurrentDefault());

  loader_ = nullptr;

  run_loop.RunUntilIdle();
  EXPECT_FALSE(called);
}

TEST_F(ThrottlingURLLoaderTest, CancelBeforeRedirect) {
  throttle_->set_will_redirect_request_callback(base::BindLambdaForTesting(
      [](blink::URLLoaderThrottle::Delegate* delegate, bool* /* defer */,
         std::vector<std::string>* /* removed_headers */,
         net::HttpRequestHeaders* /* modified_headers */,
         net::HttpRequestHeaders* /* modified_cors_exempt_headers */) {
        delegate->CancelWithError(net::ERR_ACCESS_DENIED);
      }));

  base::RunLoop run_loop;
  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop](int error) {
        EXPECT_EQ(net::ERR_ACCESS_DENIED, error);
        run_loop.Quit();
      }));

  CreateLoaderAndStart();

  factory_.NotifyClientOnReceiveRedirect();

  run_loop.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, DeferBeforeRedirect) {
  base::RunLoop run_loop1;
  throttle_->set_will_redirect_request_callback(base::BindLambdaForTesting(
      [&run_loop1](
          blink::URLLoaderThrottle::Delegate* delegate, bool* defer,
          std::vector<std::string>* /* removed_headers */,
          net::HttpRequestHeaders* /* modified_headers */,
          net::HttpRequestHeaders* /* modified_cors_exempt_headers */) {
        *defer = true;
        run_loop1.Quit();
      }));

  base::RunLoop run_loop2;
  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop2](int error) {
        EXPECT_EQ(net::ERR_UNEXPECTED, error);
        run_loop2.Quit();
      }));

  CreateLoaderAndStart();

  factory_.NotifyClientOnReceiveRedirect();

  run_loop1.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  factory_.NotifyClientOnComplete(net::ERR_UNEXPECTED);

  base::RunLoop run_loop3;
  run_loop3.RunUntilIdle();

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(0u, client_.on_complete_called());

  throttle_->delegate()->Resume();
  run_loop2.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(1u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, ModifyHeadersBeforeRedirect) {
  throttle_->set_will_redirect_request_callback(base::BindLambdaForTesting(
      [](blink::URLLoaderThrottle::Delegate* delegate, bool* /* defer */,
         std::vector<std::string>* removed_headers,
         net::HttpRequestHeaders* modified_headers,
         net::HttpRequestHeaders* modified_cors_exempt_headers) {
        removed_headers->push_back("X-Test-Header-1");
        modified_headers->SetHeader("X-Test-Header-2", "Foo");
        modified_headers->SetHeader("X-Test-Header-3", "Throttle Value");
        modified_cors_exempt_headers->SetHeader("X-Test-Cors-Exempt-Header-1",
                                                "Bubble");
      }));

  client_.set_on_received_redirect_callback(base::BindLambdaForTesting([&]() {
    net::HttpRequestHeaders modified_headers;
    modified_headers.SetHeader("X-Test-Header-3", "Client Value");
    modified_headers.SetHeader("X-Test-Header-4", "Bar");
    net::HttpRequestHeaders modified_cors_exempt_headers;
    modified_cors_exempt_headers.SetHeader("X-Test-Cors-Exempt-Header-1",
                                           "Bobble");
    loader_->FollowRedirect({} /* removed_headers */,
                            std::move(modified_headers),
                            std::move(modified_cors_exempt_headers));
  }));

  CreateLoaderAndStart();
  factory_.NotifyClientOnReceiveRedirect();
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(factory_.headers_removed_on_redirect().empty());
  EXPECT_THAT(factory_.headers_removed_on_redirect(),
              testing::ElementsAre("X-Test-Header-1"));
  ASSERT_FALSE(factory_.headers_modified_on_redirect().IsEmpty());
  EXPECT_EQ(
      "X-Test-Header-2: Foo\r\n"
      "X-Test-Header-3: Client Value\r\n"
      "X-Test-Header-4: Bar\r\n\r\n",
      factory_.headers_modified_on_redirect().ToString());
  ASSERT_FALSE(factory_.cors_exempt_headers_modified_on_redirect().IsEmpty());
  EXPECT_EQ("X-Test-Cors-Exempt-Header-1: Bobble\r\n\r\n",
            factory_.cors_exempt_headers_modified_on_redirect().ToString());
}

TEST_F(ThrottlingURLLoaderTest, RemoveAcceptLanguageHeader) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {network::features::kReduceAcceptLanguageOriginTrial}, {});

  // Remove Accept-Language header if new header has no Accept-Language header.
  throttle_->set_will_redirect_request_callback(base::BindLambdaForTesting(
      [](blink::URLLoaderThrottle::Delegate* delegate, bool* /* defer */,
         std::vector<std::string>* removed_headers,
         net::HttpRequestHeaders* modified_headers,
         net::HttpRequestHeaders* modified_cors_exempt_headers) {
        modified_headers->SetHeader("Accept-Language", "en");
        modified_headers->SetHeader("X-Test-Header-1", "Foo");
        modified_cors_exempt_headers->SetHeader("X-Test-Cors-Exempt-Header-1",
                                                "Bubble");
      }));

  // New header without Accept-Language header.
  client_.set_on_received_redirect_callback(base::BindLambdaForTesting([&]() {
    net::HttpRequestHeaders modified_headers;
    modified_headers.SetHeader("X-Test-Header-2", "Bar");
    net::HttpRequestHeaders modified_cors_exempt_headers;
    modified_cors_exempt_headers.SetHeader("X-Test-Cors-Exempt-Header-1",
                                           "Bobble");
    loader_->FollowRedirect({"Accept-Language"} /* removed_headers */,
                            std::move(modified_headers),
                            std::move(modified_cors_exempt_headers));
  }));

  CreateLoaderAndStart();
  factory_.NotifyClientOnReceiveRedirect();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(factory_.headers_removed_on_redirect(),
              testing::ElementsAre("Accept-Language"));
  EXPECT_EQ(
      "X-Test-Header-1: Foo\r\n"
      "X-Test-Header-2: Bar\r\n\r\n",
      factory_.headers_modified_on_redirect().ToString());
  EXPECT_EQ("X-Test-Cors-Exempt-Header-1: Bobble\r\n\r\n",
            factory_.cors_exempt_headers_modified_on_redirect().ToString());
}

TEST_F(ThrottlingURLLoaderTest, ModifyHeaderInResumeBeforeRedirect) {
  base::RunLoop run_loop1;
  throttle_->set_will_redirect_request_callback(base::BindLambdaForTesting(
      [&run_loop1](blink::URLLoaderThrottle::Delegate* delegate, bool* defer,
                   std::vector<std::string>* removed_headers,
                   net::HttpRequestHeaders* modified_headers,
                   net::HttpRequestHeaders* modified_cors_exempt_headers) {
        *defer = true;
        run_loop1.Quit();
      }));

  CreateLoaderAndStart();
  factory_.NotifyClientOnReceiveRedirect();
  run_loop1.Run();

  net::HttpRequestHeaders modified_headers;
  net::HttpRequestHeaders modified_cors_exempt_headers;
  modified_headers.SetHeader("X-Test-Header-1", "Foo");
  modified_cors_exempt_headers.SetHeader("X-Test-Header-2", "Bar");
  throttle_->delegate()->UpdateDeferredRequestHeaders(
      modified_headers, modified_cors_exempt_headers);
  throttle_->delegate()->Resume();

  loader_->FollowRedirect({}, {}, {});

  base::RunLoop run_loop2;
  run_loop2.RunUntilIdle();

  EXPECT_EQ("X-Test-Header-1: Foo\r\n\r\n",
            factory_.headers_modified_on_redirect().ToString());
  EXPECT_EQ("X-Test-Header-2: Bar\r\n\r\n",
            factory_.cors_exempt_headers_modified_on_redirect().ToString());
}

TEST_F(ThrottlingURLLoaderTest, MultipleThrottlesModifyHeadersBeforeRedirect) {
  auto* throttle2 = new TestURLLoaderThrottle();
  throttles_.push_back(base::WrapUnique(throttle2));

  throttle_->set_will_redirect_request_callback(base::BindLambdaForTesting(
      [](blink::URLLoaderThrottle::Delegate* delegate, bool* /* defer */,
         std::vector<std::string>* removed_headers,
         net::HttpRequestHeaders* modified_headers,
         net::HttpRequestHeaders* modified_cors_exempt_headers) {
        removed_headers->push_back("X-Test-Header-0");
        removed_headers->push_back("X-Test-Header-1");
        modified_headers->SetHeader("X-Test-Header-3", "Foo");
        modified_headers->SetHeader("X-Test-Header-4", "Throttle1");
      }));

  throttle2->set_will_redirect_request_callback(base::BindLambdaForTesting(
      [](blink::URLLoaderThrottle::Delegate* delegate, bool* /* defer */,
         std::vector<std::string>* removed_headers,
         net::HttpRequestHeaders* modified_headers,
         net::HttpRequestHeaders* modified_cors_exempt_headers) {
        removed_headers->push_back("X-Test-Header-1");
        removed_headers->push_back("X-Test-Header-2");
        modified_headers->SetHeader("X-Test-Header-4", "Throttle2");
      }));

  client_.set_on_received_redirect_callback(base::BindLambdaForTesting(
      [&]() { loader_->FollowRedirect({}, {}, {}); }));

  CreateLoaderAndStart();
  factory_.NotifyClientOnReceiveRedirect();
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(factory_.headers_removed_on_redirect().empty());
  EXPECT_THAT(factory_.headers_removed_on_redirect(),
              testing::ElementsAre("X-Test-Header-0", "X-Test-Header-1",
                                   "X-Test-Header-2"));
  ASSERT_FALSE(factory_.headers_modified_on_redirect().IsEmpty());
  EXPECT_EQ(
      "X-Test-Header-3: Foo\r\n"
      "X-Test-Header-4: Throttle2\r\n\r\n",
      factory_.headers_modified_on_redirect().ToString());
}

TEST_F(ThrottlingURLLoaderTest, CancelBeforeResponse) {
  throttle_->set_will_process_response_callback(base::BindLambdaForTesting(
      [](blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
        delegate->CancelWithError(net::ERR_ACCESS_DENIED);
      }));

  base::RunLoop run_loop;
  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop](int error) {
        EXPECT_EQ(net::ERR_ACCESS_DENIED, error);
        run_loop.Quit();
      }));

  CreateLoaderAndStart();

  factory_.NotifyClientOnReceiveResponse();

  run_loop.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());

  EXPECT_TRUE(
      throttle_->observed_response_url().EqualsIgnoringRef(request_url));

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, DeferBeforeResponse) {
  base::RunLoop run_loop1;
  throttle_->set_will_process_response_callback(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
        *defer = true;
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop2](int error) {
        EXPECT_EQ(net::ERR_UNEXPECTED, error);
        run_loop2.Quit();
      }));

  CreateLoaderAndStart();

  factory_.NotifyClientOnReceiveResponse();

  run_loop1.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());

  EXPECT_TRUE(
      throttle_->observed_response_url().EqualsIgnoringRef(request_url));

  factory_.NotifyClientOnComplete(net::ERR_UNEXPECTED);

  base::RunLoop run_loop3;
  run_loop3.RunUntilIdle();

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(0u, client_.on_complete_called());

  throttle_->delegate()->Resume();
  run_loop2.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());

  EXPECT_TRUE(
      throttle_->observed_response_url().EqualsIgnoringRef(request_url));

  EXPECT_EQ(1u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, PipeClosure) {
  base::RunLoop run_loop;
  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop](int error) {
        EXPECT_EQ(net::ERR_ABORTED, error);
        run_loop.Quit();
      }));

  CreateLoaderAndStart();

  factory_.CloseClientPipe();

  run_loop.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, ResumeNoOpIfNotDeferred) {
  auto resume_callback = base::BindRepeating(
      [](blink::URLLoaderThrottle::Delegate* delegate, bool* /* defer */) {
        delegate->Resume();
        delegate->Resume();
      });
  throttle_->set_will_start_request_callback(resume_callback);
  throttle_->set_will_process_response_callback(std::move(resume_callback));
  throttle_->set_will_redirect_request_callback(base::BindLambdaForTesting(
      [](blink::URLLoaderThrottle::Delegate* delegate, bool* /* defer */,
         std::vector<std::string>* /* removed_headers */,
         net::HttpRequestHeaders* /* modified_headers */,
         net::HttpRequestHeaders* /* modified_cors_exempt_headers */) {
        delegate->Resume();
        delegate->Resume();
      }));

  base::RunLoop run_loop;
  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop](int error) {
        EXPECT_EQ(net::OK, error);
        run_loop.Quit();
      }));

  CreateLoaderAndStart();
  factory_.NotifyClientOnReceiveRedirect();
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle_->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());

  EXPECT_TRUE(
      throttle_->observed_response_url().EqualsIgnoringRef(redirect_url));

  EXPECT_EQ(1u, client_.on_received_response_called());
  EXPECT_EQ(1u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, CancelNoOpIfAlreadyCanceled) {
  throttle_->set_will_start_request_callback(base::BindRepeating(
      [](blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
        delegate->CancelWithError(net::ERR_ACCESS_DENIED);
        delegate->CancelWithError(net::ERR_UNEXPECTED);
      }));

  base::RunLoop run_loop;
  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop](int error) {
        EXPECT_EQ(net::ERR_ACCESS_DENIED, error);
        run_loop.Quit();
      }));

  CreateLoaderAndStart();
  throttle_->delegate()->CancelWithError(net::ERR_INVALID_ARGUMENT);
  run_loop.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  EXPECT_EQ(0u, factory_.create_loader_and_start_called());

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, ResumeNoOpIfAlreadyCanceled) {
  throttle_->set_will_process_response_callback(base::BindLambdaForTesting(
      [](blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
        delegate->CancelWithError(net::ERR_ACCESS_DENIED);
        delegate->Resume();
      }));

  base::RunLoop run_loop1;
  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop1](int error) {
        EXPECT_EQ(net::ERR_ACCESS_DENIED, error);
        run_loop1.Quit();
      }));

  CreateLoaderAndStart();

  factory_.NotifyClientOnReceiveResponse();

  run_loop1.Run();

  throttle_->delegate()->Resume();

  base::RunLoop run_loop2;
  run_loop2.RunUntilIdle();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());

  EXPECT_TRUE(
      throttle_->observed_response_url().EqualsIgnoringRef(request_url));

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, MultipleThrottlesBasicSupport) {
  throttles_.emplace_back(std::make_unique<TestURLLoaderThrottle>());
  auto* throttle2 =
      static_cast<TestURLLoaderThrottle*>(throttles_.back().get());
  CreateLoaderAndStart();
  factory_.NotifyClientOnReceiveResponse();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle2->will_start_request_called());
}

TEST_F(ThrottlingURLLoaderTest, BlockWithOneOfMultipleThrottles) {
  throttles_.emplace_back(std::make_unique<TestURLLoaderThrottle>());
  auto* throttle2 =
      static_cast<TestURLLoaderThrottle*>(throttles_.back().get());
  throttle2->set_will_start_request_callback(base::BindLambdaForTesting(
      [](blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
        *defer = true;
      }));

  base::RunLoop loop;
  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&loop](int error) {
        EXPECT_EQ(net::OK, error);
        loop.Quit();
      }));

  CreateLoaderAndStart();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle2->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle2->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle2->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());
  EXPECT_EQ(0u, throttle2->will_process_response_called());

  EXPECT_EQ(0u, factory_.create_loader_and_start_called());

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(0u, client_.on_complete_called());

  throttle2->delegate()->Resume();
  factory_.factory_remote().FlushForTesting();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());

  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  loop.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle2->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle2->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle2->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());
  EXPECT_EQ(1u, throttle2->will_process_response_called());

  EXPECT_TRUE(
      throttle_->observed_response_url().EqualsIgnoringRef(request_url));
  EXPECT_TRUE(
      throttle2->observed_response_url().EqualsIgnoringRef(request_url));

  EXPECT_EQ(1u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, BlockWithMultipleThrottles) {
  throttles_.emplace_back(std::make_unique<TestURLLoaderThrottle>());
  auto* throttle2 =
      static_cast<TestURLLoaderThrottle*>(throttles_.back().get());

  // Defers a request on both throttles.
  throttle_->set_will_start_request_callback(base::BindLambdaForTesting(
      [](blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
        *defer = true;
      }));
  throttle2->set_will_start_request_callback(base::BindLambdaForTesting(
      [](blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
        *defer = true;
      }));

  base::RunLoop loop;
  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&loop](int error) {
        EXPECT_EQ(net::OK, error);
        loop.Quit();
      }));

  CreateLoaderAndStart();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle2->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle2->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle2->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());
  EXPECT_EQ(0u, throttle2->will_process_response_called());

  EXPECT_EQ(0u, factory_.create_loader_and_start_called());

  EXPECT_EQ(0u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(0u, client_.on_complete_called());

  throttle_->delegate()->Resume();

  // Should still not have started because there's |throttle2| is still blocking
  // the request.
  factory_.factory_remote().FlushForTesting();
  EXPECT_EQ(0u, factory_.create_loader_and_start_called());

  throttle2->delegate()->Resume();

  // Now it should have started.
  factory_.factory_remote().FlushForTesting();
  EXPECT_EQ(1u, factory_.create_loader_and_start_called());

  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  loop.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle2->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle2->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle2->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());
  EXPECT_EQ(1u, throttle2->will_process_response_called());

  EXPECT_TRUE(
      throttle_->observed_response_url().EqualsIgnoringRef(request_url));
  EXPECT_TRUE(
      throttle2->observed_response_url().EqualsIgnoringRef(request_url));

  EXPECT_EQ(1u, client_.on_received_response_called());
  EXPECT_EQ(0u, client_.on_received_redirect_called());
  EXPECT_EQ(1u, client_.on_complete_called());
}

TEST_F(ThrottlingURLLoaderTest, PauseResumeReadingBodyFromNet) {
  throttles_.emplace_back(std::make_unique<TestURLLoaderThrottle>());
  auto* throttle2 =
      static_cast<TestURLLoaderThrottle*>(throttles_.back().get());

  // Test that it is okay to call delegate->PauseReadingBodyFromNet() even
  // before the loader is created.
  throttle_->set_will_start_request_callback(base::BindLambdaForTesting(
      [](blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
        delegate->PauseReadingBodyFromNet();
        *defer = true;
      }));
  throttle2->set_will_start_request_callback(base::BindLambdaForTesting(
      [](blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
        delegate->PauseReadingBodyFromNet();
      }));

  CreateLoaderAndStart();

  throttle_->delegate()->Resume();

  factory_.factory_remote().FlushForTesting();
  EXPECT_EQ(1u, factory_.create_loader_and_start_called());

  // Make sure all URLLoader calls before this point are delivered to the impl
  // side.
  factory_.url_loader_receiver().FlushForTesting();

  // Although there were two calls to delegate->PauseReadingBodyFromNet(), only
  // one URLLoader::PauseReadingBodyFromNet() Mojo call was made.
  EXPECT_EQ(1u, factory_.pause_reading_body_from_net_called());
  EXPECT_EQ(0u, factory_.resume_reading_body_from_net_called());

  // Reading body from network is still paused by |throttle2|. Calling
  // ResumeReadingBodyFromNet() on |throttle_| shouldn't have any effect.
  throttle_->delegate()->ResumeReadingBodyFromNet();
  factory_.url_loader_receiver().FlushForTesting();
  EXPECT_EQ(1u, factory_.pause_reading_body_from_net_called());
  EXPECT_EQ(0u, factory_.resume_reading_body_from_net_called());

  // Even if we call ResumeReadingBodyFromNet() on |throttle_| one more time.
  throttle_->delegate()->ResumeReadingBodyFromNet();
  factory_.url_loader_receiver().FlushForTesting();
  EXPECT_EQ(1u, factory_.pause_reading_body_from_net_called());
  EXPECT_EQ(0u, factory_.resume_reading_body_from_net_called());

  throttle2->delegate()->ResumeReadingBodyFromNet();
  factory_.url_loader_receiver().FlushForTesting();
  EXPECT_EQ(1u, factory_.pause_reading_body_from_net_called());
  EXPECT_EQ(1u, factory_.resume_reading_body_from_net_called());
}

TEST_F(ThrottlingURLLoaderTest,
       DestroyingThrottlingURLLoaderInDelegateCall_Response) {
  base::RunLoop run_loop1;
  throttle_->set_will_process_response_callback(base::BindLambdaForTesting(
      [&run_loop1](blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
        *defer = true;
        run_loop1.Quit();
      }));

  base::RunLoop run_loop2;
  client_.set_on_received_response_callback(base::BindLambdaForTesting([&]() {
    // Destroy the ThrottlingURLLoader while inside a delegate call from a
    // throttle.
    loader().reset();

    // The throttle should stay alive.
    EXPECT_NE(nullptr, throttle());

    run_loop2.Quit();
  }));

  CreateLoaderAndStart();

  factory_.NotifyClientOnReceiveResponse();

  run_loop1.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());

  EXPECT_TRUE(
      throttle_->observed_response_url().EqualsIgnoringRef(request_url));

  throttle_->delegate()->Resume();
  run_loop2.Run();

  // The ThrottlingURLLoader should be gone.
  EXPECT_EQ(nullptr, loader_);
  // The throttle should stay alive and destroyed later.
  EXPECT_NE(nullptr, throttle_);

  task_environment_.RunUntilIdle();
  EXPECT_EQ(nullptr, throttle_.get());
}

// Regression test for crbug.com/833292.
TEST_F(ThrottlingURLLoaderTest,
       DestroyingThrottlingURLLoaderInDelegateCall_Redirect) {
  base::RunLoop run_loop1;
  throttle_->set_will_redirect_request_callback(base::BindLambdaForTesting(
      [&run_loop1](
          blink::URLLoaderThrottle::Delegate* delegate, bool* defer,
          std::vector<std::string>* /* removed_headers */,
          net::HttpRequestHeaders* /* modified_headers */,
          net::HttpRequestHeaders* /* modified_cors_exempt_headers */) {
        *defer = true;
        run_loop1.Quit();
      }));

  base::RunLoop run_loop2;
  client_.set_on_received_redirect_callback(base::BindRepeating(
      [](ThrottlingURLLoaderTest* test,
         const base::RepeatingClosure& quit_closure) {
        // Destroy the ThrottlingURLLoader while inside a delegate call from a
        // throttle.
        test->loader().reset();

        // The throttle should stay alive.
        EXPECT_NE(nullptr, test->throttle());

        quit_closure.Run();
      },
      base::Unretained(this), run_loop2.QuitClosure()));

  CreateLoaderAndStart();

  factory_.NotifyClientOnReceiveRedirect();

  run_loop1.Run();

  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  throttle_->delegate()->Resume();
  run_loop2.Run();

  // The ThrottlingURLLoader should be gone.
  EXPECT_EQ(nullptr, loader_);
  // The throttle should stay alive and destroyed later.
  EXPECT_NE(nullptr, throttle_);

  task_environment_.RunUntilIdle();
  EXPECT_EQ(nullptr, throttle_.get());
}

// Call RestartWithFlags() from a single throttle while processing
// BeforeWillProcessResponse().
TEST_F(ThrottlingURLLoaderTest, RestartWithFlags) {
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  base::RunLoop run_loop3;

  // Check that the initial loader uses the default load flags (0).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(0, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  // Restart the request when processing BeforeWillProcessResponse(), using
  // different load flags (1).
  throttle_->set_before_will_process_response_callback(
      base::BindRepeating([](blink::URLLoaderThrottle::Delegate* delegate,
                             bool* defer) { delegate->RestartWithFlags(1); }));

  CreateLoaderAndStart();

  run_loop1.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  // The next time we intercept CreateLoaderAndStart() should be for the
  // restarted request (load flags of 1).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(1, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop2.QuitClosure()));

  factory_.NotifyClientOnReceiveResponse();

  run_loop2.Run();

  // Now that the restarted request has been made, clear
  // BeforeWillProcessResponse() so it doesn't restart the request yet again.
  throttle_->set_before_will_process_response_callback(
      TestURLLoaderThrottle::ThrottleCallback());

  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop3](int error) {
        EXPECT_EQ(net::OK, error);
        run_loop3.Quit();
      }));

  // Complete the response.
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop3.Run();

  EXPECT_EQ(2u, factory_.create_loader_and_start_called());
  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(2u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());
}

// Call RestartWithFlags() from a single throttle after having deferred
// BeforeWillProcessResponse().
TEST_F(ThrottlingURLLoaderTest, DeferThenRestartWithFlags) {
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  base::RunLoop run_loop3;
  base::RunLoop run_loop4;

  // Check that the initial loader uses the default load flags (0).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(0, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  // Defer BeforeWillProcessResponse().
  throttle_->set_before_will_process_response_callback(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
        *defer = true;
        quit_closure.Run();
      },
      run_loop2.QuitClosure()));

  CreateLoaderAndStart();

  run_loop1.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  factory_.NotifyClientOnReceiveResponse();

  run_loop2.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  // The next time we intercept CreateLoaderAndStart() should be for the
  // restarted request (load flags of 1).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(1, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop3.QuitClosure()));

  throttle_->delegate()->RestartWithFlags(1);
  throttle_->delegate()->Resume();

  run_loop3.Run();

  // Now that the restarted request has been made, clear
  // BeforeWillProcessResponse().
  throttle_->set_before_will_process_response_callback(
      TestURLLoaderThrottle::ThrottleCallback());

  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop4](int error) {
        EXPECT_EQ(net::OK, error);
        run_loop4.Quit();
      }));

  // Complete the response.
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop4.Run();

  EXPECT_EQ(2u, factory_.create_loader_and_start_called());
  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(2u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());
}

// Call RestartWithFlags() from a multiple throttles while processing
// BeforeWillProcessResponse(). Ensures that the request is restarted exactly
// once, using the combination of all additional load flags.
TEST_F(ThrottlingURLLoaderTest, MultipleRestartWithFlags) {
  // Create two additional TestURLLoaderThrottles for a total of 3, and keep
  // local unowned pointers to them in |throttles|.
  std::vector<TestURLLoaderThrottle*> throttles;
  ASSERT_EQ(1u, throttles_.size());
  throttles.push_back(throttle_);
  for (size_t i = 0; i < 2u; ++i) {
    auto throttle = std::make_unique<TestURLLoaderThrottle>();
    throttles.push_back(throttle.get());
    throttles_.push_back(std::move(throttle));
  }
  ASSERT_EQ(3u, throttles_.size());
  ASSERT_EQ(3u, throttles.size());

  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  base::RunLoop run_loop3;

  // Check that the initial loader uses the default load flags (0).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(0, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  // Have two of the three throttles restart whe processing
  // BeforeWillProcessResponse(), using
  // different load flags (2 and 8).
  throttles[0]->set_before_will_process_response_callback(
      base::BindRepeating([](blink::URLLoaderThrottle::Delegate* delegate,
                             bool* defer) { delegate->RestartWithFlags(2); }));
  throttles[2]->set_before_will_process_response_callback(
      base::BindRepeating([](blink::URLLoaderThrottle::Delegate* delegate,
                             bool* defer) { delegate->RestartWithFlags(8); }));

  CreateLoaderAndStart();

  run_loop1.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(0u, throttle->before_will_process_response_called());
    EXPECT_EQ(0u, throttle->will_process_response_called());
  }

  // The next time we intercept CreateLoaderAndStart() should be for the
  // restarted request (load flags of 10 = (2 | 8)).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(10, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop2.QuitClosure()));

  factory_.NotifyClientOnReceiveResponse();

  run_loop2.Run();

  // Now that the restarted request has been made, clear
  // BeforeWillProcessResponse() so it doesn't restart the request yet again.
  for (auto* throttle : throttles) {
    throttle->set_before_will_process_response_callback(
        TestURLLoaderThrottle::ThrottleCallback());
  }

  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop3](int error) {
        EXPECT_EQ(net::OK, error);
        run_loop3.Quit();
      }));

  // Complete the response.
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop3.Run();

  EXPECT_EQ(2u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(2u, throttle->before_will_process_response_called());
    EXPECT_EQ(1u, throttle->will_process_response_called());
  }
}

// Call RestartWithFlags() from multiple throttles after having deferred
// BeforeWillProcessResponse() in each. Ensures that the request is started
// exactly once, using the combination of all additional load flags.
TEST_F(ThrottlingURLLoaderTest, MultipleDeferThenRestartWithFlags) {
  // Create two additional TestURLLoaderThrottles for a total of 3, and keep
  // local unowned pointers to them in |throttles|.
  std::vector<TestURLLoaderThrottle*> throttles;
  ASSERT_EQ(1u, throttles_.size());
  throttles.push_back(throttle_);
  for (size_t i = 0; i < 2u; ++i) {
    auto throttle = std::make_unique<TestURLLoaderThrottle>();
    throttles.push_back(throttle.get());
    throttles_.push_back(std::move(throttle));
  }

  ASSERT_EQ(3u, throttles_.size());
  ASSERT_EQ(3u, throttles.size());

  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  base::RunLoop run_loop3;
  base::RunLoop run_loop4;

  // Check that the initial loader uses the default load flags (0).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(0, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  // Have all of the throttles defer. Once they have all been deferred, quit
  // run_loop2.
  int throttle_counter = 0;
  for (auto* throttle : throttles) {
    throttle->set_before_will_process_response_callback(base::BindRepeating(
        [](const base::RepeatingClosure& quit_closure, int* count,
           blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
          *defer = true;
          if (++(*count) == 3) {
            quit_closure.Run();
          }
        },
        run_loop2.QuitClosure(), &throttle_counter));
  }

  CreateLoaderAndStart();

  run_loop1.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(0u, throttle->before_will_process_response_called());
    EXPECT_EQ(0u, throttle->will_process_response_called());
  }

  factory_.NotifyClientOnReceiveResponse();

  run_loop2.Run();

  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(1u, throttle->before_will_process_response_called());
    EXPECT_EQ(0u, throttle->will_process_response_called());
  }

  // The next time we intercept CreateLoaderAndStart() should be for the
  // restarted request (load flags of 1 | 2 | 4).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(7, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop3.QuitClosure()));

  int next_load_flag = 1;
  for (auto* throttle : throttles) {
    throttle->delegate()->RestartWithFlags(next_load_flag);
    throttle->delegate()->Resume();
    next_load_flag <<= 1;
  }

  run_loop3.Run();

  // Now that the restarted request has been made, clear
  // BeforeWillProcessResponse().
  for (auto* throttle : throttles) {
    throttle->set_before_will_process_response_callback(
        TestURLLoaderThrottle::ThrottleCallback());
  }

  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop4](int error) {
        EXPECT_EQ(net::OK, error);
        run_loop4.Quit();
      }));

  // Complete the response.
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop4.Run();

  EXPECT_EQ(2u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(2u, throttle->before_will_process_response_called());
    EXPECT_EQ(1u, throttle->will_process_response_called());
  }
}

// Call RestartWithFlags() from multiple throttles -- two while deferred, and
// one while processing BeforeWillProcessResponse(). Ensures that the request is
// restarted exactly once, using the combination of all additional load flags.
TEST_F(ThrottlingURLLoaderTest, MultipleRestartWithFlagsDeferAndSync) {
  // Create two additional TestURLLoaderThrottles for a total of 3, and keep
  // local unowned pointers to them in |throttles|.
  std::vector<TestURLLoaderThrottle*> throttles;
  ASSERT_EQ(1u, throttles_.size());
  throttles.push_back(throttle_);
  for (size_t i = 0; i < 2u; ++i) {
    auto throttle = std::make_unique<TestURLLoaderThrottle>();
    throttles.push_back(throttle.get());
    throttles_.push_back(std::move(throttle));
  }

  ASSERT_EQ(3u, throttles_.size());
  ASSERT_EQ(3u, throttles.size());

  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  base::RunLoop run_loop3;
  base::RunLoop run_loop4;

  // Check that the initial loader uses the default load flags (0).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(0, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  // Have two of the throttles defer, and one call restart
  // synchronously. Once all are run, quit run_loop2.
  int throttle_counter = 0;
  for (size_t i = 0; i < 2u; ++i) {
    throttles[i]->set_before_will_process_response_callback(base::BindRepeating(
        [](const base::RepeatingClosure& quit_closure, int* count,
           blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
          *defer = true;
          if (++(*count) == 3) {
            quit_closure.Run();
          }
        },
        run_loop2.QuitClosure(), &throttle_counter));
  }
  throttles[2]->set_before_will_process_response_callback(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure, int* count,
         blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
        delegate->RestartWithFlags(4);
        if (++(*count) == 3) {
          quit_closure.Run();
        }
      },
      run_loop2.QuitClosure(), &throttle_counter));

  CreateLoaderAndStart();

  run_loop1.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(0u, throttle->before_will_process_response_called());
    EXPECT_EQ(0u, throttle->will_process_response_called());
  }

  factory_.NotifyClientOnReceiveResponse();

  run_loop2.Run();

  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(1u, throttle->before_will_process_response_called());
    EXPECT_EQ(0u, throttle->will_process_response_called());
  }

  // The next time we intercept CreateLoaderAndStart() should be for the
  // restarted request (load flags of 1 | 2 | 4).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(7, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop3.QuitClosure()));

  int next_load_flag = 1;
  for (auto* throttle : throttles) {
    throttle->delegate()->RestartWithFlags(next_load_flag);
    throttle->delegate()->Resume();
    next_load_flag <<= 1;
  }

  run_loop3.Run();

  // Now that the restarted request has been made, clear
  // BeforeWillProcessResponse().
  for (auto* throttle : throttles) {
    throttle->set_before_will_process_response_callback(
        TestURLLoaderThrottle::ThrottleCallback());
  }

  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop4](int error) {
        EXPECT_EQ(net::OK, error);
        run_loop4.Quit();
      }));

  // Complete the response.
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop4.Run();

  EXPECT_EQ(2u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(2u, throttle->before_will_process_response_called());
    EXPECT_EQ(1u, throttle->will_process_response_called());
  }
}

// Call RestartWithURLResetAndFlags() from a single throttle while processing
// BeforeWillProcessResponse(), and verify that it restarts with the original
// URL.
TEST_F(ThrottlingURLLoaderTest, RestartWithURLResetAndFlags) {
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  base::RunLoop run_loop3;

  // URL for internal redirect.
  GURL modified_url = GURL("www.example.uk.com");
  throttle_->set_modify_url_in_will_start(modified_url);

  // Check that the initial loader uses the default load flags (0).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(0, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  // Set the client to actually follow redirects to allow URL resetting to
  // occur.
  client_.set_on_received_redirect_callback(base::BindLambdaForTesting([&]() {
    net::HttpRequestHeaders modified_headers;
    loader_->FollowRedirect({} /* removed_headers */,
                            std::move(modified_headers),
                            {} /* modified_cors_exempt_headers */);
  }));

  // Restart the request when processing BeforeWillProcessResponse(), using
  // different load flags (1), and an URL reset.
  throttle_->set_before_will_process_response_callback(base::BindRepeating(
      [](blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
        delegate->RestartWithURLResetAndFlags(1);
      }));

  CreateLoaderAndStart();

  run_loop1.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());
  EXPECT_EQ(throttle_->observed_response_url(), modified_url);

  // The next time we intercept CreateLoaderAndStart() should be for the
  // restarted request (load flags of 1).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(1, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop2.QuitClosure()));

  factory_.NotifyClientOnReceiveResponse();

  run_loop2.Run();

  // Now that the restarted request has been made, clear
  // BeforeWillProcessResponse() so it doesn't restart the request yet again.
  throttle_->set_before_will_process_response_callback(
      TestURLLoaderThrottle::ThrottleCallback());

  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop3](int error) {
        EXPECT_EQ(net::OK, error);
        run_loop3.Quit();
      }));

  // Complete the response.
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop3.Run();

  EXPECT_EQ(2u, factory_.create_loader_and_start_called());
  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle_->will_redirect_request_called());
  EXPECT_EQ(2u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());
  EXPECT_EQ(throttle_->observed_response_url(), request_url);
}

// Call RestartWithURLResetAndFlags() from a single throttle after having
// deferred BeforeWillProcessResponse(), and verify it uses the original URL.
TEST_F(ThrottlingURLLoaderTest, DeferThenRestartWithURLResetAndFlags) {
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  base::RunLoop run_loop3;
  base::RunLoop run_loop4;

  // URL for internal redirect.
  GURL modified_url = GURL("www.example.uk.com");
  throttle_->set_modify_url_in_will_start(modified_url);

  // Check that the initial loader uses the default load flags (0).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(0, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  // Set the client to actually follow redirects to allow URL resetting to
  // occur.
  client_.set_on_received_redirect_callback(base::BindLambdaForTesting([&]() {
    net::HttpRequestHeaders modified_headers;
    loader_->FollowRedirect({} /* removed_headers */,
                            std::move(modified_headers),
                            {} /* modified_cors_exempt_headers */);
  }));

  // Defer BeforeWillProcessResponse().
  throttle_->set_before_will_process_response_callback(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
        *defer = true;
        quit_closure.Run();
      },
      run_loop2.QuitClosure()));

  CreateLoaderAndStart();

  run_loop1.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());
  EXPECT_EQ(throttle_->observed_response_url(), modified_url);

  factory_.NotifyClientOnReceiveResponse();

  run_loop2.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  // The next time we intercept CreateLoaderAndStart() should be for the
  // restarted request (load flags of 1).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(1, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop3.QuitClosure()));

  throttle_->delegate()->RestartWithURLResetAndFlags(1);
  throttle_->delegate()->Resume();

  run_loop3.Run();

  // Now that the restarted request has been made, clear
  // BeforeWillProcessResponse().
  throttle_->set_before_will_process_response_callback(
      TestURLLoaderThrottle::ThrottleCallback());

  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop4](int error) {
        EXPECT_EQ(net::OK, error);
        run_loop4.Quit();
      }));

  // Complete the response.
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop4.Run();

  EXPECT_EQ(2u, factory_.create_loader_and_start_called());
  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle_->will_redirect_request_called());
  EXPECT_EQ(2u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());
  EXPECT_EQ(throttle_->observed_response_url(), request_url);
}

// Call RestartWithURLResetFlags() from a multiple throttles while processing
// BeforeWillProcessResponse(). Ensures that the request is restarted exactly
// once, using the combination of all additional load flags, and with the
// original URL.
TEST_F(ThrottlingURLLoaderTest, MultipleRestartWithURLResetAndFlags) {
  // Create two additional TestURLLoaderThrottles for a total of 3, and keep
  // local unowned pointers to them in |throttles|.
  std::vector<TestURLLoaderThrottle*> throttles;
  ASSERT_EQ(1u, throttles_.size());
  throttles.push_back(throttle_);
  for (size_t i = 0; i < 2u; ++i) {
    auto throttle = std::make_unique<TestURLLoaderThrottle>();
    throttles.push_back(throttle.get());
    throttles_.push_back(std::move(throttle));
  }
  ASSERT_EQ(3u, throttles_.size());
  ASSERT_EQ(3u, throttles.size());

  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  base::RunLoop run_loop3;

  // URL for internal redirect.
  GURL modified_url = GURL("www.example.uk.com");
  throttle_->set_modify_url_in_will_start(modified_url);

  // Check that the initial loader uses the default load flags (0).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(0, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  // Set the client to actually follow redirects to allow URL resetting to
  // occur.
  client_.set_on_received_redirect_callback(base::BindLambdaForTesting([&]() {
    net::HttpRequestHeaders modified_headers;
    loader_->FollowRedirect({} /* removed_headers */,
                            std::move(modified_headers),
                            {} /* modified_cors_exempt_headers */);
  }));

  // Have two of the three throttles restart whe processing
  // BeforeWillProcessResponse(), using
  // different load flags (2 and 8), but both with URL resets.
  throttles[0]->set_before_will_process_response_callback(base::BindRepeating(
      [](blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
        delegate->RestartWithURLResetAndFlags(2);
      }));
  throttles[2]->set_before_will_process_response_callback(base::BindRepeating(
      [](blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
        delegate->RestartWithURLResetAndFlags(8);
      }));

  CreateLoaderAndStart();

  run_loop1.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(0u, throttle->before_will_process_response_called());
    EXPECT_EQ(0u, throttle->will_process_response_called());
    EXPECT_EQ(throttle_->observed_response_url(), modified_url);
  }

  // The next time we intercept CreateLoaderAndStart() should be for the
  // restarted request (load flags of 10 = (2 | 8)).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(10, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop2.QuitClosure()));

  factory_.NotifyClientOnReceiveResponse();

  run_loop2.Run();

  // Now that the restarted request has been made, clear
  // BeforeWillProcessResponse() so it doesn't restart the request yet again.
  for (auto* throttle : throttles) {
    throttle->set_before_will_process_response_callback(
        TestURLLoaderThrottle::ThrottleCallback());
  }

  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop3](int error) {
        EXPECT_EQ(net::OK, error);
        run_loop3.Quit();
      }));

  // Complete the response.
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop3.Run();

  EXPECT_EQ(2u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(1u, throttle->will_redirect_request_called());
    EXPECT_EQ(2u, throttle->before_will_process_response_called());
    EXPECT_EQ(1u, throttle->will_process_response_called());
    EXPECT_EQ(throttle_->observed_response_url(), request_url);
  }
}

// Call RestartWithURLResetAndFlags() from multiple throttles after having
// deferred BeforeWillProcessResponse() in each. Ensures that the request is
// started exactly once, using the combination of all additional load flags,
// and with the original URL.
TEST_F(ThrottlingURLLoaderTest, MultipleDeferThenRestartWithURLResetAndFlags) {
  // Create two additional TestURLLoaderThrottles for a total of 3, and keep
  // local unowned pointers to them in |throttles|.
  std::vector<TestURLLoaderThrottle*> throttles;
  ASSERT_EQ(1u, throttles_.size());
  throttles.push_back(throttle_);
  for (size_t i = 0; i < 2u; ++i) {
    auto throttle = std::make_unique<TestURLLoaderThrottle>();
    throttles.push_back(throttle.get());
    throttles_.push_back(std::move(throttle));
  }

  ASSERT_EQ(3u, throttles_.size());
  ASSERT_EQ(3u, throttles.size());

  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  base::RunLoop run_loop3;
  base::RunLoop run_loop4;

  // URL for internal redirect.
  GURL modified_url = GURL("www.example.uk.com");
  throttle_->set_modify_url_in_will_start(modified_url);

  // Check that the initial loader uses the default load flags (0).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(0, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  // Set the client to actually follow redirects to allow URL resetting to
  // occur.
  client_.set_on_received_redirect_callback(base::BindLambdaForTesting([&]() {
    net::HttpRequestHeaders modified_headers;
    loader_->FollowRedirect({} /* removed_headers */,
                            std::move(modified_headers),
                            {} /* modified_cors_exempt_headers */);
  }));

  // Have all of the throttles defer. Once they have all been deferred, quit
  // run_loop2.
  int throttle_counter = 0;
  for (auto* throttle : throttles) {
    throttle->set_before_will_process_response_callback(base::BindRepeating(
        [](const base::RepeatingClosure& quit_closure, int* count,
           blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
          *defer = true;
          if (++(*count) == 3) {
            quit_closure.Run();
          }
        },
        run_loop2.QuitClosure(), &throttle_counter));
  }

  CreateLoaderAndStart();

  run_loop1.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(0u, throttle->before_will_process_response_called());
    EXPECT_EQ(0u, throttle->will_process_response_called());
    EXPECT_EQ(throttle_->observed_response_url(), modified_url);
  }

  factory_.NotifyClientOnReceiveResponse();

  run_loop2.Run();

  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(1u, throttle->before_will_process_response_called());
    EXPECT_EQ(0u, throttle->will_process_response_called());
  }

  // The next time we intercept CreateLoaderAndStart() should be for the
  // restarted request (load flags of 1 | 2 | 4).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(7, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop3.QuitClosure()));

  int next_load_flag = 1;
  for (auto* throttle : throttles) {
    throttle->delegate()->RestartWithURLResetAndFlags(next_load_flag);
    throttle->delegate()->Resume();
    next_load_flag <<= 1;
  }

  run_loop3.Run();

  // Now that the restarted request has been made, clear
  // BeforeWillProcessResponse().
  for (auto* throttle : throttles) {
    throttle->set_before_will_process_response_callback(
        TestURLLoaderThrottle::ThrottleCallback());
  }

  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop4](int error) {
        EXPECT_EQ(net::OK, error);
        run_loop4.Quit();
      }));

  // Complete the response.
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop4.Run();

  EXPECT_EQ(2u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(1u, throttle->will_redirect_request_called());
    EXPECT_EQ(2u, throttle->before_will_process_response_called());
    EXPECT_EQ(1u, throttle->will_process_response_called());
    EXPECT_EQ(throttle_->observed_response_url(), request_url);
  }
}

// Call RestartWithURLResetAndFlags() from multiple throttles -- two while
// deferred, and one while processing BeforeWillProcessResponse(). Ensures that
// the request is restarted exactly once, using the combination of all
// additional load flags, and that the restarted requests use the original URL.
TEST_F(ThrottlingURLLoaderTest,
       MultipleRestartWithURLResetAndFlagsDeferAndSync) {
  // Create two additional TestURLLoaderThrottles for a total of 3, and keep
  // local unowned pointers to them in |throttles|.
  std::vector<TestURLLoaderThrottle*> throttles;
  ASSERT_EQ(1u, throttles_.size());
  throttles.push_back(throttle_);
  for (size_t i = 0; i < 2u; ++i) {
    auto throttle = std::make_unique<TestURLLoaderThrottle>();
    throttles.push_back(throttle.get());
    throttles_.push_back(std::move(throttle));
  }

  ASSERT_EQ(3u, throttles_.size());
  ASSERT_EQ(3u, throttles.size());

  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  base::RunLoop run_loop3;
  base::RunLoop run_loop4;
  base::RunLoop run_loop_for_redirect;

  // URL for internal redirect.
  GURL modified_url = GURL("www.example.uk.com");
  throttle_->set_modify_url_in_will_start(modified_url);

  // Check that the initial loader uses the default load flags (0).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(0, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  // Set the client to actually follow redirects to allow URL resetting to
  // occur.
  client_.set_on_received_redirect_callback(base::BindLambdaForTesting([&]() {
    net::HttpRequestHeaders modified_headers;
    loader_->FollowRedirect({} /* removed_headers */,
                            std::move(modified_headers),
                            {} /* modified_cors_exempt_headers */);
    run_loop_for_redirect.Quit();
  }));

  // Have two of the throttles defer, and one call restart
  // synchronously. Once all are run, quit run_loop2.
  int throttle_counter = 0;
  for (size_t i = 0; i < 2u; ++i) {
    throttles[i]->set_before_will_process_response_callback(base::BindRepeating(
        [](const base::RepeatingClosure& quit_closure, int* count,
           blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
          *defer = true;
          if (++(*count) == 3) {
            quit_closure.Run();
          }
        },
        run_loop2.QuitClosure(), &throttle_counter));
  }
  throttles[2]->set_before_will_process_response_callback(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure, int* count,
         blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
        delegate->RestartWithURLResetAndFlags(4);
        if (++(*count) == 3) {
          quit_closure.Run();
        }
      },
      run_loop2.QuitClosure(), &throttle_counter));

  CreateLoaderAndStart();

  run_loop1.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(0u, throttle->before_will_process_response_called());
    EXPECT_EQ(0u, throttle->will_process_response_called());
    EXPECT_EQ(throttle_->observed_response_url(), modified_url);
  }

  factory_.NotifyClientOnReceiveResponse();

  run_loop2.Run();

  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(1u, throttle->before_will_process_response_called());
    EXPECT_EQ(0u, throttle->will_process_response_called());
  }

  // The next time we intercept CreateLoaderAndStart() should be for the
  // restarted request (load flags of 1 | 2 | 4).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(7, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop3.QuitClosure()));

  int next_load_flag = 1;
  throttles[0]->delegate()->RestartWithURLResetAndFlags(next_load_flag);
  throttles[0]->delegate()->Resume();
  next_load_flag <<= 1;

  throttles[1]->delegate()->RestartWithURLResetAndFlags(next_load_flag);
  throttles[1]->delegate()->Resume();
  next_load_flag <<= 1;
  run_loop_for_redirect.Run();

  throttles[2]->delegate()->RestartWithURLResetAndFlags(next_load_flag);
  throttles[2]->delegate()->Resume();
  next_load_flag <<= 1;

  run_loop3.Run();

  // Now that the restarted request has been made, clear
  // BeforeWillProcessResponse().
  for (auto* throttle : throttles) {
    throttle->set_before_will_process_response_callback(
        TestURLLoaderThrottle::ThrottleCallback());
  }

  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop4](int error) {
        EXPECT_EQ(net::OK, error);
        run_loop4.Quit();
      }));

  // Complete the response.
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop4.Run();

  EXPECT_EQ(2u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(1u, throttle->will_redirect_request_called());
    EXPECT_EQ(2u, throttle->before_will_process_response_called());
    EXPECT_EQ(1u, throttle->will_process_response_called());
    EXPECT_EQ(throttle_->observed_response_url(), request_url);
  }
}

// Call RestartWithFlags() and RestartWithURLResetFlags() from separate
// throttles while processing BeforeWillProcessResponse(). Ensures that the
// request is restarted exactly once, using the combination of all additional
// load flags, and with the original URL.
TEST_F(ThrottlingURLLoaderTest, MultipleRestartsOfMultipleTypes) {
  // Create two additional TestURLLoaderThrottles for a total of 3, and keep
  // local unowned pointers to them in |throttles|.
  std::vector<TestURLLoaderThrottle*> throttles;
  ASSERT_EQ(1u, throttles_.size());
  throttles.push_back(throttle_);
  for (size_t i = 0; i < 2u; ++i) {
    auto throttle = std::make_unique<TestURLLoaderThrottle>();
    throttles.push_back(throttle.get());
    throttles_.push_back(std::move(throttle));
  }
  ASSERT_EQ(3u, throttles_.size());
  ASSERT_EQ(3u, throttles.size());

  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  base::RunLoop run_loop3;

  // URL for internal redirect.
  GURL modified_url = GURL("www.example.uk.com");
  throttle_->set_modify_url_in_will_start(modified_url);

  // Check that the initial loader uses the default load flags (0).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(0, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  // Set the client to actually follow redirects to allow URL resetting to
  // occur.
  client_.set_on_received_redirect_callback(base::BindLambdaForTesting([&]() {
    net::HttpRequestHeaders modified_headers;
    loader_->FollowRedirect({} /* removed_headers */,
                            std::move(modified_headers),
                            {} /* modified_cors_exempt_headers */);
  }));

  // Have two of the three throttles restart when processing
  // BeforeWillProcessResponse(), using
  // different load flags (2 and 8), and one with URL resets.
  throttles[0]->set_before_will_process_response_callback(
      base::BindRepeating([](blink::URLLoaderThrottle::Delegate* delegate,
                             bool* defer) { delegate->RestartWithFlags(2); }));
  throttles[2]->set_before_will_process_response_callback(base::BindRepeating(
      [](blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
        delegate->RestartWithURLResetAndFlags(8);
      }));

  CreateLoaderAndStart();

  run_loop1.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(0u, throttle->before_will_process_response_called());
    EXPECT_EQ(0u, throttle->will_process_response_called());
    EXPECT_EQ(throttle_->observed_response_url(), modified_url);
  }

  // The next time we intercept CreateLoaderAndStart() should be for the
  // restarted request (load flags of 10 = (2 | 8)).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(10, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop2.QuitClosure()));

  factory_.NotifyClientOnReceiveResponse();

  run_loop2.Run();

  // Now that the restarted request has been made, clear
  // BeforeWillProcessResponse() so it doesn't restart the request yet again.
  for (auto* throttle : throttles) {
    throttle->set_before_will_process_response_callback(
        TestURLLoaderThrottle::ThrottleCallback());
  }

  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop3](int error) {
        EXPECT_EQ(net::OK, error);
        run_loop3.Quit();
      }));

  // Complete the response.
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop3.Run();

  EXPECT_EQ(2u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(1u, throttle->will_redirect_request_called());
    EXPECT_EQ(2u, throttle->before_will_process_response_called());
    EXPECT_EQ(1u, throttle->will_process_response_called());
    EXPECT_EQ(throttle_->observed_response_url(), request_url);
  }
}

// Call RestartWithURLResetAndFlags() and RestartWithFlags from separate
// throttles after having deferred BeforeWillProcessResponse() in each. Ensures
// that the request is started exactly once, using the combination of all
// additional load flags, and with the original URL.
TEST_F(ThrottlingURLLoaderTest, MultipleDeferThenRestartsOfMultipleTypes) {
  // Create two additional TestURLLoaderThrottles for a total of 3, and keep
  // local unowned pointers to them in |throttles|.
  std::vector<TestURLLoaderThrottle*> throttles;
  ASSERT_EQ(1u, throttles_.size());
  throttles.push_back(throttle_);
  for (size_t i = 0; i < 2u; ++i) {
    auto throttle = std::make_unique<TestURLLoaderThrottle>();
    throttles.push_back(throttle.get());
    throttles_.push_back(std::move(throttle));
  }

  ASSERT_EQ(3u, throttles_.size());
  ASSERT_EQ(3u, throttles.size());

  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  base::RunLoop run_loop3;
  base::RunLoop run_loop4;

  // URL for internal redirect.
  GURL modified_url = GURL("www.example.uk.com");
  throttle_->set_modify_url_in_will_start(modified_url);

  // Check that the initial loader uses the default load flags (0).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(0, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  // Set the client to actually follow redirects to allow URL resetting to
  // occur.
  client_.set_on_received_redirect_callback(base::BindLambdaForTesting([&]() {
    net::HttpRequestHeaders modified_headers;
    loader_->FollowRedirect({} /* removed_headers */,
                            std::move(modified_headers),
                            {} /* modified_cors_exempt_headers */);
  }));

  // Have all of the throttles defer. Once they have all been deferred, quit
  // run_loop2.
  int throttle_counter = 0;
  for (auto* throttle : throttles) {
    throttle->set_before_will_process_response_callback(base::BindRepeating(
        [](const base::RepeatingClosure& quit_closure, int* count,
           blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
          *defer = true;
          if (++(*count) == 3) {
            quit_closure.Run();
          }
        },
        run_loop2.QuitClosure(), &throttle_counter));
  }

  CreateLoaderAndStart();

  run_loop1.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(0u, throttle->before_will_process_response_called());
    EXPECT_EQ(0u, throttle->will_process_response_called());
    EXPECT_EQ(throttle_->observed_response_url(), modified_url);
  }

  factory_.NotifyClientOnReceiveResponse();

  run_loop2.Run();

  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(1u, throttle->before_will_process_response_called());
    EXPECT_EQ(0u, throttle->will_process_response_called());
  }

  // The next time we intercept CreateLoaderAndStart() should be for the
  // restarted request (load flags of 1 | 2 | 4).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(7, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop3.QuitClosure()));

  // Restart throttles with different load flags, one with an URL reset.
  int next_load_flag = 1;
  bool with_url_reset = true;
  for (auto* throttle : throttles) {
    if (with_url_reset) {
      throttle->delegate()->RestartWithURLResetAndFlags(next_load_flag);
      with_url_reset = false;
    }
    throttle->delegate()->RestartWithFlags(next_load_flag);
    throttle->delegate()->Resume();
    next_load_flag <<= 1;
  }

  run_loop3.Run();

  // Now that the restarted request has been made, clear
  // BeforeWillProcessResponse().
  for (auto* throttle : throttles) {
    throttle->set_before_will_process_response_callback(
        TestURLLoaderThrottle::ThrottleCallback());
  }

  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop4](int error) {
        EXPECT_EQ(net::OK, error);
        run_loop4.Quit();
      }));

  // Complete the response.
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop4.Run();

  EXPECT_EQ(2u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(1u, throttle->will_redirect_request_called());
    EXPECT_EQ(2u, throttle->before_will_process_response_called());
    EXPECT_EQ(1u, throttle->will_process_response_called());
    EXPECT_EQ(throttle_->observed_response_url(), request_url);
  }
}

// Call RestartWithURLResetAndFlags() from two throttles while
// deferred, and one RestartWithFlags() while processing
// BeforeWillProcessResponse(). Ensures that the request is restarted exactly
// once, using the combination of all additional load flags, and that the
// restarted requests use the original URL.
TEST_F(ThrottlingURLLoaderTest, MultipleRestartOfMultipleTypesDeferAndSync) {
  // Create two additional TestURLLoaderThrottles for a total of 3, and keep
  // local unowned pointers to them in |throttles|.
  std::vector<TestURLLoaderThrottle*> throttles;
  ASSERT_EQ(1u, throttles_.size());
  throttles.push_back(throttle_);
  for (size_t i = 0; i < 2u; ++i) {
    auto throttle = std::make_unique<TestURLLoaderThrottle>();
    throttles.push_back(throttle.get());
    throttles_.push_back(std::move(throttle));
  }

  ASSERT_EQ(3u, throttles_.size());
  ASSERT_EQ(3u, throttles.size());

  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  base::RunLoop run_loop3;
  base::RunLoop run_loop4;
  base::RunLoop run_loop_for_redirect;

  // URL for internal redirect.
  GURL modified_url = GURL("www.example.uk.com");
  throttle_->set_modify_url_in_will_start(modified_url);

  // Check that the initial loader uses the default load flags (0).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(0, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  // Set the client to actually follow redirects to allow URL resetting to
  // occur.
  client_.set_on_received_redirect_callback(base::BindLambdaForTesting([&]() {
    net::HttpRequestHeaders modified_headers;
    loader_->FollowRedirect({} /* removed_headers */,
                            std::move(modified_headers),
                            {} /* modified_cors_exempt_headers */);
    run_loop_for_redirect.Quit();
  }));

  // Have two of the throttles defer, and one call restart
  // synchronously. Once all are run, quit run_loop2.
  int throttle_counter = 0;
  for (size_t i = 0; i < 2u; ++i) {
    throttles[i]->set_before_will_process_response_callback(base::BindRepeating(
        [](const base::RepeatingClosure& quit_closure, int* count,
           blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
          *defer = true;
          if (++(*count) == 3) {
            quit_closure.Run();
          }
        },
        run_loop2.QuitClosure(), &throttle_counter));
  }
  throttles[2]->set_before_will_process_response_callback(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure, int* count,
         blink::URLLoaderThrottle::Delegate* delegate, bool* defer) {
        delegate->RestartWithFlags(4);
        if (++(*count) == 3) {
          quit_closure.Run();
        }
      },
      run_loop2.QuitClosure(), &throttle_counter));

  CreateLoaderAndStart();

  run_loop1.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(0u, throttle->before_will_process_response_called());
    EXPECT_EQ(0u, throttle->will_process_response_called());
    EXPECT_EQ(throttle_->observed_response_url(), modified_url);
  }

  factory_.NotifyClientOnReceiveResponse();

  run_loop2.Run();

  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(0u, throttle->will_redirect_request_called());
    EXPECT_EQ(1u, throttle->before_will_process_response_called());
    EXPECT_EQ(0u, throttle->will_process_response_called());
  }

  // The next time we intercept CreateLoaderAndStart() should be for the
  // restarted request (load flags of 1 | 2 | 4).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(7, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop3.QuitClosure()));

  int next_load_flag = 1;
  throttles[0]->delegate()->RestartWithURLResetAndFlags(next_load_flag);
  throttles[0]->delegate()->Resume();
  next_load_flag <<= 1;

  throttles[1]->delegate()->RestartWithURLResetAndFlags(next_load_flag);
  throttles[1]->delegate()->Resume();
  next_load_flag <<= 1;
  run_loop_for_redirect.Run();

  throttles[2]->delegate()->RestartWithURLResetAndFlags(next_load_flag);
  throttles[2]->delegate()->Resume();
  next_load_flag <<= 1;

  run_loop3.Run();

  // Now that the restarted request has been made, clear
  // BeforeWillProcessResponse().
  for (auto* throttle : throttles) {
    throttle->set_before_will_process_response_callback(
        TestURLLoaderThrottle::ThrottleCallback());
  }

  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop4](int error) {
        EXPECT_EQ(net::OK, error);
        run_loop4.Quit();
      }));

  // Complete the response.
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop4.Run();

  EXPECT_EQ(2u, factory_.create_loader_and_start_called());
  for (auto* throttle : throttles) {
    EXPECT_EQ(1u, throttle->will_start_request_called());
    EXPECT_EQ(1u, throttle->will_redirect_request_called());
    EXPECT_EQ(2u, throttle->before_will_process_response_called());
    EXPECT_EQ(1u, throttle->will_process_response_called());
    EXPECT_EQ(throttle_->observed_response_url(), request_url);
  }
}

// Test restarts from "BeforeWillRedirectRequest"
TEST_F(ThrottlingURLLoaderTest,
       RestartWithURLResetAndFlagsBeforeWillRedirectRequest) {
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  base::RunLoop run_loop3;

  // URL for internal redirect.
  GURL modified_url = GURL("www.example.uk.com");
  throttle_->set_modify_url_in_will_start(modified_url);

  // Check that the initial loader uses the default load flags (0).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(0, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop1.QuitClosure()));

  // Set the client to actually follow redirects to allow URL resetting to
  // occur.
  client_.set_on_received_redirect_callback(base::BindLambdaForTesting([&]() {
    net::HttpRequestHeaders modified_headers;
    loader_->FollowRedirect({} /* removed_headers */,
                            std::move(modified_headers),
                            {} /* modified_cors_exempt_headers */);
  }));

  // Restart the request when processing BeforeWillProcessResponse(), using
  // different load flags (1), and an URL reset.
  throttle_->set_before_will_redirect_request_callback(
      base::BindLambdaForTesting(
          [&](blink::URLLoaderThrottle::Delegate* delegate, bool* /* defer */,
              std::vector<std::string>* /* removed_headers */,

              net::HttpRequestHeaders* /* modified_headers */,
              net::HttpRequestHeaders* /* modified_cors_exempt_headers */) {
            delegate->RestartWithURLResetAndFlags(1);
          }));

  CreateLoaderAndStart();

  run_loop1.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(0u, throttle_->before_will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());
  EXPECT_EQ(throttle_->observed_response_url(), modified_url);

  // The next time we intercept CreateLoaderAndStart() should be for the
  // restarted request (load flags of 1).
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const base::RepeatingClosure& quit_closure,
         const network::ResourceRequest& url_request) {
        EXPECT_EQ(1, url_request.load_flags);
        quit_closure.Run();
      },
      run_loop2.QuitClosure()));

  factory_.NotifyClientOnReceiveRedirect();

  run_loop2.Run();

  // Now that the restarted request has been made, clear
  // BeforeWillRedirectResponse() so it doesn't restart the request again.
  throttle_->set_before_will_redirect_request_callback(
      TestURLLoaderThrottle::ThrottleRedirectCallback());

  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop3](int error) {
        EXPECT_EQ(net::OK, error);
        run_loop3.Quit();
      }));

  // Complete the response.
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop3.Run();

  EXPECT_EQ(2u, factory_.create_loader_and_start_called());
  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(2u, throttle_->before_will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());
  EXPECT_EQ(throttle_->observed_response_url(), request_url);
}

// Test to make sure that restarts can't happen from "WillRedirectRequest", and
// that restarts from one redirect do not affect the next one.
TEST_F(ThrottlingURLLoaderTest,
       RestartWithURLResetAndFlagsWillRedirectRequestNoOp) {
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  base::RunLoop run_loop3;

  // Set the client to actually follow redirects to allow URL resetting to
  // occur.
  client_.set_on_received_redirect_callback(base::BindLambdaForTesting([&]() {
    net::HttpRequestHeaders modified_headers;
    loader_->FollowRedirect({} /* removed_headers */,
                            std::move(modified_headers),
                            {} /* modified_cors_exempt_headers */);
  }));

  client_.set_on_complete_callback(
      base::BindLambdaForTesting([&run_loop3](int error) {
        EXPECT_EQ(net::OK, error);
        run_loop3.Quit();
      }));

  // Restart the request when processing WillRedirectRequest(), using
  // different load flags (1), and an URL reset. This should be a no-op, as the
  // reset flag is only checked *before* WillRedirectRequest.
  throttle_->set_will_redirect_request_callback(base::BindLambdaForTesting(
      [&run_loop1](
          blink::URLLoaderThrottle::Delegate* delegate, bool* /* defer */,
          std::vector<std::string>* /* removed_headers */,
          net::HttpRequestHeaders* /* modified_headers */,
          net::HttpRequestHeaders* /* modified_cors_exempt_headers */) {
        delegate->RestartWithURLResetAndFlags(1);
        run_loop1.Quit();
      }));

  CreateLoaderAndStart();
  factory_.NotifyClientOnReceiveRedirect();
  run_loop1.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(1u, throttle_->before_will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  // No restarts, so create loader and start isn't called again
  factory_.set_on_create_loader_and_start(base::BindRepeating(
      [](const network::ResourceRequest& url_request) { NOTREACHED(); }));

  // Make another redirect to ensure a restart from one doesn't affect the next
  // one.
  throttle_->set_will_redirect_request_callback(base::BindLambdaForTesting(
      [&run_loop2](
          blink::URLLoaderThrottle::Delegate* delegate, bool* /* defer */,
          std::vector<std::string>* /* removed_headers */,
          net::HttpRequestHeaders* /* modified_headers */,
          net::HttpRequestHeaders* /* modified_cors_exempt_headers */) {
        delegate->RestartWithURLResetAndFlags(1);
        run_loop2.Quit();
      }));

  factory_.NotifyClientOnReceiveRedirect();
  run_loop2.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(2u, throttle_->before_will_redirect_request_called());
  EXPECT_EQ(2u, throttle_->will_redirect_request_called());
  EXPECT_EQ(0u, throttle_->before_will_process_response_called());
  EXPECT_EQ(0u, throttle_->will_process_response_called());

  // Ensure that the flag above doesn't affect responses.
  factory_.NotifyClientOnReceiveResponse();
  factory_.NotifyClientOnComplete(net::OK);

  run_loop3.Run();

  EXPECT_EQ(1u, factory_.create_loader_and_start_called());
  EXPECT_EQ(1u, throttle_->will_start_request_called());
  EXPECT_EQ(2u, throttle_->before_will_redirect_request_called());
  EXPECT_EQ(2u, throttle_->will_redirect_request_called());
  EXPECT_EQ(1u, throttle_->before_will_process_response_called());
  EXPECT_EQ(1u, throttle_->will_process_response_called());
  EXPECT_EQ(throttle_->observed_response_url(), redirect_url);
}

}  // namespace
}  // namespace blink
