/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader_client.h"
#include "third_party/blink/renderer/platform/loader/testing/replaying_bytes_consumer.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

class RawResourceTest : public testing::Test {
 public:
  RawResourceTest() = default;
  ~RawResourceTest() override = default;

 protected:
  class NoopResponseBodyLoaderClient
      : public GarbageCollected<NoopResponseBodyLoaderClient>,
        public ResponseBodyLoaderClient {
    USING_GARBAGE_COLLECTED_MIXIN(NoopResponseBodyLoaderClient);

   public:
    ~NoopResponseBodyLoaderClient() override {}
    void DidReceiveData(base::span<const char>) override {}
    void DidFinishLoadingBody() override {}
    void DidFailLoadingBody() override {}
    void DidCancelLoadingBody() override {}
  };

  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RawResourceTest);
};

TEST_F(RawResourceTest, DontIgnoreAcceptForCacheReuse) {
  scoped_refptr<const SecurityOrigin> source_origin =
      SecurityOrigin::CreateUniqueOpaque();

  ResourceRequest jpeg_request;
  jpeg_request.SetHTTPAccept("image/jpeg");
  jpeg_request.SetRequestorOrigin(source_origin);

  RawResource* jpeg_resource(
      RawResource::CreateForTest(jpeg_request, ResourceType::kRaw));

  ResourceRequest png_request;
  png_request.SetHTTPAccept("image/png");
  png_request.SetRequestorOrigin(source_origin);
  EXPECT_NE(jpeg_resource->CanReuse(FetchParameters(png_request)),
            Resource::MatchStatus::kOk);
}

class DummyClient final : public GarbageCollected<DummyClient>,
                          public RawResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(DummyClient);

 public:
  DummyClient() : called_(false), number_of_redirects_received_(0) {}
  ~DummyClient() override = default;

  // ResourceClient implementation.
  void NotifyFinished(Resource* resource) override { called_ = true; }
  String DebugName() const override { return "DummyClient"; }

  void DataReceived(Resource*, const char* data, size_t length) override {
    data_.Append(data, SafeCast<wtf_size_t>(length));
  }

  bool RedirectReceived(Resource*,
                        const ResourceRequest&,
                        const ResourceResponse&) override {
    ++number_of_redirects_received_;
    return true;
  }

  bool Called() { return called_; }
  int NumberOfRedirectsReceived() const {
    return number_of_redirects_received_;
  }
  const Vector<char>& Data() { return data_; }
  void Trace(blink::Visitor* visitor) override {
    RawResourceClient::Trace(visitor);
  }

 private:
  bool called_;
  int number_of_redirects_received_;
  Vector<char> data_;
};

// This client adds another client when notified.
class AddingClient final : public GarbageCollected<AddingClient>,
                           public RawResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(AddingClient);

 public:
  AddingClient(DummyClient* client, Resource* resource)
      : dummy_client_(client), resource_(resource) {}

  ~AddingClient() override = default;

  // ResourceClient implementation.
  void NotifyFinished(Resource* resource) override {
    auto* platform = static_cast<TestingPlatformSupportWithMockScheduler*>(
        Platform::Current());

    // First schedule an asynchronous task to remove the client.
    // We do not expect a client to be called if the client is removed before
    // a callback invocation task queued inside addClient() is scheduled.
    platform->test_task_runner()->PostTask(
        FROM_HERE,
        WTF::Bind(&AddingClient::RemoveClient, WrapPersistent(this)));
    resource->AddClient(dummy_client_, platform->test_task_runner().get());
  }
  String DebugName() const override { return "AddingClient"; }

  void RemoveClient() { resource_->RemoveClient(dummy_client_); }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(dummy_client_);
    visitor->Trace(resource_);
    RawResourceClient::Trace(visitor);
  }

 private:
  Member<DummyClient> dummy_client_;
  Member<Resource> resource_;
};

