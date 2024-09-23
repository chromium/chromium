// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader.h"

#include <memory>
#include <string>
#include <utility>
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/back_forward_cache_loader_helper.h"
#include "third_party/blink/renderer/platform/loader/fetch/data_pipe_bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/testing/bytes_consumer_test_reader.h"
#include "third_party/blink/renderer/platform/loader/testing/replaying_bytes_consumer.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

class TestBackForwardCacheLoaderHelper : public BackForwardCacheLoaderHelper {
 public:
  TestBackForwardCacheLoaderHelper() = default;

  void EvictFromBackForwardCache(
      mojom::blink::RendererEvictionReason reason) override {}

  void DidBufferLoadWhileInBackForwardCache(bool update_process_wide_count,
                                            size_t num_bytes) override {}

  void Detach() override {}
};

class ResponseBodyLoaderTest : public testing::Test {
 protected:
  using Command = ReplayingBytesConsumer::Command;
  using PublicState = BytesConsumer::PublicState;
  using Result = BytesConsumer::Result;

  class TestClient final : public GarbageCollected<TestClient>,
                           public ResponseBodyLoaderClient {
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
      data_.Append(data.data(), static_cast<wtf_size_t>(data.size()));
      switch (option_) {
        case Option::kNone:
          break;
        case Option::kAbortOnDidReceiveData:
          loader_->Abort();
          break;
        case Option::kSuspendOnDidReceiveData:
          loader_->Suspend(LoaderFreezeMode::kStrict);
          break;
      }
    }
    void DidReceiveDecodedData(
        const String& data,
        std::unique_ptr<ParkableStringImpl::SecureDigest> digest) override {}
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
    void Trace(Visitor* visitor) const override { visitor->Trace(loader_); }

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
    void Trace(Visitor* visitor) const override {
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

  ResponseBodyLoader* MakeResponseBodyLoader(
      BytesConsumer& bytes_consumer,
      ResponseBodyLoaderClient& client,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    return MakeGarbageCollected<ResponseBodyLoader>(
        bytes_consumer, client, task_runner,
        MakeGarbageCollected<TestBackForwardCacheLoaderHelper>());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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
  auto* body_loader = MakeResponseBodyLoader(*consumer, *client, task_runner);

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_TRUE(client->GetData().empty());

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
  auto* body_loader = MakeResponseBodyLoader(*consumer, *client, task_runner);

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_TRUE(client->GetData().empty());

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
  auto* body_loader = MakeResponseBodyLoader(*consumer, *client, task_runner);

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_TRUE(client->GetData().empty());

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
  auto* body_loader = MakeResponseBodyLoader(*consumer, *client, task_runner);
  client->SetLoader(*body_loader);

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_TRUE(client->GetData().empty());
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
  auto* body_loader = MakeResponseBodyLoader(*consumer, *client, task_runner);
  client->SetLoader(*body_loader);

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_TRUE(client->GetData().empty());
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
  const size_t kMax = network::features::GetLoaderChunkSize();

  consumer->Add(Command(Command::kData, std::string(kMax - 1, 'a').data()));
  consumer->Add(Command(Command::kData, std::string(2, 'b').data()));
  consumer->Add(Command(Command::kWait));
  consumer->Add(Command(Command::kData, std::string(kMax, 'c').data()));
  consumer->Add(Command(Command::kData, std::string(kMax + 3, 'd').data()));
  consumer->Add(Command(Command::kDone));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader = MakeResponseBodyLoader(*consumer, *client, task_runner);

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_TRUE(client->GetData().empty());

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
  auto* body_loader = MakeResponseBodyLoader(*consumer, *client, task_runner);

  ResponseBodyLoaderClient* intermediate_client = nullptr;
  auto data_pipe = body_loader->DrainAsDataPipe(&intermediate_client);

  ASSERT_FALSE(data_pipe);
  EXPECT_FALSE(intermediate_client);
  EXPECT_FALSE(body_loader->IsDrained());

  // We can start loading.

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_TRUE(client->GetData().empty());

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
  auto result = mojo::CreateDataPipe(nullptr, producer_end, consumer_end);

  ASSERT_EQ(result, MOJO_RESULT_OK);

  DataPipeBytesConsumer::CompletionNotifier* completion_notifier = nullptr;

  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner, std::move(consumer_end), &completion_notifier);
  auto* client = MakeGarbageCollected<TestClient>();

  auto* body_loader = MakeResponseBodyLoader(*consumer, *client, task_runner);

  ResponseBodyLoaderClient* client_for_draining = nullptr;
  auto data_pipe = body_loader->DrainAsDataPipe(&client_for_draining);

  ASSERT_TRUE(data_pipe);
  ASSERT_TRUE(client);
  EXPECT_TRUE(body_loader->IsDrained());

  client_for_draining->DidReceiveData(base::make_span("xyz", 3u));
  client_for_draining->DidReceiveData(base::make_span("abc", 3u));

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("xyzabc", client->GetData());

  client_for_draining->DidFinishLoadingBody();

  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("xyzabc", client->GetData());
}

class ResponseBodyLoaderLoadingTasksUnfreezableTest
    : public ResponseBodyLoaderTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  ResponseBodyLoaderLoadingTasksUnfreezableTest() {
    if (DeferWithBackForwardCacheEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kLoadingTasksUnfreezable);
    }
    WebRuntimeFeatures::EnableBackForwardCache(
        DeferWithBackForwardCacheEnabled());
  }

  bool DeferWithBackForwardCacheEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(ResponseBodyLoaderLoadingTasksUnfreezableTest,
       SuspendedThenSuspendedForBackForwardCacheThenResume) {
  if (!DeferWithBackForwardCacheEnabled())
    return;
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader = MakeResponseBodyLoader(*consumer, *client, task_runner);
  consumer->Add(Command(Command::kData, "he"));
  body_loader->Start();
  task_runner->RunUntilIdle();
  EXPECT_EQ("he", client->GetData());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  // Suspend (not for back-forward cache), then add some data to |consumer|.
  body_loader->Suspend(LoaderFreezeMode::kStrict);
  consumer->Add(Command(Command::kData, "llo"));
  EXPECT_FALSE(consumer->IsCommandsEmpty());
  // Simulate the "readable again" signal.
  consumer->TriggerOnStateChange();
  task_runner->RunUntilIdle();

  // When suspended not for back-forward cache, ResponseBodyLoader won't consume
  // the data.
  EXPECT_FALSE(consumer->IsCommandsEmpty());
  EXPECT_EQ("he", client->GetData());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  // Suspend for back-forward cache, then add some more data to |consumer|.
  body_loader->Suspend(LoaderFreezeMode::kBufferIncoming);
  consumer->Add(Command(Command::kData, "w"));
  consumer->Add(Command(Command::kWait));
  consumer->Add(Command(Command::kData, "o"));

  // ResponseBodyLoader will buffer data when deferred for back-forward cache,
  // but won't notify the client until it's resumed.
  EXPECT_FALSE(consumer->IsCommandsEmpty());
  task_runner->RunUntilIdle();
  EXPECT_TRUE(consumer->IsCommandsEmpty());

  EXPECT_EQ("he", client->GetData());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  // The data received while suspended will be processed after resuming, before
  // processing newer data.
  body_loader->Resume();
  consumer->Add(Command(Command::kData, "rld"));
  consumer->Add(Command(Command::kDone));

  task_runner->RunUntilIdle();
  EXPECT_EQ("helloworld", client->GetData());
  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
}

