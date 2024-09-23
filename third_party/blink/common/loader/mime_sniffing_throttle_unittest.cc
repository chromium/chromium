// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/mime_sniffing_throttle.h"

#include <memory>
#include <string_view>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/mime_sniffing_url_loader.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/gurl.h"

namespace blink {

namespace {

class MojoDataPipeSender {
 public:
  MojoDataPipeSender(mojo::ScopedDataPipeProducerHandle handle)
      : handle_(std::move(handle)),
        watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC) {}

  void Start(std::string data, base::OnceClosure done_callback) {
    data_ = std::move(data);
    done_callback_ = std::move(done_callback);
    watcher_.Watch(handle_.get(),
                   MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                   base::BindRepeating(&MojoDataPipeSender::OnWritable,
                                       base::Unretained(this)));
  }

  void OnWritable(MojoResult) {
    base::span<const uint8_t> bytes = base::as_byte_span(data_);
    bytes = bytes.subspan(sent_bytes_);
    size_t actually_written_bytes = 0;
    MojoResult result = handle_->WriteData(bytes, MOJO_WRITE_DATA_FLAG_NONE,
                                           actually_written_bytes);
    switch (result) {
      case MOJO_RESULT_OK:
        break;
      case MOJO_RESULT_FAILED_PRECONDITION:
        // Finished unexpectedly.
        std::move(done_callback_).Run();
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        // Just wait until OnWritable() is called by the watcher.
        return;
      default:
        NOTREACHED_IN_MIGRATION();
        return;
    }
    sent_bytes_ += actually_written_bytes;
    if (data_.size() == sent_bytes_)
      std::move(done_callback_).Run();
  }

  mojo::ScopedDataPipeProducerHandle ReleaseHandle() {
    return std::move(handle_);
  }

  bool has_succeeded() const { return data_.size() == sent_bytes_; }

 private:
  mojo::ScopedDataPipeProducerHandle handle_;
  mojo::SimpleWatcher watcher_;
  base::OnceClosure done_callback_;
  std::string data_;
  size_t sent_bytes_ = 0;
};

class MockDelegate : public blink::URLLoaderThrottle::Delegate {
 public:
  // Implements blink::URLLoaderThrottle::Delegate.
  void CancelWithError(int error_code,
                       std::string_view custom_reason) override {
    NOTIMPLEMENTED();
  }
  void Resume() override {
    is_resumed_ = true;
    // Resume from OnReceiveResponse() with a customized response header.
    destination_loader_client()->OnReceiveResponse(
        std::move(updated_response_head_), std::move(body_), std::nullopt);
  }

  void UpdateDeferredResponseHead(
      network::mojom::URLResponseHeadPtr new_response_head,
      mojo::ScopedDataPipeConsumerHandle body) override {
    updated_response_head_ = std::move(new_response_head);
    body_ = std::move(body);
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
    pending_receiver_ = original_loader->InitWithNewPipeAndPassReceiver();

    *original_client_receiver =
        source_loader_client_remote_.BindNewPipeAndPassReceiver();

    if (no_body_)
      return;

    DCHECK(!source_body_handle_);
    mojo::ScopedDataPipeConsumerHandle consumer;
    EXPECT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr, source_body_handle_, consumer));
    *body = std::move(consumer);
  }

  void LoadResponseBody(const std::string& body) {
    MojoDataPipeSender sender(std::move(source_body_handle_));
    base::RunLoop loop;
    sender.Start(body, loop.QuitClosure());
    loop.Run();

    EXPECT_TRUE(sender.has_succeeded());
    source_body_handle_ = sender.ReleaseHandle();
  }

  void CompleteResponse() {
    source_loader_client_remote()->OnComplete(
        network::URLLoaderCompletionStatus());
    source_body_handle_.reset();
  }

  uint32_t ReadResponseBody(size_t size) {
    std::vector<uint8_t> buffer(size);
    MojoResult result = destination_loader_client_.response_body().ReadData(
        MOJO_READ_DATA_FLAG_NONE, buffer, size);
    switch (result) {
      case MOJO_RESULT_OK:
        return size;
      case MOJO_RESULT_FAILED_PRECONDITION:
        return 0;
      case MOJO_RESULT_SHOULD_WAIT:
        return 0;
      default:
        NOTREACHED_IN_MIGRATION();
    }
    return 0;
  }

  void ResetProducer() { source_body_handle_.reset(); }

  bool is_intercepted() const { return is_intercepted_; }
  bool is_resumed() const { return is_resumed_; }
  void set_no_body() { no_body_ = true; }

  network::TestURLLoaderClient* destination_loader_client() {
    return &destination_loader_client_;
  }

  mojo::Remote<network::mojom::URLLoaderClient>& source_loader_client_remote() {
    return source_loader_client_remote_;
  }

 private:
  bool is_intercepted_ = false;
  bool is_resumed_ = false;
  bool no_body_ = false;
  network::mojom::URLResponseHeadPtr updated_response_head_;
  mojo::ScopedDataPipeConsumerHandle body_;

  // A pair of a loader and a loader client for destination of the response.
  mojo::Remote<network::mojom::URLLoader> destination_loader_remote_;
  network::TestURLLoaderClient destination_loader_client_;

  // A pair of a receiver and a remote for source of the response.
  mojo::PendingReceiver<network::mojom::URLLoader> pending_receiver_;
  mojo::Remote<network::mojom::URLLoaderClient> source_loader_client_remote_;

  mojo::ScopedDataPipeProducerHandle source_body_handle_;
};

}  // namespace