TEST_F(RawResourceTest, AddClientDuringCallback) {
  Resource* raw = RawResource::CreateForTest(
      KURL("data:text/html,"), SecurityOrigin::CreateUniqueOpaque(),
      ResourceType::kRaw);
  raw->SetResponse(ResourceResponse(KURL("http://600.613/")));
  raw->FinishForTest();
  EXPECT_FALSE(raw->GetResponse().IsNull());

  Persistent<DummyClient> dummy_client = MakeGarbageCollected<DummyClient>();
  Persistent<AddingClient> adding_client =
      MakeGarbageCollected<AddingClient>(dummy_client.Get(), raw);
  raw->AddClient(adding_client, platform_->test_task_runner().get());
  platform_->RunUntilIdle();
  raw->RemoveClient(adding_client);
  EXPECT_FALSE(dummy_client->Called());
  EXPECT_FALSE(raw->IsAlive());
}

// This client removes another client when notified.
class RemovingClient : public GarbageCollected<RemovingClient>,
                       public RawResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(RemovingClient);

 public:
  explicit RemovingClient(DummyClient* client) : dummy_client_(client) {}

  ~RemovingClient() override = default;

  // ResourceClient implementation.
  void NotifyFinished(Resource* resource) override {
    resource->RemoveClient(dummy_client_);
    resource->RemoveClient(this);
  }
  String DebugName() const override { return "RemovingClient"; }
  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(dummy_client_);
    RawResourceClient::Trace(visitor);
  }

 private:
  Member<DummyClient> dummy_client_;
};

TEST_F(RawResourceTest, RemoveClientDuringCallback) {
  Resource* raw = RawResource::CreateForTest(
      KURL("data:text/html,"), SecurityOrigin::CreateUniqueOpaque(),
      ResourceType::kRaw);
  raw->SetResponse(ResourceResponse(KURL("http://600.613/")));
  raw->FinishForTest();
  EXPECT_FALSE(raw->GetResponse().IsNull());

  Persistent<DummyClient> dummy_client = MakeGarbageCollected<DummyClient>();
  Persistent<RemovingClient> removing_client =
      MakeGarbageCollected<RemovingClient>(dummy_client.Get());
  raw->AddClient(dummy_client, platform_->test_task_runner().get());
  raw->AddClient(removing_client, platform_->test_task_runner().get());
  platform_->RunUntilIdle();
  EXPECT_FALSE(raw->IsAlive());
}

TEST_F(RawResourceTest, PreloadWithAsynchronousAddClient) {
  ResourceRequest request(KURL("data:text/html,"));
  request.SetRequestorOrigin(SecurityOrigin::CreateUniqueOpaque());
  request.SetUseStreamOnResponse(true);

  Resource* raw = RawResource::CreateForTest(request, ResourceType::kRaw);
  raw->MarkAsPreload();

  auto* bytes_consumer = MakeGarbageCollected<ReplayingBytesConsumer>(
      platform_->test_task_runner());
  bytes_consumer->Add(ReplayingBytesConsumer::Command(
      ReplayingBytesConsumer::Command::kData, "hello"));
  bytes_consumer->Add(
      ReplayingBytesConsumer::Command(ReplayingBytesConsumer::Command::kDone));
  ResponseBodyLoader* body_loader = MakeGarbageCollected<ResponseBodyLoader>(
      *bytes_consumer, *MakeGarbageCollected<NoopResponseBodyLoaderClient>(),
      platform_->test_task_runner().get());
  Persistent<DummyClient> dummy_client = MakeGarbageCollected<DummyClient>();

  // Set the response first to make ResourceClient addition asynchronous.
  raw->SetResponse(ResourceResponse(KURL("http://600.613/")));

  FetchParameters params(request);
  params.MutableResourceRequest().SetUseStreamOnResponse(false);
  raw->MatchPreload(params, platform_->test_task_runner().get());
  raw->AddClient(dummy_client, platform_->test_task_runner().get());

  raw->ResponseBodyReceived(*body_loader, platform_->test_task_runner());
  raw->FinishForTest();
  EXPECT_FALSE(dummy_client->Called());

  platform_->RunUntilIdle();

  EXPECT_TRUE(dummy_client->Called());
  EXPECT_EQ("hello",
            String(dummy_client->Data().data(), dummy_client->Data().size()));
}

}  // namespace blink
