// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader.h"

#include <memory>
#include <string>
#include <utility>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/data_pipe_bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/testing/bytes_consumer_test_reader.h"
#include "third_party/blink/renderer/platform/loader/testing/replaying_bytes_consumer.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

class ResponseBodyLoaderTest : public testing::Test {
 protected:
  using Command = ReplayingBytesConsumer::Command;
  using PublicState = BytesConsumer::PublicState;
  using Result = BytesConsumer::Result;
  class TestClient final : public GarbageCollected<TestClient>,
                           public ResponseBodyLoaderClient {
    USING_GARBAGE_COLLECTED_MIXIN(TestClient);

   public:
    enum class Option {
      kNone,
      kAbortOnDidReceiveData,
      kSuspendOnDidReceiveData,
    };

    TestClient() : TestClient(Option::kNone) {}
    TestClient(Option option) : option_(option) {}
    ~TestClient() override {}

    String GetData() { return data_.ToString(); }
    bool LoadingIsFinished() const { return finished_; }
    bool LoadingIsFailed() const { return failed_; }
    bool LoadingIsCancelled() const { return cancelled_; }

    void DidReceiveData(base::span<const char> data) override {
      DCHECK(!finished_);
      DCHECK(!failed_);
      data_.Append(data.data(), data.size());
      switch (option_) {
        case Option::kNone:
          break;
        case Option::kAbortOnDidReceiveData:
          loader_->Abort();
          break;
        case Option::kSuspendOnDidReceiveData:
          loader_->Suspend();
          break;
      }
    }
    void DidFinishLoadingBody() override {
      DCHECK(!finished_);
      DCHECK(!failed_);
      finished_ = true;
    }
    void DidFailLoadingBody() override {
      DCHECK(!finished_);
      DCHECK(!failed_);
      failed_ = true;
    }
    void DidCancelLoadingBody() override {
      DCHECK(!finished_);
      DCHECK(!failed_);
      cancelled_ = true;
    }

    void SetLoader(ResponseBodyLoader& loader) { loader_ = loader; }
    void Trace(Visitor* visitor) override { visitor->Trace(loader_); }

   private:
    const Option option_;
    Member<ResponseBodyLoader> loader_;
    StringBuilder data_;
    bool finished_ = false;
    bool failed_ = false;
    bool cancelled_ = false;
  };

  class ReadingClient final : public GarbageCollected<ReadingClient>,
                              public BytesConsumer::Client {
    USING_GARBAGE_COLLECTED_MIXIN(ReadingClient);

   public:
    ReadingClient(BytesConsumer& bytes_consumer,
                  TestClient& test_response_body_loader_client)
        : bytes_consumer_(bytes_consumer),
          test_response_body_loader_client_(test_response_body_loader_client) {}

    void OnStateChangeInternal() {
      while (true) {
        const char* buffer = nullptr;
        size_t available = 0;
        Result result = bytes_consumer_->BeginRead(&buffer, &available);
        if (result == Result::kShouldWait)
          return;
        if (result == Result::kOk) {
          result = bytes_consumer_->EndRead(available);
        }
        if (result != Result::kOk)
          return;
      }
    }

    // BytesConsumer::Client implementation
    void OnStateChange() override {
      on_state_change_called_ = true;
      OnStateChangeInternal();
      // Notification is done asynchronously.
      EXPECT_FALSE(test_response_body_loader_client_->LoadingIsCancelled());
      EXPECT_FALSE(test_response_body_loader_client_->LoadingIsFinished());
      EXPECT_FALSE(test_response_body_loader_client_->LoadingIsFailed());
    }
    String DebugName() const override { return "ReadingClient"; }
    void Trace(Visitor* visitor) override {
      visitor->Trace(bytes_consumer_);
      visitor->Trace(test_response_body_loader_client_);
      BytesConsumer::Client::Trace(visitor);
    }

    bool IsOnStateChangeCalled() const { return on_state_change_called_; }

   private:
    bool on_state_change_called_ = false;
    const Member<BytesConsumer> bytes_consumer_;
    const Member<TestClient> test_response_body_loader_client_;
  };
};

class ResponseBodyLoaderDrainedBytesConsumerNotificationOutOfOnStateChangeTest
    : public ResponseBodyLoaderTest {};

class ResponseBodyLoaderDrainedBytesConsumerNotificationInOnStateChangeTest
    : public ResponseBodyLoaderTest {};

