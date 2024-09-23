// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/video_rvfc/video_frame_request_callback_collection.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

using testing::_;

namespace blink {

constexpr double kDefaultTimestamp = 12345.0;

class MockVideoFrameCallback
    : public VideoFrameRequestCallbackCollection::VideoFrameCallback {
 public:
  MOCK_METHOD2(Invoke, void(double, const VideoFrameCallbackMetadata*));
};

class VideoFrameRequestCallbackCollectionTest : public PageTestBase {
 public:
  using CallbackId = int;

  VideoFrameRequestCallbackCollectionTest()
      : execution_context_(MakeGarbageCollected<NullExecutionContext>()),
        collection_(MakeGarbageCollected<VideoFrameRequestCallbackCollection>(
            execution_context_.Get())) {}
  ~VideoFrameRequestCallbackCollectionTest() override {
    execution_context_->NotifyContextDestroyed();
  }

  VideoFrameRequestCallbackCollection* collection() {
    return collection_.Get();
  }

  Persistent<MockVideoFrameCallback> CreateCallback() {
    return MakeGarbageCollected<MockVideoFrameCallback>();
  }

 private:
  Persistent<ExecutionContext> execution_context_;
  Persistent<VideoFrameRequestCallbackCollection> collection_;
};

TEST_F(VideoFrameRequestCallbackCollectionTest, AddSingleCallback) {
  EXPECT_TRUE(collection()->IsEmpty());

  auto callback = CreateCallback();
  CallbackId id = collection()->RegisterFrameCallback(callback.Get());

  EXPECT_EQ(id, callback->Id());
  EXPECT_FALSE(collection()->IsEmpty());
}

TEST_F(VideoFrameRequestCallbackCollectionTest, InvokeSingleCallback) {
  auto* metadata = VideoFrameCallbackMetadata::Create();
  auto callback = CreateCallback();
  collection()->RegisterFrameCallback(callback.Get());

  EXPECT_CALL(*callback, Invoke(kDefaultTimestamp, metadata));
  collection()->ExecuteFrameCallbacks(kDefaultTimestamp, metadata);

  EXPECT_TRUE(collection()->IsEmpty());
}

TEST_F(VideoFrameRequestCallbackCollectionTest, CancelSingleCallback) {
  auto callback = CreateCallback();
  CallbackId id = collection()->RegisterFrameCallback(callback.Get());
  EXPECT_FALSE(callback->IsCancelled());
  // The callback should not be invoked.
  EXPECT_CALL(*callback, Invoke(_, _)).Times(0);

  // Cancelling an non existent ID should do nothing.
  collection()->CancelFrameCallback(id + 100);
  EXPECT_FALSE(collection()->IsEmpty());
  EXPECT_FALSE(callback->IsCancelled());

  // Cancel the callback this time.
  collection()->CancelFrameCallback(id);
  EXPECT_TRUE(collection()->IsEmpty());

  collection()->ExecuteFrameCallbacks(kDefaultTimestamp,
                                      VideoFrameCallbackMetadata::Create());
  EXPECT_TRUE(collection()->IsEmpty());
}

TEST_F(VideoFrameRequestCallbackCollectionTest, ExecuteMultipleCallbacks) {
  auto callback_1 = CreateCallback();
  collection()->RegisterFrameCallback(callback_1.Get());

  auto callback_2 = CreateCallback();
  collection()->RegisterFrameCallback(callback_2.Get());

  EXPECT_CALL(*callback_1, Invoke(_, _));
  EXPECT_CALL(*callback_2, Invoke(_, _));
  collection()->ExecuteFrameCallbacks(kDefaultTimestamp,
                                      VideoFrameCallbackMetadata::Create());

  // All callbacks should have been executed and removed.
  EXPECT_TRUE(collection()->IsEmpty());
}

TEST_F(VideoFrameRequestCallbackCollectionTest, CreateCallbackDuringExecution) {
  Persistent<MockVideoFrameCallback> created_callback;
  CallbackId created_id = 0;

  auto callback = CreateCallback();
  EXPECT_CALL(*callback, Invoke(_, _))
      .WillOnce(testing::WithoutArgs(testing::Invoke([&]() {
        created_callback = CreateCallback();
        created_id =
            collection()->RegisterFrameCallback(created_callback.Get());
        EXPECT_CALL(*created_callback, Invoke(_, _)).Times(0);
      })));

  collection()->RegisterFrameCallback(callback.Get());
  collection()->ExecuteFrameCallbacks(kDefaultTimestamp,
                                      VideoFrameCallbackMetadata::Create());

  EXPECT_NE(created_id, 0);
  EXPECT_FALSE(collection()->IsEmpty());

  // The created callback should be executed the second time around.
  EXPECT_CALL(*created_callback, Invoke(_, _)).Times(1);
  collection()->ExecuteFrameCallbacks(kDefaultTimestamp,
                                      VideoFrameCallbackMetadata::Create());
  EXPECT_TRUE(collection()->IsEmpty());
}

TEST_F(VideoFrameRequestCallbackCollectionTest, CancelCallbackDuringExecution) {
  auto dummy_callback = CreateCallback();
  CallbackId dummy_callback_id =
      collection()->RegisterFrameCallback(dummy_callback.Get());

  // This is a hacky way of simulating a callback being cancelled mid-execution.
  // We guess the ID of the 3rd callback, since (as an implementation detail)
  // CallbackIds are distributed sequentially.
  int expected_target_id = dummy_callback_id + 2;

  auto cancelling_callback = CreateCallback();
  EXPECT_CALL(*cancelling_callback, Invoke(_, _))
      .WillOnce(testing::WithoutArgs(testing::Invoke(
          [&]() { collection()->CancelFrameCallback(expected_target_id); })));
  collection()->RegisterFrameCallback(cancelling_callback.Get());

  auto target_callback = CreateCallback();
  CallbackId target_callback_id =
      collection()->RegisterFrameCallback(target_callback.Get());

  EXPECT_CALL(*target_callback, Invoke(_, _)).Times(0);
  EXPECT_EQ(expected_target_id, target_callback_id);

  collection()->ExecuteFrameCallbacks(kDefaultTimestamp,
                                      VideoFrameCallbackMetadata::Create());

  // Everything should have been cleared
  EXPECT_TRUE(collection()->IsEmpty());
}

}  // namespace blink