class MimeSniffingThrottleTest : public testing::Test {
 protected:
  // Be the first member so it is destroyed last.
  base::test::TaskEnvironment task_environment_;
};

TEST_F(MimeSniffingThrottleTest, NoMimeTypeWithSniffableScheme) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  auto response_head = network::mojom::URLResponseHead::New();
  bool defer = false;
  throttle->WillProcessResponse(GURL("https://example.com"),
                                response_head.get(), &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate->is_intercepted());
}

TEST_F(MimeSniffingThrottleTest, SniffableMimeTypeWithSniffableScheme) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->mime_type = "text/plain";
  bool defer = false;
  throttle->WillProcessResponse(GURL("https://example.com"),
                                response_head.get(), &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate->is_intercepted());
}

TEST_F(MimeSniffingThrottleTest, NotSniffableMimeTypeWithSniffableScheme) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->mime_type = "text/javascript";
  bool defer = false;
  throttle->WillProcessResponse(GURL("https://example.com"),
                                response_head.get(), &defer);
  EXPECT_FALSE(defer);
  EXPECT_FALSE(delegate->is_intercepted());
}

TEST_F(MimeSniffingThrottleTest, NoMimeTypeWithNotSniffableScheme) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  auto response_head = network::mojom::URLResponseHead::New();
  bool defer = false;
  throttle->WillProcessResponse(GURL("wss://example.com"), response_head.get(),
                                &defer);
  EXPECT_FALSE(defer);
  EXPECT_FALSE(delegate->is_intercepted());
}

TEST_F(MimeSniffingThrottleTest, SniffableMimeTypeWithNotSniffableScheme) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->mime_type = "text/plain";
  bool defer = false;
  throttle->WillProcessResponse(GURL("wss://example.com"), response_head.get(),
                                &defer);
  EXPECT_FALSE(defer);
  EXPECT_FALSE(delegate->is_intercepted());
}

TEST_F(MimeSniffingThrottleTest, NotSniffableMimeTypeWithNotSniffableScheme) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->mime_type = "text/javascript";
  bool defer = false;
  throttle->WillProcessResponse(GURL("wss://example.com"), response_head.get(),
                                &defer);
  EXPECT_FALSE(defer);
  EXPECT_FALSE(delegate->is_intercepted());
}

TEST_F(MimeSniffingThrottleTest, SniffableButAlreadySniffed) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->mime_type = "text/plain";
  response_head->did_mime_sniff = true;
  bool defer = false;
  throttle->WillProcessResponse(GURL("https://example.com"),
                                response_head.get(), &defer);
  EXPECT_FALSE(defer);
  EXPECT_FALSE(delegate->is_intercepted());
}

TEST_F(MimeSniffingThrottleTest, NoBody) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  GURL response_url("https://example.com");
  auto response_head = network::mojom::URLResponseHead::New();
  bool defer = false;
  delegate->set_no_body();
  throttle->WillProcessResponse(response_url, response_head.get(), &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate->is_intercepted());

  // Call OnComplete() without sending body.
  delegate->source_loader_client_remote()->OnComplete(
      network::URLLoaderCompletionStatus(net::ERR_FAILED));
  delegate->destination_loader_client()->RunUntilComplete();

  // The mime type should be updated to the default mime type ("text/plain").
  EXPECT_TRUE(delegate->destination_loader_client()->has_received_response());
  EXPECT_EQ("text/plain",
            delegate->destination_loader_client()->response_head()->mime_type);
}

TEST_F(MimeSniffingThrottleTest, EmptyBody) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  GURL response_url("https://example.com");
  auto response_head = network::mojom::URLResponseHead::New();
  bool defer = false;
  throttle->WillProcessResponse(response_url, response_head.get(), &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate->is_intercepted());

  delegate->ResetProducer();

  delegate->source_loader_client_remote()->OnComplete(
      network::URLLoaderCompletionStatus());
  delegate->destination_loader_client()->RunUntilComplete();

  // The mime type should be updated to the default mime type ("text/plain").
  EXPECT_TRUE(delegate->destination_loader_client()->has_received_response());
  EXPECT_EQ("text/plain",
            delegate->destination_loader_client()->response_head()->mime_type);
}

