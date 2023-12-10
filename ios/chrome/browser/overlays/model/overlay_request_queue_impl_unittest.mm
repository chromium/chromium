// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/overlay_request_queue_impl.h"

#import <vector>

#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_cancel_handler.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_request_cancel_handler.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_user_data.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

namespace {
// Fake queue delegate.  Keeps ownership of all requests removed from a queue,
// recording whether the requests were removed for cancellation.
class FakeOverlayRequestQueueImplDelegate
    : public OverlayRequestQueueImpl::Delegate {
 public:
  FakeOverlayRequestQueueImplDelegate() {}
  ~FakeOverlayRequestQueueImplDelegate() {}

  void OverlayRequestRemoved(OverlayRequestQueueImpl* queue,
                             std::unique_ptr<OverlayRequest> request,
                             bool cancelled) override {
    removed_requests_.emplace_back(std::move(request), cancelled);
  }

  void OverlayRequestQueueWillReplaceDelegate(
      OverlayRequestQueueImpl* queue) override {}

  // Whether `request` was removed from the queue.
  bool WasRequestRemoved(OverlayRequest* request) {
    return GetRemovedRequestStorage(request) != nullptr;
  }

  // Whether `request` was removed from the queue for cancellation.
  bool WasRequestCancelled(OverlayRequest* request) {
    const RemovedRequestStorage* storage = GetRemovedRequestStorage(request);
    return storage && storage->cancelled;
  }

 private:
  // Stores the removed requests and whether the requests were cancelled.
  struct RemovedRequestStorage {
    RemovedRequestStorage(std::unique_ptr<OverlayRequest> request,
                          bool cancelled)
        : request(std::move(request)), cancelled(cancelled) {}
    RemovedRequestStorage(RemovedRequestStorage&& storage)
        : request(std::move(storage.request)), cancelled(storage.cancelled) {}
    ~RemovedRequestStorage() {}

    std::unique_ptr<OverlayRequest> request;
    bool cancelled;
  };

  // Returns the request storage for `request`.
  const RemovedRequestStorage* GetRemovedRequestStorage(
      OverlayRequest* request) {
    for (auto& storage : removed_requests_) {
      if (storage.request.get() == request)
        return &storage;
    }
    return nullptr;
  }

  // Storages holding the requests that were removed from the queue being
  // delegated.  Keeps ownership of removed requests and whether they were
  // cancelled.
  std::vector<RemovedRequestStorage> removed_requests_;
};
// Mock queue observer.
class MockOverlayRequestQueueImplObserver
    : public OverlayRequestQueueImpl::Observer {
 public:
  MockOverlayRequestQueueImplObserver() {}
  ~MockOverlayRequestQueueImplObserver() override {}

  MOCK_METHOD3(RequestAddedToQueue,
               void(OverlayRequestQueueImpl*, OverlayRequest*, size_t));

  void OverlayRequestQueueDestroyed(OverlayRequestQueueImpl* queue) override {
    queue->RemoveObserver(this);
  }
};
// A cancel handler that never cancels its request.
class NoOpCancelHandler : public OverlayRequestCancelHandler {
 public:
  NoOpCancelHandler(OverlayRequest* request, OverlayRequestQueue* queue)
      : OverlayRequestCancelHandler(request, queue) {}
  ~NoOpCancelHandler() override = default;
};
}  // namespace

// Test fixture for RequestQueueImpl.
class OverlayRequestQueueImplTest : public PlatformTest {
 public:
  OverlayRequestQueueImplTest()
      : PlatformTest(), web_state_(std::make_unique<web::FakeWebState>()) {
    OverlayRequestQueueImpl::Container::CreateForWebState(web_state_.get());
    queue()->SetDelegate(&delegate_);
    queue()->AddObserver(&observer_);
  }
  ~OverlayRequestQueueImplTest() override {
    if (web_state_) {
      queue()->SetDelegate(nullptr);
      queue()->RemoveObserver(&observer_);
    }
  }

  OverlayRequestQueueImpl* queue() {
    // Use the kWebContentArea queue for testing.
    return OverlayRequestQueueImpl::Container::FromWebState(web_state_.get())
        ->QueueForModality(OverlayModality::kWebContentArea);
  }
  MockOverlayRequestQueueImplObserver& observer() { return observer_; }

  OverlayRequest* AddRequest() {
    std::unique_ptr<OverlayRequest> passed_request =
        OverlayRequest::CreateWithConfig<FakeOverlayUserData>();
    OverlayRequest* request = passed_request.get();
    EXPECT_CALL(observer(),
                RequestAddedToQueue(queue(), request, queue()->size()));
    queue()->AddRequest(std::move(passed_request));
    return request;
  }

 protected:
  FakeOverlayRequestQueueImplDelegate delegate_;
  MockOverlayRequestQueueImplObserver observer_;
  std::unique_ptr<web::FakeWebState> web_state_;
};

// Tests that state is updated correctly and observer callbacks are received
// when adding requests to the back of the queue.
TEST_F(OverlayRequestQueueImplTest, AddRequest) {
  OverlayRequest* first_request = AddRequest();
  AddRequest();

  EXPECT_EQ(first_request, queue()->front_request());
  EXPECT_EQ(2U, queue()->size());
}

