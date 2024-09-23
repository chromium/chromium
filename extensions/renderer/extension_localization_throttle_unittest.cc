// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_localization_throttle.h"

#include <string_view>

#include "base/test/task_environment.h"
#include "extensions/renderer/shared_l10n_map.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "net/base/request_priority.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/platform/web_url.h"

namespace extensions {
namespace {

class FakeURLLoader final : public network::mojom::URLLoader {
 public:
  enum class Status {
    kInitial,
    kPauseReading,
    kResumeReading,
  };

  explicit FakeURLLoader(
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver)
      : receiver_(this, std::move(url_loader_receiver)) {}
  ~FakeURLLoader() override = default;

  FakeURLLoader(const FakeURLLoader&) = delete;
  FakeURLLoader& operator=(const FakeURLLoader&) = delete;

  // network::mojom::URLLoader overrides.
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override {
    NOTREACHED_IN_MIGRATION();
  }
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    set_priority_called_ = true;
  }
  void PauseReadingBodyFromNet() override { status_ = Status::kPauseReading; }
  void ResumeReadingBodyFromNet() override { status_ = Status::kResumeReading; }

  bool set_priority_called() const { return set_priority_called_; }

  Status status() const { return status_; }

 private:
  bool set_priority_called_ = false;

  Status status_ = Status::kInitial;

  mojo::Receiver<network::mojom::URLLoader> receiver_;
};

class FakeDelegate : public blink::URLLoaderThrottle::Delegate {
 public:
  // Implements blink::URLLoaderThrottle::Delegate.
  void CancelWithError(int error_code,
                       std::string_view custom_reason) override {
    cancel_error_code_ = error_code;
    cancel_custom_reason_ = std::string(custom_reason);
  }
  void Resume() override { NOTREACHED_IN_MIGRATION(); }

  void UpdateDeferredResponseHead(
      network::mojom::URLResponseHeadPtr new_response_head,
      mojo::ScopedDataPipeConsumerHandle body) override {
    NOTREACHED_IN_MIGRATION();
  }
  void InterceptResponse(
      mojo::PendingRemote<network::mojom::URLLoader> new_loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>
          new_client_receiver,
      mojo::PendingRemote<network::mojom::URLLoader>* original_loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>*
          original_client_receiver,
      mojo::ScopedDataPipeConsumerHandle* body) override {
    is_intercepted_ = true;

    destination_loader_remote_.Bind(std::move(new_loader));
    ASSERT_TRUE(
        mojo::FusePipes(std::move(new_client_receiver),
                        mojo::PendingRemote<network::mojom::URLLoaderClient>(
                            destination_loader_client_.CreateRemote())));
    source_url_loader_ = std::make_unique<FakeURLLoader>(
        original_loader->InitWithNewPipeAndPassReceiver());

    *original_client_receiver =
        source_loader_client_remote_.BindNewPipeAndPassReceiver();

    DCHECK(!source_body_handle_);

    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    EXPECT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(/*options=*/nullptr, source_body_handle_,
                                   consumer_handle));
    body->swap(consumer_handle);

    destination_loader_client()->OnReceiveResponse(
        network::mojom::URLResponseHead::New(), std::move(consumer_handle),
        std::nullopt);
  }

  void LoadResponseBody(const std::string& body) {
    mojo::BlockingCopyFromString(body, source_body_handle_);
  }

  void CompleteResponse() {
    source_loader_client_remote()->OnComplete(
        network::URLLoaderCompletionStatus());
    source_body_handle_.reset();
  }

  bool is_intercepted() const { return is_intercepted_; }
  const std::optional<int>& cancel_error_code() const {
    return cancel_error_code_;
  }
  const std::optional<std::string>& cancel_custom_reason() const {
    return cancel_custom_reason_;
  }

  mojo::Remote<network::mojom::URLLoader>& destination_loader_remote() {
    return destination_loader_remote_;
  }

  network::TestURLLoaderClient* destination_loader_client() {
    return &destination_loader_client_;
  }

  FakeURLLoader* source_url_loader() { return source_url_loader_.get(); }

  mojo::Remote<network::mojom::URLLoaderClient>& source_loader_client_remote() {
    return source_loader_client_remote_;
  }

  mojo::ScopedDataPipeProducerHandle& source_body_handle() {
    return source_body_handle_;
  }

 private:
  bool is_intercepted_ = false;
  std::optional<int> cancel_error_code_;
  std::optional<std::string> cancel_custom_reason_;