TEST_F(MimeSniffingThrottleTest, Body_PlainText) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  GURL response_url("https://example.com");
  auto response_head = network::mojom::URLResponseHead::New();
  bool defer = false;
  throttle->WillProcessResponse(response_url, response_head.get(), &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate->is_intercepted());

  // Send the body and complete the response.
  delegate->LoadResponseBody("This is a text.");
  delegate->CompleteResponse();
  delegate->destination_loader_client()->RunUntilComplete();

  // The mime type should be updated.
  EXPECT_TRUE(delegate->is_resumed());
  EXPECT_EQ("text/plain",
            delegate->destination_loader_client()->response_head()->mime_type);
}

TEST_F(MimeSniffingThrottleTest, Body_Docx) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  GURL response_url("https://example.com/hogehoge.docx");
  auto response_head = network::mojom::URLResponseHead::New();
  bool defer = false;
  throttle->WillProcessResponse(response_url, response_head.get(), &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate->is_intercepted());

  // Send the body and complete the response.
  delegate->LoadResponseBody("\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1");
  delegate->CompleteResponse();
  delegate->destination_loader_client()->RunUntilComplete();

  // The mime type should be updated.
  EXPECT_TRUE(delegate->is_resumed());
  EXPECT_EQ("application/msword",
            delegate->destination_loader_client()->response_head()->mime_type);
}

TEST_F(MimeSniffingThrottleTest, Body_PNG) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  GURL response_url("https://example.com/hogehoge.docx");
  auto response_head = network::mojom::URLResponseHead::New();
  bool defer = false;
  throttle->WillProcessResponse(response_url, response_head.get(), &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate->is_intercepted());

  // Send the body and complete the response.
  delegate->LoadResponseBody("\x89PNG\x0D\x0A\x1A\x0A");
  delegate->CompleteResponse();
  delegate->destination_loader_client()->RunUntilComplete();

  // The mime type should be updated.
  EXPECT_TRUE(delegate->is_resumed());
  EXPECT_EQ("image/png",
            delegate->destination_loader_client()->response_head()->mime_type);
}

TEST_F(MimeSniffingThrottleTest, Body_LongPlainText) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  GURL response_url("https://example.com");
  auto response_head = network::mojom::URLResponseHead::New();
  bool defer = false;
  throttle->WillProcessResponse(response_url, response_head.get(), &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate->is_intercepted());

  // 64KiB is coming from the default value used in
  // mojo::core::Core::CreateDataPipe().
  const uint32_t kDefaultDataPipeBufferSize = 64 * 1024;
  std::string long_body(kDefaultDataPipeBufferSize * 2, 'x');

  // Send the data to the MimeSniffingURLLoader.
  // |delegate|'s MojoDataPipeSender sends the first
  // |kDefaultDataPipeBufferSize| bytes to MimeSniffingURLLoader and
  // MimeSniffingURLLoader will read the first |kDefaultDataPipeBufferSize|
  // bytes of the body, so the MojoDataPipeSender can push the rest of
  // |kDefaultDataPipeBufferSize| of the body soon and finishes sending the
  // body. After this, MimeSniffingURLLoader is waiting to push the body to the
  // destination data pipe since the pipe should be full until it's read.
  delegate->LoadResponseBody(long_body);
  task_environment_.RunUntilIdle();

  // Send OnComplete() to the MimeSniffingURLLoader.
  delegate->CompleteResponse();
  task_environment_.RunUntilIdle();
  // MimeSniffingURLLoader should not send OnComplete() to the destination
  // client until it finished writing all the data.
  EXPECT_FALSE(
      delegate->destination_loader_client()->has_received_completion());

  // Read the half of the body. This unblocks MimeSniffingURLLoader to push the
  // rest of the body to the data pipe.
  uint32_t read_bytes = delegate->ReadResponseBody(long_body.size() / 2);
  task_environment_.RunUntilIdle();

  // Read the rest of the body.
  read_bytes += delegate->ReadResponseBody(long_body.size() / 2);
  task_environment_.RunUntilIdle();
  delegate->destination_loader_client()->RunUntilComplete();

  // Check if all data has been read.
  EXPECT_EQ(long_body.size(), read_bytes);

  // The mime type should be updated.
  EXPECT_TRUE(delegate->is_resumed());
  EXPECT_EQ("text/plain",
            delegate->destination_loader_client()->response_head()->mime_type);
}

TEST_F(MimeSniffingThrottleTest, Abort_NoBodyPipe) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  GURL response_url("https://example.com");
  auto response_head = network::mojom::URLResponseHead::New();
  bool defer = false;
  throttle->WillProcessResponse(response_url, response_head.get(), &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate->is_intercepted());

  // Send the body
  std::string body = "This should be long enough to complete sniffing.";
  body.resize(1024, 'a');
  delegate->LoadResponseBody(body);
  task_environment_.RunUntilIdle();

  // Release a pipe for the body on the receiver side.
  delegate->destination_loader_client()->response_body_release();
  task_environment_.RunUntilIdle();

  // Calling OnComplete should not crash.
  delegate->CompleteResponse();
  task_environment_.RunUntilIdle();
}

}  // namespace blink
