// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/canvas_async_blob_creator.h"

#include <list>

#include "components/ukm/test_ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/graphics/color_correction_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

typedef CanvasAsyncBlobCreator::IdleTaskStatus IdleTaskStatus;

class MockCanvasAsyncBlobCreator : public CanvasAsyncBlobCreator {
 public:
  MockCanvasAsyncBlobCreator(scoped_refptr<StaticBitmapImage> image,
                             ImageEncodingMimeType mime_type,
                             Document* document,
                             bool fail_encoder_initialization = false)
      : CanvasAsyncBlobCreator(
            image,
            CanvasAsyncBlobCreator::GetImageEncodeOptionsForMimeType(mime_type),
            kHTMLCanvasToBlobCallback,
            nullptr,
            base::TimeTicks(),
            document->GetExecutionContext(),
            0,
            nullptr) {
    if (fail_encoder_initialization)
      fail_encoder_initialization_for_test_ = true;
    enforce_idle_encoding_for_test_ = true;
  }
  void Run() { loop_.Run(); }
  CanvasAsyncBlobCreator::IdleTaskStatus GetIdleTaskStatus() {
    return idle_task_status_;
  }

  MOCK_METHOD0(SignalTaskSwitchInStartTimeoutEventForTesting, void());
  MOCK_METHOD0(SignalTaskSwitchInCompleteTimeoutEventForTesting, void());

 private:
  base::RunLoop loop_;

 protected:
  void CreateBlobAndReturnResult(Vector<unsigned char> encoded_image) override {
  }
  void CreateNullAndReturnResult() override {}
  void SignalAlternativeCodePathFinishedForTesting() override;
  void PostDelayedTaskToCurrentThread(const base::Location&,
                                      base::OnceClosure,
                                      double delay_ms) override;
};

void MockCanvasAsyncBlobCreator::SignalAlternativeCodePathFinishedForTesting() {
  loop_.Quit();
}

void MockCanvasAsyncBlobCreator::PostDelayedTaskToCurrentThread(
    const base::Location& location,
    base::OnceClosure task,
    double delay_ms) {
  // override delay to 0.
  CanvasAsyncBlobCreator::PostDelayedTaskToCurrentThread(location,
                                                         std::move(task),
                                                         /*delay_ms=*/0);
}

//==============================================================================

class MockCanvasAsyncBlobCreatorWithoutStart
    : public MockCanvasAsyncBlobCreator {
 public:
  MockCanvasAsyncBlobCreatorWithoutStart(scoped_refptr<StaticBitmapImage> image,
                                         Document* document)
      : MockCanvasAsyncBlobCreator(image, kMimeTypePng, document) {}

 protected:
  void ScheduleInitiateEncoding(double) override {
    // Deliberately make scheduleInitiateEncoding do nothing so that idle
    // task never starts
  }
};

//==============================================================================

class MockCanvasAsyncBlobCreatorWithoutComplete
    : public MockCanvasAsyncBlobCreator {
 public:
  MockCanvasAsyncBlobCreatorWithoutComplete(
      scoped_refptr<StaticBitmapImage> image,
      Document* document,
      bool fail_encoder_initialization = false)
      : MockCanvasAsyncBlobCreator(image,
                                   kMimeTypePng,
                                   document,
                                   fail_encoder_initialization) {}

 protected:
  void ScheduleInitiateEncoding(double quality) override {
    PostDelayedTaskToCurrentThread(
        FROM_HERE,
        WTF::BindOnce(
            &MockCanvasAsyncBlobCreatorWithoutComplete::InitiateEncoding,
            WrapPersistent(this), quality, base::TimeTicks::Max()),
        /*delay_ms=*/0);
  }

  void IdleEncodeRows(base::TimeTicks deadline) override {
    // Deliberately make idleEncodeRows do nothing so that idle task never
    // completes
  }
};

//==============================================================================

class CanvasAsyncBlobCreatorTest : public PageTestBase {
 public:
  void PrepareMockCanvasAsyncBlobCreatorWithoutStart();
  void PrepareMockCanvasAsyncBlobCreatorWithoutComplete();
  void PrepareMockCanvasAsyncBlobCreatorFail();

 protected:
  CanvasAsyncBlobCreatorTest();
  MockCanvasAsyncBlobCreator* AsyncBlobCreator() {
    return async_blob_creator_.Get();
  }
  ukm::UkmRecorder* UkmRecorder() { return &ukm_recorder_; }
  void TearDown() override;