  //  The chain of mojom::URLLoaderClient:
  //    [Blink side]
  //    destination_loader_client_
  //     <- ExtensionLocalizationURLLoader::destination_url_loader_client_
  //     <- ExtensionLocalizationURLLoader
  //     <- ExtensionLocalizationURLLoader::source_url_client_receiver_
  //     <- source_loader_client_remote_
  //    [Browser process side]

  //  The chain of mojom::URLLoader:
  //    [Blink side]
  //    destination_loader_remote_
  //     -> ExtensionLocalizationURLLoader (SelfOwnedReceiver)
  //     -> ExtensionLocalizationURLLoader::source_url_loader_
  //     -> source_url_loader_
  //    [Browser process side]

  mojo::Remote<network::mojom::URLLoader> destination_loader_remote_;
  network::TestURLLoaderClient destination_loader_client_;

  std::unique_ptr<FakeURLLoader> source_url_loader_;
  mojo::Remote<network::mojom::URLLoaderClient> source_loader_client_remote_;

  mojo::ScopedDataPipeProducerHandle source_body_handle_;
};

class ExtensionLocalizationThrottleTest : public testing::Test {
 protected:
  void SetUp() override {
    extensions::SharedL10nMap::L10nMessagesMap messages;
    messages.insert(std::make_pair("hello", "hola"));
    messages.insert(std::make_pair("world", "mundo"));
    extensions::SharedL10nMap::GetInstance().SetMessagesForTesting(
        "some_id", std::move(messages));
  }
  // Be the first member so it is destroyed last.
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ExtensionLocalizationThrottleTest, DoNotCreate) {
  EXPECT_FALSE(ExtensionLocalizationThrottle::MaybeCreate(
      std::nullopt, blink::WebURL(GURL("https://example.com/test.css"))));
  EXPECT_FALSE(ExtensionLocalizationThrottle::MaybeCreate(
      std::nullopt, blink::WebURL(GURL("http://example.com/test.css"))));
}

TEST_F(ExtensionLocalizationThrottleTest, DoNotIntercept) {
  const GURL url("chrome-extension://some_id/test.txt");
  auto throttle = ExtensionLocalizationThrottle::MaybeCreate(
      std::nullopt, blink::WebURL(url));
  ASSERT_TRUE(throttle);
  auto delegate = std::make_unique<FakeDelegate>();
  throttle->set_delegate(delegate.get());

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->mime_type = "text/plain";
  bool defer = false;
  throttle->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
  EXPECT_FALSE(delegate->is_intercepted());
}

TEST_F(ExtensionLocalizationThrottleTest, OneMessage) {
  const GURL url("chrome-extension://some_id/test.css");
  auto throttle = ExtensionLocalizationThrottle::MaybeCreate(
      std::nullopt, blink::WebURL(url));
  ASSERT_TRUE(throttle);

  auto delegate = std::make_unique<FakeDelegate>();
  throttle->set_delegate(delegate.get());

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->mime_type = "text/css";
  bool defer = false;
  throttle->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
  EXPECT_TRUE(delegate->is_intercepted());
  delegate->LoadResponseBody("__MSG_hello__!");
  delegate->CompleteResponse();
  delegate->destination_loader_client()->RunUntilComplete();

  std::string response;
  EXPECT_TRUE(mojo::BlockingCopyToString(
      delegate->destination_loader_client()->response_body_release(),
      &response));
  EXPECT_EQ("hola!", response);
  EXPECT_EQ(
      net::OK,
      delegate->destination_loader_client()->completion_status().error_code);
}

TEST_F(ExtensionLocalizationThrottleTest, TwoMessages) {
  const GURL url("chrome-extension://some_id/test.css");
  auto throttle = ExtensionLocalizationThrottle::MaybeCreate(
      std::nullopt, blink::WebURL(url));
  ASSERT_TRUE(throttle);

  auto delegate = std::make_unique<FakeDelegate>();
  throttle->set_delegate(delegate.get());

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->mime_type = "text/css";
  bool defer = false;
  throttle->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
  EXPECT_TRUE(delegate->is_intercepted());
  delegate->LoadResponseBody("__MSG_hello__ __MSG");
  task_environment_.RunUntilIdle();
  delegate->LoadResponseBody("_world__!");
  delegate->CompleteResponse();

  delegate->destination_loader_client()->RunUntilComplete();

  std::string response;
  EXPECT_TRUE(mojo::BlockingCopyToString(
      delegate->destination_loader_client()->response_body_release(),
      &response));
  EXPECT_EQ("hola mundo!", response);
  EXPECT_EQ(
      net::OK,
      delegate->destination_loader_client()->completion_status().error_code);
}