TEST_P(ResponseBodyLoaderLoadingTasksUnfreezableTest,
       FinishedWhileSuspendedThenSuspendedForBackForwardCacheThenResume) {
  if (!DeferWithBackForwardCacheEnabled())
    return;
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader = MakeResponseBodyLoader(*consumer, *client, task_runner);
  consumer->Add(Command(Command::kData, "he"));
  body_loader->Start();
  task_runner->RunUntilIdle();
  EXPECT_EQ("he", client->GetData());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  // Suspend (not for back-forward cache), then add some data to |consumer| with
  // the finish signal at the end.
  body_loader->Suspend(LoaderFreezeMode::kStrict);
  consumer->Add(Command(Command::kData, "llo"));
  consumer->Add(Command(Command::kDone));
  // Simulate the "readable again" signal.
  consumer->TriggerOnStateChange();
  EXPECT_FALSE(consumer->IsCommandsEmpty());
  task_runner->RunUntilIdle();

  // When suspended not for back-forward cache, ResponseBodyLoader won't consume
  // the data, including the finish signal.
  EXPECT_FALSE(consumer->IsCommandsEmpty());
  EXPECT_EQ("he", client->GetData());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  // Suspend for back-forward cache.
  body_loader->Suspend(LoaderFreezeMode::kBufferIncoming);
  // ResponseBodyLoader will buffer data when deferred for back-forward cache,
  // but won't notify the client until it's resumed.
  EXPECT_FALSE(consumer->IsCommandsEmpty());
  task_runner->RunUntilIdle();
  EXPECT_TRUE(consumer->IsCommandsEmpty());

  EXPECT_EQ("he", client->GetData());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  // The data received while suspended will be processed after resuming,
  // including the finish signal.
  body_loader->Resume();
  task_runner->RunUntilIdle();
  EXPECT_EQ("hello", client->GetData());
  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
}