TEST_F(ResponseBodyLoaderTest, Load) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  consumer->Add(Command(Command::kData, "he"));
  consumer->Add(Command(Command::kWait));
  consumer->Add(Command(Command::kData, "llo"));
  consumer->Add(Command(Command::kDone));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader =
      MakeGarbageCollected<ResponseBodyLoader>(*consumer, *client, task_runner);

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_TRUE(client->GetData().IsEmpty());

  body_loader->Start();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("he", client->GetData());

  task_runner->RunUntilIdle();

  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("hello", client->GetData());
}

TEST_F(ResponseBodyLoaderTest, LoadFailure) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  consumer->Add(Command(Command::kData, "he"));
  consumer->Add(Command(Command::kWait));
  consumer->Add(Command(Command::kData, "llo"));
  consumer->Add(Command(Command::kError));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader =
      MakeGarbageCollected<ResponseBodyLoader>(*consumer, *client, task_runner);

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_TRUE(client->GetData().IsEmpty());

  body_loader->Start();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("he", client->GetData());

  task_runner->RunUntilIdle();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_TRUE(client->LoadingIsFailed());
  EXPECT_EQ("hello", client->GetData());
}

TEST_F(ResponseBodyLoaderTest, LoadWithDataAndDone) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  consumer->Add(Command(Command::kData, "he"));
  consumer->Add(Command(Command::kWait));
  consumer->Add(Command(Command::kDataAndDone, "llo"));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader =
      MakeGarbageCollected<ResponseBodyLoader>(*consumer, *client, task_runner);

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_TRUE(client->GetData().IsEmpty());

  body_loader->Start();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("he", client->GetData());

  task_runner->RunUntilIdle();

  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("hello", client->GetData());
}

TEST_F(ResponseBodyLoaderTest, Abort) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  consumer->Add(Command(Command::kData, "he"));
  consumer->Add(Command(Command::kWait));
  consumer->Add(Command(Command::kData, "llo"));
  consumer->Add(Command(Command::kDone));

  auto* client = MakeGarbageCollected<TestClient>(
      TestClient::Option::kAbortOnDidReceiveData);
  auto* body_loader =
      MakeGarbageCollected<ResponseBodyLoader>(*consumer, *client, task_runner);
  client->SetLoader(*body_loader);

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_TRUE(client->GetData().IsEmpty());
  EXPECT_FALSE(body_loader->IsAborted());

  body_loader->Start();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("he", client->GetData());
  EXPECT_TRUE(body_loader->IsAborted());

  task_runner->RunUntilIdle();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("he", client->GetData());
  EXPECT_TRUE(body_loader->IsAborted());
}

TEST_F(ResponseBodyLoaderTest, Suspend) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  consumer->Add(Command(Command::kData, "h"));
  consumer->Add(Command(Command::kDataAndDone, "ello"));

  auto* client = MakeGarbageCollected<TestClient>(
      TestClient::Option::kSuspendOnDidReceiveData);
  auto* body_loader =
      MakeGarbageCollected<ResponseBodyLoader>(*consumer, *client, task_runner);
  client->SetLoader(*body_loader);

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_TRUE(client->GetData().IsEmpty());
  EXPECT_FALSE(body_loader->IsSuspended());

  body_loader->Start();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("h", client->GetData());
  EXPECT_TRUE(body_loader->IsSuspended());

  task_runner->RunUntilIdle();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("h", client->GetData());
  EXPECT_TRUE(body_loader->IsSuspended());

  body_loader->Resume();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("h", client->GetData());
  EXPECT_FALSE(body_loader->IsSuspended());

  task_runner->RunUntilIdle();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("hello", client->GetData());
  EXPECT_TRUE(body_loader->IsSuspended());

  body_loader->Resume();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("hello", client->GetData());
  EXPECT_FALSE(body_loader->IsSuspended());

  task_runner->RunUntilIdle();

  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("hello", client->GetData());
  EXPECT_FALSE(body_loader->IsSuspended());
}