 private:
  Persistent<MockCanvasAsyncBlobCreator> async_blob_creator_;
  ukm::TestUkmRecorder ukm_recorder_;
};

CanvasAsyncBlobCreatorTest::CanvasAsyncBlobCreatorTest() = default;

scoped_refptr<StaticBitmapImage> CreateTransparentImage(int width, int height) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
  if (!surface)
    return nullptr;
  return UnacceleratedStaticBitmapImage::Create(surface->makeImageSnapshot());
}

void CanvasAsyncBlobCreatorTest::
    PrepareMockCanvasAsyncBlobCreatorWithoutStart() {
  async_blob_creator_ =
      MakeGarbageCollected<MockCanvasAsyncBlobCreatorWithoutStart>(
          CreateTransparentImage(20, 20), &GetDocument());
}

void CanvasAsyncBlobCreatorTest::
    PrepareMockCanvasAsyncBlobCreatorWithoutComplete() {
  async_blob_creator_ =
      MakeGarbageCollected<MockCanvasAsyncBlobCreatorWithoutComplete>(
          CreateTransparentImage(20, 20), &GetDocument());
}

void CanvasAsyncBlobCreatorTest::PrepareMockCanvasAsyncBlobCreatorFail() {
  // We reuse the class MockCanvasAsyncBlobCreatorWithoutComplete because
  // this test case is expected to fail at initialization step before
  // completion.
  async_blob_creator_ =
      MakeGarbageCollected<MockCanvasAsyncBlobCreatorWithoutComplete>(
          CreateTransparentImage(20, 20), &GetDocument(), true);
}

void CanvasAsyncBlobCreatorTest::TearDown() {
  async_blob_creator_ = nullptr;
}

//==============================================================================

TEST_F(CanvasAsyncBlobCreatorTest,
       IdleTaskNotStartedWhenStartTimeoutEventHappens) {
  // This test mocks the scenario when idle task is not started when the
  // StartTimeoutEvent is inspecting the idle task status.
  // The whole image encoding process (including initialization)  will then
  // become carried out in the alternative code path instead.
  PrepareMockCanvasAsyncBlobCreatorWithoutStart();
  EXPECT_CALL(*(AsyncBlobCreator()),
              SignalTaskSwitchInStartTimeoutEventForTesting());

  AsyncBlobCreator()->ScheduleAsyncBlobCreation(1.0);
  AsyncBlobCreator()->Run();

  testing::Mock::VerifyAndClearExpectations(AsyncBlobCreator());
  EXPECT_EQ(IdleTaskStatus::kIdleTaskSwitchedToImmediateTask,
            AsyncBlobCreator()->GetIdleTaskStatus());
}

TEST_F(CanvasAsyncBlobCreatorTest,
       IdleTaskNotCompletedWhenCompleteTimeoutEventHappens) {
  // This test mocks the scenario when idle task is not completed when the
  // CompleteTimeoutEvent is inspecting the idle task status.
  // The remaining image encoding process (excluding initialization)  will
  // then become carried out in the alternative code path instead.
  PrepareMockCanvasAsyncBlobCreatorWithoutComplete();
  EXPECT_CALL(*(AsyncBlobCreator()),
              SignalTaskSwitchInCompleteTimeoutEventForTesting());
  AsyncBlobCreator()->ScheduleAsyncBlobCreation(1.0);
  AsyncBlobCreator()->Run();

  testing::Mock::VerifyAndClearExpectations(AsyncBlobCreator());
  EXPECT_EQ(IdleTaskStatus::kIdleTaskSwitchedToImmediateTask,
            AsyncBlobCreator()->GetIdleTaskStatus());
}

TEST_F(CanvasAsyncBlobCreatorTest, IdleTaskFailedWhenStartTimeoutEventHappens) {
  // This test mocks the scenario when idle task is not failed during when
  // either the StartTimeoutEvent or the CompleteTimeoutEvent is inspecting
  // the idle task status.
  PrepareMockCanvasAsyncBlobCreatorFail();
  AsyncBlobCreator()->ScheduleAsyncBlobCreation(1.0);
  AsyncBlobCreator()->Run();

  EXPECT_EQ(IdleTaskStatus::kIdleTaskFailed,
            AsyncBlobCreator()->GetIdleTaskStatus());
}

}  // namespace blink