TEST_P(ResponseBodyLoaderLoadingTasksUnfreezableTest,
       SuspendedForBackForwardCacheThenSuspendedThenResume) {
  if (!DeferWithBackForwardCacheEnabled())
    return;
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader = MakeResponseBodyLoader(*consumer, *client, task_runner);
  consumer->Add(Command(Command::kData, "he"));
  body_loader->Start();
  task_runner->RunUntilIdle();

  EXPECT_EQ("he", client->GetData());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  // Suspend for back-forward cache, then add some more data to |consumer|.
  body_loader->Suspend(LoaderFreezeMode::kBufferIncoming);
  consumer->Add(Command(Command::kData, "llo"));
  EXPECT_FALSE(consumer->IsCommandsEmpty());
  // Simulate the "readable again" signal.
  consumer->TriggerOnStateChange();

  // ResponseBodyLoader will buffer data  when deferred for back-forward cache,
  // but won't notify the client until it's resumed.
  while (!consumer->IsCommandsEmpty()) {
    task_runner->RunUntilIdle();
  }

  EXPECT_EQ("he", client->GetData());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  // Suspend (not for back-forward cache), then add some data to |consumer|.
  body_loader->Suspend(LoaderFreezeMode::kStrict);
  consumer->Add(Command(Command::kData, "w"));
  consumer->Add(Command(Command::kWait));
  consumer->Add(Command(Command::kData, "o"));

  // When suspended not for back-forward cache, ResponseBodyLoader won't consume
  // the data, even with OnStateChange triggered.
  for (int i = 0; i < 3; ++i) {
    consumer->TriggerOnStateChange();
    task_runner->RunUntilIdle();
  }
  EXPECT_FALSE(consumer->IsCommandsEmpty());
  EXPECT_EQ("he", client->GetData());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  // The data received while suspended will be processed after resuming, before
  // processing newer data.
  body_loader->Resume();
  consumer->Add(Command(Command::kData, "rld"));
  consumer->Add(Command(Command::kDone));

  task_runner->RunUntilIdle();
  EXPECT_EQ("helloworld", client->GetData());
  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
}

TEST_P(ResponseBodyLoaderLoadingTasksUnfreezableTest,
       ReadDataFromConsumerWhileSuspendedForBackForwardCacheLong) {
  if (!DeferWithBackForwardCacheEnabled())
    return;
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader = MakeResponseBodyLoader(*consumer, *client, task_runner);
  body_loader->Start();
  task_runner->RunUntilIdle();
  EXPECT_EQ("", client->GetData());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  // Suspend, then add a long response body to |consumer|.
  body_loader->Suspend(LoaderFreezeMode::kBufferIncoming);
  std::string body(70000, '*');
  consumer->Add(Command(Command::kDataAndDone, body.c_str()));

  // ResponseBodyLoader will buffer data when deferred, and won't notify the
  // client until it's resumed.
  EXPECT_FALSE(consumer->IsCommandsEmpty());
  // Simulate the "readable" signal.
  consumer->TriggerOnStateChange();
  while (!consumer->IsCommandsEmpty()) {
    task_runner->RunUntilIdle();
  }

  EXPECT_EQ("", client->GetData());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());

  // The data received while suspended will be processed after resuming.
  body_loader->Resume();
  task_runner->RunUntilIdle();
  EXPECT_EQ(AtomicString(body.c_str()), client->GetData());
  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ResponseBodyLoaderLoadingTasksUnfreezableTest,
                         ::testing::Bool());

TEST_F(ResponseBodyLoaderTest, DrainAsDataPipeAndReportError) {
  mojo::ScopedDataPipeConsumerHandle consumer_end;
  mojo::ScopedDataPipeProducerHandle producer_end;
  auto result = mojo::CreateDataPipe(nullptr, producer_end, consumer_end);

  ASSERT_EQ(result, MOJO_RESULT_OK);

  DataPipeBytesConsumer::CompletionNotifier* completion_notifier = nullptr;

  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner, std::move(consumer_end), &completion_notifier);
  auto* client = MakeGarbageCollected<TestClient>();

  auto* body_loader = MakeResponseBodyLoader(*consumer, *client, task_runner);

  ResponseBodyLoaderClient* client_for_draining = nullptr;
  auto data_pipe = body_loader->DrainAsDataPipe(&client_for_draining);

  ASSERT_TRUE(data_pipe);
  ASSERT_TRUE(client);
  EXPECT_TRUE(body_loader->IsDrained());

  client_for_draining->DidReceiveData(base::make_span("xyz", 3u));
  client_for_draining->DidReceiveData(base::make_span("abc", 3u));

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

  auto* body_loader =
      MakeResponseBodyLoader(*original_consumer, *client, task_runner);

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

  auto* body_loader =
      MakeResponseBodyLoader(*original_consumer, *client, task_runner);

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