// Tests that GetRequest() returns the expected values.
TEST_F(OverlayRequestQueueImplTest, GetRequest) {
  OverlayRequest* first_request = AddRequest();
  OverlayRequest* second_request = AddRequest();
  OverlayRequest* third_request = AddRequest();

  // Verify GetRequest() results.
  EXPECT_EQ(first_request, queue()->GetRequest(/*index=*/0));
  EXPECT_EQ(second_request, queue()->GetRequest(/*index=*/1));
  EXPECT_EQ(third_request, queue()->GetRequest(/*index=*/2));

  // Verify array-syntax accessor results.
  EXPECT_EQ(first_request, (*queue())[0]);
  EXPECT_EQ(second_request, (*queue())[1]);
  EXPECT_EQ(third_request, (*queue())[2]);
}

// Tests that state is updated correctly and observer callbacks are received
// when inserting requests into the middle of the queue.
TEST_F(OverlayRequestQueueImplTest, InsertRequest) {
  AddRequest();
  AddRequest();
  ASSERT_EQ(2U, queue()->size());

  // Insert a request into the middle of the queue.
  void* kInsertedRequestConfigValue = &kInsertedRequestConfigValue;
  std::unique_ptr<OverlayRequest> inserted_request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>(
          kInsertedRequestConfigValue);
  OverlayRequest* request = inserted_request.get();
  EXPECT_CALL(observer(), RequestAddedToQueue(queue(), request, 1));
  queue()->InsertRequest(1, std::move(inserted_request));

  // Verify that the request is inserted correctly.
  EXPECT_EQ(3U, queue()->size());
  EXPECT_EQ(kInsertedRequestConfigValue,
            queue()->GetRequest(1)->GetConfig<FakeOverlayUserData>()->value());
}

// Tests that PopFrontRequest() correctly updates state, notifies observers, and
// transfers ownership of the popped request to the delegate.
TEST_F(OverlayRequestQueueImplTest, PopFrontRequest) {
  // Add two requests to the queue.
  OverlayRequest* first_request = AddRequest();
  OverlayRequest* second_request = AddRequest();
  ASSERT_EQ(first_request, queue()->front_request());
  ASSERT_EQ(2U, queue()->size());

  // Pop the first request and check that the size and front request have been
  // updated.
  queue()->PopFrontRequest();
  EXPECT_EQ(second_request, queue()->front_request());
  EXPECT_EQ(1U, queue()->size());
  EXPECT_TRUE(delegate_.WasRequestRemoved(first_request));
  EXPECT_FALSE(delegate_.WasRequestCancelled(first_request));
}

// Tests that CancelAllRequests() correctly updates state and transfers requests
// to the delegate.
TEST_F(OverlayRequestQueueImplTest, CancelAllRequests) {
  // Add two requests to the queue then cancel all requests, verifying that
  // the observer callback is received for each.
  OverlayRequest* first_request = AddRequest();
  OverlayRequest* second_request = AddRequest();
  queue()->CancelAllRequests();

  EXPECT_EQ(0U, queue()->size());
  EXPECT_TRUE(delegate_.WasRequestCancelled(first_request));
  EXPECT_TRUE(delegate_.WasRequestCancelled(second_request));
}

// Tests that a cancellation via a cancel handler correctly updates state and
// transfers the caoncelled requests to the delegate.
TEST_F(OverlayRequestQueueImplTest, CustomCancelHandler) {
  std::unique_ptr<OverlayRequest> passed_request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>();
  OverlayRequest* request = passed_request.get();
  std::unique_ptr<FakeOverlayRequestCancelHandler> passed_cancel_handler =
      std::make_unique<FakeOverlayRequestCancelHandler>(request, queue());
  FakeOverlayRequestCancelHandler* cancel_handler = passed_cancel_handler.get();
  EXPECT_CALL(observer(),
              RequestAddedToQueue(queue(), request, queue()->size()));
  queue()->AddRequest(std::move(passed_request),
                      std::move(passed_cancel_handler));

  // Trigger cancellation via the cancel handler, and verify that the request is
  // correctly removed.
  cancel_handler->TriggerCancellation();

  EXPECT_EQ(0U, queue()->size());
  EXPECT_TRUE(delegate_.WasRequestCancelled(request));
}

// Tests that the request's WebState is set up when it is added to the queue.
TEST_F(OverlayRequestQueueImplTest, WebStateSetup) {
  std::unique_ptr<OverlayRequest> added_request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>();
  OverlayRequest* request = added_request.get();
  ASSERT_FALSE(request->GetQueueWebState());

  // Add the request and verify that the WebState is set.
  EXPECT_CALL(observer(),
              RequestAddedToQueue(queue(), request, queue()->size()));
  queue()->AddRequest(std::move(added_request));
  EXPECT_EQ(web_state_.get(), request->GetQueueWebState());
}

// Tests that requests are cancelled upon queue destruction even if their cancel
// handlers do not explicitly handle cancellation on WebState destruction.
TEST_F(OverlayRequestQueueImplTest, CancellationUponDestruction) {
  std::unique_ptr<OverlayRequest> passed_request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>();
  OverlayRequest* request = passed_request.get();
  std::unique_ptr<OverlayRequestCancelHandler> cancel_handler =
      std::make_unique<NoOpCancelHandler>(passed_request.get(), queue());
  EXPECT_CALL(observer(),
              RequestAddedToQueue(queue(), request, queue()->size()));
  queue()->AddRequest(std::move(passed_request), std::move(cancel_handler));

  // Destroy the OverlayRequestQueue by destroying its owning WebState.
  web_state_ = nullptr;

  // Verify that the request was cancelled even though the cancel handler never
  // executed CancelRequest().
  EXPECT_TRUE(delegate_.WasRequestCancelled(request));
}