TEST_F(ResponseBodyLoaderTest, ReadTooBigBuffer) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  constexpr auto kMax = ResponseBodyLoader::kMaxNumConsumedBytesInTask;

  consumer->Add(Command(Command::kData, std::string(kMax - 1, 'a').data()));
  consumer->Add(Command(Command::kData, std::string(2, 'b').data()));
  consumer->Add(Command(Command::kWait));
  consumer->Add(Command(Command::kData, std::string(kMax, 'c').data()));
  consumer->Add(Command(Command::kData, std::string(kMax + 3, 'd').data()));
  consumer->Add(Command(Command::kDone));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader =
      MakeGarbageCollected<ResponseBodyLoader>(*consumer, *client, task_runner);

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_TRUE(client->GetData().IsEmpty());

  body_loader->Start();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ((std::string(kMax - 1, 'a') + 'b').data(), client->GetData());

  task_runner->RunUntilIdle();

  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ((std::string(kMax - 1, 'a') + "bb" + std::string(kMax, 'c') +
             std::string(kMax + 3, 'd'))
                .data(),
            client->GetData());
}

TEST_F(ResponseBodyLoaderTest, NotDrainable) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  consumer->Add(Command(Command::kData, "he"));
  consumer->Add(Command(Command::kWait));
  consumer->Add(Command(Command::kData, "llo"));
  consumer->Add(Command(Command::kDone));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader =
      MakeGarbageCollected<ResponseBodyLoader>(*consumer, *client, task_runner);

  ResponseBodyLoaderClient* intermediate_client = nullptr;
  auto data_pipe = body_loader->DrainAsDataPipe(&intermediate_client);

  ASSERT_FALSE(data_pipe);
  EXPECT_FALSE(intermediate_client);
  EXPECT_FALSE(body_loader->IsDrained());

  // We can start loading.

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_TRUE(client->GetData().IsEmpty());

  body_loader->Start();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("he", client->GetData());

  task_runner->RunUntilIdle();

  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("hello", client->GetData());
}

TEST_F(ResponseBodyLoaderTest, DrainAsDataPipe) {
  mojo::ScopedDataPipeConsumerHandle consumer_end;
  mojo::ScopedDataPipeProducerHandle producer_end;
  auto result = mojo::CreateDataPipe(nullptr, &producer_end, &consumer_end);

  ASSERT_EQ(result, MOJO_RESULT_OK);

  DataPipeBytesConsumer::CompletionNotifier* completion_notifier = nullptr;

  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner, std::move(consumer_end), &completion_notifier);
  auto* client = MakeGarbageCollected<TestClient>();

  auto* body_loader =
      MakeGarbageCollected<ResponseBodyLoader>(*consumer, *client, task_runner);

  ResponseBodyLoaderClient* client_for_draining = nullptr;
  auto data_pipe = body_loader->DrainAsDataPipe(&client_for_draining);

  ASSERT_TRUE(data_pipe);
  ASSERT_TRUE(client);
  EXPECT_TRUE(body_loader->IsDrained());

  client_for_draining->DidReceiveData(base::make_span("xyz", 3));
  client_for_draining->DidReceiveData(base::make_span("abc", 3));

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("xyzabc", client->GetData());

  client_for_draining->DidFinishLoadingBody();

  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("xyzabc", client->GetData());
}

TEST_F(ResponseBodyLoaderTest, DrainAsDataPipeAndReportError) {
  mojo::ScopedDataPipeConsumerHandle consumer_end;
  mojo::ScopedDataPipeProducerHandle producer_end;
  auto result = mojo::CreateDataPipe(nullptr, &producer_end, &consumer_end);

  ASSERT_EQ(result, MOJO_RESULT_OK);

  DataPipeBytesConsumer::CompletionNotifier* completion_notifier = nullptr;

  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner, std::move(consumer_end), &completion_notifier);
  auto* client = MakeGarbageCollected<TestClient>();

  auto* body_loader =
      MakeGarbageCollected<ResponseBodyLoader>(*consumer, *client, task_runner);

  ResponseBodyLoaderClient* client_for_draining = nullptr;
  auto data_pipe = body_loader->DrainAsDataPipe(&client_for_draining);

  ASSERT_TRUE(data_pipe);
  ASSERT_TRUE(client);
  EXPECT_TRUE(body_loader->IsDrained());

  client_for_draining->DidReceiveData(base::make_span("xyz", 3));
  client_for_draining->DidReceiveData(base::make_span("abc", 3));

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("xyzabc", client->GetData());

  client_for_draining->DidFailLoadingBody();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_TRUE(client->LoadingIsFailed());
  EXPECT_EQ("xyzabc", client->GetData());
}