TEST_F(ExtensionLocalizationThrottleTest, EmptyData) {
  const GURL url("chrome-extension://some_id/test.css");
  auto throttle = ExtensionLocalizationThrottle::MaybeCreate(
      std::nullopt, blink::WebURL(url));
  ASSERT_TRUE(throttle);

  auto delegate = std::make_unique<FakeDelegate>();
  throttle->set_delegate(delegate.get());

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->mime_type = "text/css";
  bool defer = false;
  throttle->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
  EXPECT_TRUE(delegate->is_intercepted());
  delegate->CompleteResponse();
  delegate->destination_loader_client()->RunUntilComplete();

  std::string response;
  EXPECT_TRUE(mojo::BlockingCopyToString(
      delegate->destination_loader_client()->response_body_release(),
      &response));
  EXPECT_EQ("", response);
  EXPECT_EQ(
      net::OK,
      delegate->destination_loader_client()->completion_status().error_code);
}

// Regression test for https://crbug.com/1475798
TEST_F(ExtensionLocalizationThrottleTest, Cancel) {
  const GURL url("chrome-extension://some_id/test.css");
  auto throttle = ExtensionLocalizationThrottle::MaybeCreate(
      std::nullopt, blink::WebURL(url));
  ASSERT_TRUE(throttle);

  auto delegate = std::make_unique<FakeDelegate>();
  throttle->set_delegate(delegate.get());

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->mime_type = "text/css";
  bool defer = false;
  throttle->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
  EXPECT_TRUE(delegate->is_intercepted());
  delegate->LoadResponseBody("__MSG_hello__!");
  delegate->CompleteResponse();
  // Run all tasks in the main thread to make DataPipeProducer::SequenceState
  // call PostTask(&SequenceState::StartOnSequence) to a background thread.
  base::RunLoop().RunUntilIdle();
  // Resetting `destination_loader_remote` triggers
  // ExtensionLocalizationURLLoader destruction.
  delegate->destination_loader_remote().reset();
  // Run all tasks in the main thread to destroy the
  // ExtensionLocalizationURLLoader.
  base::RunLoop().RunUntilIdle();
  // Runs SequenceState::StartOnSequence in the background thread.
  task_environment_.RunUntilIdle();
}

TEST_F(ExtensionLocalizationThrottleTest, SourceSideError) {
  const GURL url("chrome-extension://some_id/test.css");
  auto throttle = ExtensionLocalizationThrottle::MaybeCreate(
      std::nullopt, blink::WebURL(url));
  ASSERT_TRUE(throttle);

  auto delegate = std::make_unique<FakeDelegate>();
  throttle->set_delegate(delegate.get());

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->mime_type = "text/css";
  bool defer = false;
  throttle->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
  EXPECT_TRUE(delegate->is_intercepted());
  delegate->LoadResponseBody("__MSG_hello__!");

  delegate->source_loader_client_remote()->OnComplete(
      network::URLLoaderCompletionStatus(net::ERR_OUT_OF_MEMORY));
  delegate->source_body_handle().reset();

  delegate->destination_loader_client()->RunUntilComplete();

  std::string response;
  EXPECT_TRUE(mojo::BlockingCopyToString(
      delegate->destination_loader_client()->response_body_release(),
      &response));
  EXPECT_EQ("hola!", response);
  EXPECT_EQ(
      net::ERR_OUT_OF_MEMORY,
      delegate->destination_loader_client()->completion_status().error_code);
}

TEST_F(ExtensionLocalizationThrottleTest, WriteError) {
  const GURL url("chrome-extension://some_id/test.css");
  auto throttle = ExtensionLocalizationThrottle::MaybeCreate(
      std::nullopt, blink::WebURL(url));
  ASSERT_TRUE(throttle);

  auto delegate = std::make_unique<FakeDelegate>();
  throttle->set_delegate(delegate.get());

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->mime_type = "text/css";
  bool defer = false;
  throttle->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
  EXPECT_TRUE(delegate->is_intercepted());

  // Release the body to cause write error.
  delegate->destination_loader_client()->response_body_release();
  task_environment_.RunUntilIdle();

  delegate->LoadResponseBody("__MSG_hello__!");
  delegate->CompleteResponse();
  delegate->destination_loader_client()->RunUntilComplete();

  EXPECT_EQ(
      net::ERR_INSUFFICIENT_RESOURCES,
      delegate->destination_loader_client()->completion_status().error_code);
}