TEST_F(ResponseBodyLoaderTest, AbortDrainAsBytesConsumerWhileLoading) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* original_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  original_consumer->Add(Command(Command::kData, "hello"));
  original_consumer->Add(Command(Command::kDone));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader =
      MakeResponseBodyLoader(*original_consumer, *client, task_runner);
  BytesConsumer& consumer = body_loader->DrainAsBytesConsumer();

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer.GetPublicState());

  body_loader->Abort();
  EXPECT_EQ(PublicState::kErrored, consumer.GetPublicState());
  EXPECT_EQ("Response body loading was aborted", consumer.GetError().Message());
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

  auto* body_loader =
      MakeResponseBodyLoader(*original_consumer, *client, task_runner);

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

  auto* body_loader =
      MakeResponseBodyLoader(*original_consumer, *client, task_runner);

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

  auto* body_loader =
      MakeResponseBodyLoader(*original_consumer, *client, task_runner);

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

  auto* body_loader =
      MakeResponseBodyLoader(*original_consumer, *client, task_runner);
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

  auto* body_loader =
      MakeResponseBodyLoader(*original_consumer, *client, task_runner);
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

  auto* body_loader =
      MakeResponseBodyLoader(*original_consumer, *client, task_runner);
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
  auto result = mojo::CreateDataPipe(nullptr, producer_end, consumer_end);

  ASSERT_EQ(result, MOJO_RESULT_OK);

  DataPipeBytesConsumer::CompletionNotifier* completion_notifier = nullptr;

  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* original_consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner, std::move(consumer_end), &completion_notifier);
  auto* client = MakeGarbageCollected<TestClient>();

  auto* body_loader =
      MakeResponseBodyLoader(*original_consumer, *client, task_runner);

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

  auto* body_loader =
      MakeResponseBodyLoader(*original_consumer, *client, task_runner);
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

  auto* body_loader =
      MakeResponseBodyLoader(*original_consumer, *client, task_runner);

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

  auto* body_loader =
      MakeResponseBodyLoader(*original_consumer, *client, task_runner);

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

  auto* body_loader =
      MakeResponseBodyLoader(*original_consumer, *client, task_runner);

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

class ResponseBodyLoaderTestAllowDrainAsBytesConsumerInBFCache
    : public ResponseBodyLoaderTest {
 protected:
  ResponseBodyLoaderTestAllowDrainAsBytesConsumerInBFCache() {
    scoped_feature_list_.InitWithFeatures(
        {features::kAllowDatapipeDrainedAsBytesConsumerInBFCache,
         features::kLoadingTasksUnfreezable},
        {});
    WebRuntimeFeatures::EnableBackForwardCache(true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that when response loader is suspended for back/forward cache and the
// datapipe is drained as bytes consumer, the data keeps processing without
// firing `DidFinishLoadingBody()`, which will be dispatched after resume.
TEST_F(ResponseBodyLoaderTestAllowDrainAsBytesConsumerInBFCache,
       DrainAsBytesConsumer) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* original_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  original_consumer->Add(Command(Command::kData, "he"));
  original_consumer->Add(Command(Command::kWait));
  original_consumer->Add(Command(Command::kData, "l"));
  original_consumer->Add(Command(Command::kData, "lo"));

  auto* client = MakeGarbageCollected<TestClient>();

  auto* body_loader =
      MakeResponseBodyLoader(*original_consumer, *client, task_runner);

  BytesConsumer& consumer = body_loader->DrainAsBytesConsumer();

  EXPECT_TRUE(body_loader->IsDrained());
  EXPECT_NE(&consumer, original_consumer);

  // Suspend for back-forward cache, then add some more data to |consumer|.
  body_loader->Suspend(LoaderFreezeMode::kBufferIncoming);
  original_consumer->Add(Command(Command::kData, "world"));
  original_consumer->Add(Command(Command::kDone));

  auto* reader = MakeGarbageCollected<BytesConsumerTestReader>(&consumer);

  auto result = reader->Run(task_runner.get());
  EXPECT_EQ(result.first, BytesConsumer::Result::kDone);
  EXPECT_EQ(String(result.second.data(), result.second.size()), "helloworld");
  // Check that `DidFinishLoadingBody()` has not been called.
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("helloworld", client->GetData());

  // Resume the body loader.
  body_loader->Resume();
  task_runner->RunUntilIdle();
  // Check that `DidFinishLoadingBody()` has now been called.
  EXPECT_FALSE(client->LoadingIsCancelled());
  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
}

}  // namespace

}  // namespace blink