TEST_F(ResponseBodyLoaderTest, DrainAsBytesConsumer) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* original_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  original_consumer->Add(Command(Command::kData, "he"));
  original_consumer->Add(Command(Command::kWait));
  original_consumer->Add(Command(Command::kData, "l"));
  original_consumer->Add(Command(Command::kData, "lo"));
  original_consumer->Add(Command(Command::kDone));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader = MakeGarbageCollected<ResponseBodyLoader>(
      *original_consumer, *client, task_runner);

  BytesConsumer& consumer = body_loader->DrainAsBytesConsumer();

  EXPECT_TRUE(body_loader->IsDrained());
  EXPECT_NE(&consumer, original_consumer);

  auto* reader = MakeGarbageCollected<BytesConsumerTestReader>(&consumer);

  auto result = reader->Run(task_runner.get());
  EXPECT_EQ(result.first, BytesConsumer::Result::kDone);
  EXPECT_EQ(String(result.second.data(), result.second.size()), "hello");
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("hello", client->GetData());
}

TEST_F(ResponseBodyLoaderTest, CancelDrainedBytesConsumer) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* original_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  original_consumer->Add(Command(Command::kData, "he"));
  original_consumer->Add(Command(Command::kWait));
  original_consumer->Add(Command(Command::kData, "llo"));
  original_consumer->Add(Command(Command::kDone));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader = MakeGarbageCollected<ResponseBodyLoader>(
      *original_consumer, *client, task_runner);

  BytesConsumer& consumer = body_loader->DrainAsBytesConsumer();

  EXPECT_TRUE(body_loader->IsDrained());
  EXPECT_NE(&consumer, original_consumer);
  consumer.Cancel();

  auto* reader = MakeGarbageCollected<BytesConsumerTestReader>(&consumer);

  auto result = reader->Run(task_runner.get());
  EXPECT_EQ(result.first, BytesConsumer::Result::kDone);
  EXPECT_EQ(String(result.second.data(), result.second.size()), String());

  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  task_runner->RunUntilIdle();

  EXPECT_TRUE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
}

TEST_F(ResponseBodyLoaderTest, DrainAsBytesConsumerWithError) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* original_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  original_consumer->Add(Command(Command::kData, "he"));
  original_consumer->Add(Command(Command::kWait));
  original_consumer->Add(Command(Command::kData, "llo"));
  original_consumer->Add(Command(Command::kError));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader = MakeGarbageCollected<ResponseBodyLoader>(
      *original_consumer, *client, task_runner);

  BytesConsumer& consumer = body_loader->DrainAsBytesConsumer();

  EXPECT_TRUE(body_loader->IsDrained());
  EXPECT_NE(&consumer, original_consumer);

  auto* reader = MakeGarbageCollected<BytesConsumerTestReader>(&consumer);

  auto result = reader->Run(task_runner.get());
  EXPECT_EQ(result.first, BytesConsumer::Result::kError);
  EXPECT_EQ(String(result.second.data(), result.second.size()), "hello");
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_TRUE(client->LoadingIsFailed());
}

TEST_F(ResponseBodyLoaderTest, AbortAfterBytesConsumerIsDrained) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* original_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  original_consumer->Add(Command(Command::kData, "he"));
  original_consumer->Add(Command(Command::kWait));
  original_consumer->Add(Command(Command::kData, "llo"));
  original_consumer->Add(Command(Command::kDone));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader = MakeGarbageCollected<ResponseBodyLoader>(
      *original_consumer, *client, task_runner);

  BytesConsumer& consumer = body_loader->DrainAsBytesConsumer();
  auto* bytes_consumer_client =
      MakeGarbageCollected<ReadingClient>(consumer, *client);
  consumer.SetClient(bytes_consumer_client);

  EXPECT_TRUE(body_loader->IsDrained());
  EXPECT_NE(&consumer, original_consumer);

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer.GetPublicState());
  EXPECT_FALSE(bytes_consumer_client->IsOnStateChangeCalled());
  body_loader->Abort();
  EXPECT_EQ(PublicState::kErrored, consumer.GetPublicState());
  EXPECT_TRUE(bytes_consumer_client->IsOnStateChangeCalled());

  task_runner->RunUntilIdle();

  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
}

TEST_F(ResponseBodyLoaderTest, AbortAfterBytesConsumerIsDrainedIsNotified) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* original_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader = MakeGarbageCollected<ResponseBodyLoader>(
      *original_consumer, *client, task_runner);

  BytesConsumer& consumer = body_loader->DrainAsBytesConsumer();

  EXPECT_TRUE(body_loader->IsDrained());
  EXPECT_NE(&consumer, original_consumer);

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer.GetPublicState());
  body_loader->Abort();
  EXPECT_EQ(PublicState::kErrored, consumer.GetPublicState());

  task_runner->RunUntilIdle();

  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
}