TEST_F(ExtensionLocalizationThrottleTest, CreateDataPipeError) {
  const GURL url("chrome-extension://some_id/test.css");
  auto throttle = ExtensionLocalizationThrottle::MaybeCreate(
      std::nullopt, blink::WebURL(url));
  ASSERT_TRUE(throttle);
  throttle->ForceCreateDataPipeErrorForTest();

  auto delegate = std::make_unique<FakeDelegate>();
  throttle->set_delegate(delegate.get());

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->mime_type = "text/css";
  bool defer = false;
  throttle->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_TRUE(defer);
  EXPECT_FALSE(delegate->is_intercepted());
  EXPECT_FALSE(delegate->cancel_error_code());

  // Run loop to call DeferredCancelWithError().
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(delegate->cancel_error_code());
  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES, *delegate->cancel_error_code());
  ASSERT_TRUE(delegate->cancel_custom_reason());
  EXPECT_EQ("ExtensionLocalizationThrottle", *delegate->cancel_custom_reason());
}

TEST_F(ExtensionLocalizationThrottleTest, URLLoaderChain) {
  const GURL url("chrome-extension://some_id/test.css");
  auto throttle = ExtensionLocalizationThrottle::MaybeCreate(
      std::nullopt, blink::WebURL(url));
  ASSERT_TRUE(throttle);

  auto delegate = std::make_unique<FakeDelegate>();
  throttle->set_delegate(delegate.get());

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->mime_type = "text/css";
  bool defer = false;
  throttle->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
  EXPECT_TRUE(delegate->is_intercepted());

  FakeURLLoader* source_url_loader = delegate->source_url_loader();
  mojo::Remote<network::mojom::URLLoader>& destination_loader_remote =
      delegate->destination_loader_remote();

  ASSERT_TRUE(source_url_loader);
  EXPECT_FALSE(source_url_loader->set_priority_called());
  EXPECT_EQ(FakeURLLoader::Status::kInitial, source_url_loader->status());

  destination_loader_remote->SetPriority(net::LOW, 1);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(source_url_loader->set_priority_called());

  destination_loader_remote->PauseReadingBodyFromNet();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FakeURLLoader::Status::kPauseReading, source_url_loader->status());

  destination_loader_remote->ResumeReadingBodyFromNet();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FakeURLLoader::Status::kResumeReading, source_url_loader->status());

  delegate->LoadResponseBody("__MSG_hello__!");
  delegate->CompleteResponse();
  delegate->destination_loader_client()->RunUntilComplete();

  std::string response;
  EXPECT_TRUE(mojo::BlockingCopyToString(
      delegate->destination_loader_client()->response_body_release(),
      &response));
  EXPECT_EQ("hola!", response);
  EXPECT_EQ(
      net::OK,
      delegate->destination_loader_client()->completion_status().error_code);
}

TEST_F(ExtensionLocalizationThrottleTest,
       URLLoaderClientOnTransferSizeUpdated) {
  const GURL url("chrome-extension://some_id/test.css");
  auto throttle = ExtensionLocalizationThrottle::MaybeCreate(
      std::nullopt, blink::WebURL(url));
  ASSERT_TRUE(throttle);

  auto delegate = std::make_unique<FakeDelegate>();
  throttle->set_delegate(delegate.get());

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->mime_type = "text/css";
  bool defer = false;
  throttle->WillProcessResponse(url, response_head.get(), &defer);
  EXPECT_FALSE(defer);
  EXPECT_TRUE(delegate->is_intercepted());

  network::TestURLLoaderClient* destination_loader_client =
      delegate->destination_loader_client();
  mojo::Remote<network::mojom::URLLoaderClient>& source_loader_client_remote =
      delegate->source_loader_client_remote();

  ASSERT_TRUE(destination_loader_client);
  EXPECT_EQ(0, destination_loader_client->body_transfer_size());

  source_loader_client_remote->OnTransferSizeUpdated(/*transfer_size_diff=*/10);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(10, destination_loader_client->body_transfer_size());

  delegate->LoadResponseBody("__MSG_hello__!");
  delegate->CompleteResponse();
  destination_loader_client->RunUntilComplete();

  std::string response;
  EXPECT_TRUE(mojo::BlockingCopyToString(
      destination_loader_client->response_body_release(), &response));
  EXPECT_EQ("hola!", response);
  EXPECT_EQ(net::OK, destination_loader_client->completion_status().error_code);
}

}  // namespace
}  // namespace extensions