TEST_F(ResponseBodyLoaderDrainedBytesConsumerNotificationOutOfOnStateChangeTest,
       BeginReadAndDone) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* original_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  original_consumer->Add(Command(Command::kWait));
  original_consumer->Add(Command(Command::kDataAndDone, "hello"));
  original_consumer->Add(Command(Command::kDone));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader = MakeGarbageCollected<ResponseBodyLoader>(
      *original_consumer, *client, task_runner);
  BytesConsumer& consumer = body_loader->DrainAsBytesConsumer();

  const char* buffer = nullptr;
  size_t available = 0;
  Result result = consumer.BeginRead(&buffer, &available);

  EXPECT_EQ(result, Result::kShouldWait);
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  task_runner->RunUntilIdle();
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  result = consumer.BeginRead(&buffer, &available);
  EXPECT_EQ(result, Result::kOk);
  ASSERT_EQ(available, 5u);
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  result = consumer.EndRead(available);
  EXPECT_EQ(result, Result::kDone);
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  task_runner->RunUntilIdle();
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
}

TEST_F(ResponseBodyLoaderDrainedBytesConsumerNotificationOutOfOnStateChangeTest,
       BeginReadAndError) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* original_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  original_consumer->Add(Command(Command::kWait));
  original_consumer->Add(Command(Command::kData, "hello"));
  original_consumer->Add(Command(Command::kError));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader = MakeGarbageCollected<ResponseBodyLoader>(
      *original_consumer, *client, task_runner);
  BytesConsumer& consumer = body_loader->DrainAsBytesConsumer();

  const char* buffer = nullptr;
  size_t available = 0;
  Result result = consumer.BeginRead(&buffer, &available);

  EXPECT_EQ(result, Result::kShouldWait);
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  task_runner->RunUntilIdle();
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  result = consumer.BeginRead(&buffer, &available);
  EXPECT_EQ(result, Result::kOk);
  ASSERT_EQ(available, 5u);
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  result = consumer.EndRead(available);
  EXPECT_EQ(result, Result::kOk);
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  result = consumer.BeginRead(&buffer, &available);
  EXPECT_EQ(result, Result::kError);
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  task_runner->RunUntilIdle();
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_TRUE(client->LoadingIsFailed());
}

TEST_F(ResponseBodyLoaderDrainedBytesConsumerNotificationOutOfOnStateChangeTest,
       EndReadAndDone) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* original_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  original_consumer->Add(Command(Command::kWait));
  original_consumer->Add(Command(Command::kDataAndDone, "hello"));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader = MakeGarbageCollected<ResponseBodyLoader>(
      *original_consumer, *client, task_runner);
  BytesConsumer& consumer = body_loader->DrainAsBytesConsumer();

  const char* buffer = nullptr;
  size_t available = 0;
  Result result = consumer.BeginRead(&buffer, &available);

  EXPECT_EQ(result, Result::kShouldWait);
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  task_runner->RunUntilIdle();
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  result = consumer.BeginRead(&buffer, &available);
  EXPECT_EQ(result, Result::kOk);
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  ASSERT_EQ(5u, available);
  EXPECT_EQ(String(buffer, available), "hello");

  task_runner->RunUntilIdle();
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  result = consumer.EndRead(available);
  EXPECT_EQ(result, Result::kDone);
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  task_runner->RunUntilIdle();
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
}

TEST_F(ResponseBodyLoaderDrainedBytesConsumerNotificationOutOfOnStateChangeTest,
       DrainAsDataPipe) {
  mojo::ScopedDataPipeConsumerHandle consumer_end;
  mojo::ScopedDataPipeProducerHandle producer_end;
  auto result = mojo::CreateDataPipe(nullptr, &producer_end, &consumer_end);

  ASSERT_EQ(result, MOJO_RESULT_OK);

  DataPipeBytesConsumer::CompletionNotifier* completion_notifier = nullptr;

  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* original_consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner, std::move(consumer_end), &completion_notifier);
  auto* client = MakeGarbageCollected<TestClient>();

  auto* body_loader = MakeGarbageCollected<ResponseBodyLoader>(
      *original_consumer, *client, task_runner);

  BytesConsumer& consumer = body_loader->DrainAsBytesConsumer();

  EXPECT_TRUE(consumer.DrainAsDataPipe());

  task_runner->RunUntilIdle();

  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  completion_notifier->SignalComplete();

  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
}

TEST_F(ResponseBodyLoaderDrainedBytesConsumerNotificationOutOfOnStateChangeTest,
       Cancel) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* original_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  original_consumer->Add(Command(Command::kWait));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader = MakeGarbageCollected<ResponseBodyLoader>(
      *original_consumer, *client, task_runner);
  BytesConsumer& consumer = body_loader->DrainAsBytesConsumer();

  task_runner->RunUntilIdle();
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  consumer.Cancel();
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  task_runner->RunUntilIdle();
  EXPECT_TRUE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
}

TEST_F(ResponseBodyLoaderDrainedBytesConsumerNotificationInOnStateChangeTest,
       BeginReadAndDone) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* original_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  original_consumer->Add(Command(Command::kWait));
  original_consumer->Add(Command(Command::kDone));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader = MakeGarbageCollected<ResponseBodyLoader>(
      *original_consumer, *client, task_runner);

  BytesConsumer& consumer = body_loader->DrainAsBytesConsumer();
  auto* reading_client = MakeGarbageCollected<ReadingClient>(consumer, *client);
  consumer.SetClient(reading_client);

  const char* buffer = nullptr;
  size_t available = 0;
  // This BeginRead posts a task which calls OnStateChange.
  Result result = consumer.BeginRead(&buffer, &available);
  EXPECT_EQ(result, Result::kShouldWait);

  // We'll see the change without waiting for another task.
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(
                            [](TestClient* client) {
                              EXPECT_FALSE(client->LoadingIsCancelled());
                              EXPECT_TRUE(client->LoadingIsFinished());
                              EXPECT_FALSE(client->LoadingIsFailed());
                            },
                            WrapPersistent(client)));

  task_runner->RunUntilIdle();

  EXPECT_TRUE(reading_client->IsOnStateChangeCalled());
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
}

TEST_F(ResponseBodyLoaderDrainedBytesConsumerNotificationInOnStateChangeTest,
       BeginReadAndError) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* original_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  original_consumer->Add(Command(Command::kWait));
  original_consumer->Add(Command(Command::kError));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader = MakeGarbageCollected<ResponseBodyLoader>(
      *original_consumer, *client, task_runner);

  BytesConsumer& consumer = body_loader->DrainAsBytesConsumer();
  auto* reading_client = MakeGarbageCollected<ReadingClient>(consumer, *client);
  consumer.SetClient(reading_client);

  const char* buffer = nullptr;
  size_t available = 0;
  // This BeginRead posts a task which calls OnStateChange.
  Result result = consumer.BeginRead(&buffer, &available);
  EXPECT_EQ(result, Result::kShouldWait);

  // We'll see the change without waiting for another task.
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(
                            [](TestClient* client) {
                              EXPECT_FALSE(client->LoadingIsCancelled());
                              EXPECT_FALSE(client->LoadingIsFinished());
                              EXPECT_TRUE(client->LoadingIsFailed());
                            },
                            WrapPersistent(client)));

  task_runner->RunUntilIdle();

  EXPECT_TRUE(reading_client->IsOnStateChangeCalled());
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_TRUE(client->LoadingIsFailed());
}

TEST_F(ResponseBodyLoaderDrainedBytesConsumerNotificationInOnStateChangeTest,
       EndReadAndDone) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* original_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  original_consumer->Add(Command(Command::kWait));
  original_consumer->Add(Command(Command::kDataAndDone, "hahaha"));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader = MakeGarbageCollected<ResponseBodyLoader>(
      *original_consumer, *client, task_runner);

  BytesConsumer& consumer = body_loader->DrainAsBytesConsumer();
  auto* reading_client = MakeGarbageCollected<ReadingClient>(consumer, *client);
  consumer.SetClient(reading_client);

  const char* buffer = nullptr;
  size_t available = 0;
  // This BeginRead posts a task which calls OnStateChange.
  Result result = consumer.BeginRead(&buffer, &available);
  EXPECT_EQ(result, Result::kShouldWait);

  // We'll see the change without waiting for another task.
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(
                            [](TestClient* client) {
                              EXPECT_FALSE(client->LoadingIsCancelled());
                              EXPECT_TRUE(client->LoadingIsFinished());
                              EXPECT_FALSE(client->LoadingIsFailed());
                            },
                            WrapPersistent(client)));

  task_runner->RunUntilIdle();

  EXPECT_TRUE(reading_client->IsOnStateChangeCalled());
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
}

}  // namespace

}  // namespace blink
