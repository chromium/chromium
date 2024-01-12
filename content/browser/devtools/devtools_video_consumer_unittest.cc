// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/region_capture_bounds.h"
#include "content/browser/devtools/devtools_video_consumer.h"
#include "content/public/test/test_utils.h"
#include "media/base/limits.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace content {
namespace {

// Capture parameters.
constexpr gfx::Size kResolution = gfx::Size(320, 180);  // Arbitrarily chosen.
constexpr media::VideoPixelFormat kFormat = media::PIXEL_FORMAT_I420;

// Video buffer parameters.
constexpr bool kNotPremapped = false;

// A non-zero FrameSinkId to prevent validation errors when
// DevToolsVideoConsumer::ChangeTarget(viz::FrameSinkId) is called
// (which eventually fails in FrameSinkVideoCapturerStubDispatch::Accept).
constexpr viz::FrameSinkId kInitialFrameSinkId = viz::FrameSinkId(1, 1);

}  // namespace

// Mock for the FrameSinkVideoCapturer running in the VIZ process.
class MockFrameSinkVideoCapturer : public viz::mojom::FrameSinkVideoCapturer {
 public:
  MockFrameSinkVideoCapturer() {}

  bool is_bound() const { return receiver_.is_bound(); }

  void Bind(
      mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer> receiver) {
    DCHECK(!receiver_.is_bound());
    receiver_.Bind(std::move(receiver));
  }

  void Reset() {
    receiver_.reset();
    consumer_.reset();
  }

  // This is never called.
  MOCK_METHOD1(SetFormat, void(media::VideoPixelFormat format));
  void SetMinCapturePeriod(base::TimeDelta min_capture_period) final {
    min_capture_period_ = min_capture_period;
    MockSetMinCapturePeriod(min_capture_period_);
  }
  MOCK_METHOD1(MockSetMinCapturePeriod,
               void(base::TimeDelta min_capture_period));
  void SetMinSizeChangePeriod(base::TimeDelta min_period) final {
    min_period_ = min_period;
    MockSetMinSizeChangePeriod(min_period_);
  }
  MOCK_METHOD1(MockSetMinSizeChangePeriod, void(base::TimeDelta min_period));
  void SetResolutionConstraints(const gfx::Size& min_frame_size,
                                const gfx::Size& max_frame_size,
                                bool use_fixed_aspect_ratio) final {
    min_frame_size_ = min_frame_size;
    max_frame_size_ = max_frame_size;
    MockSetResolutionConstraints(min_frame_size_, max_frame_size_, true);
  }
  MOCK_METHOD3(MockSetResolutionConstraints,
               void(const gfx::Size& min_frame_size,
                    const gfx::Size& max_frame_size,
                    bool use_fixed_aspect_ratio));
  // This is never called.
  MOCK_METHOD1(SetAutoThrottlingEnabled, void(bool));
  void ChangeTarget(const std::optional<viz::VideoCaptureTarget>& target,
                    uint32_t sub_capture_target_version) final {
    frame_sink_id_ = target ? target->frame_sink_id : viz::FrameSinkId();
    MockChangeTarget(frame_sink_id_, sub_capture_target_version);
  }
  MOCK_METHOD2(MockChangeTarget,
               void(const viz::FrameSinkId& frame_sink_id,
                    uint32_t sub_capture_target_version));
  void Start(
      mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumer> consumer,
      viz::mojom::BufferFormatPreference buffer_format_preference) final {
    DCHECK(!consumer_);
    consumer_.Bind(std::move(consumer));
    MockStart(consumer_.get(), buffer_format_preference);
  }
  MOCK_METHOD2(
      MockStart,
      void(viz::mojom::FrameSinkVideoConsumer* consumer,
           viz::mojom::BufferFormatPreference buffer_format_preference));
  void Stop() final {
    receiver_.reset();
    consumer_.reset();
    MockStop();
  }
  MOCK_METHOD0(MockStop, void());
  MOCK_METHOD0(RequestRefreshFrame, void());
  MOCK_METHOD2(
      CreateOverlay,
      void(int32_t stacking_index,
           mojo::PendingReceiver<viz::mojom::FrameSinkVideoCaptureOverlay>
               receiver));

  // Const accessors to get the cached variables.
  base::TimeDelta min_capture_period() const { return min_capture_period_; }
  base::TimeDelta min_period() const { return min_period_; }
  gfx::Size min_frame_size() const { return min_frame_size_; }
  gfx::Size max_frame_size() const { return max_frame_size_; }
  viz::FrameSinkId frame_sink_id() const { return frame_sink_id_; }

 private:
  // These variables are cached when they are received from
  // DevToolsVideoConsumer.
  base::TimeDelta min_capture_period_;
  base::TimeDelta min_period_;
  gfx::Size min_frame_size_;
  gfx::Size max_frame_size_;
  viz::FrameSinkId frame_sink_id_;
  mojo::Remote<viz::mojom::FrameSinkVideoConsumer> consumer_;

  mojo::Receiver<viz::mojom::FrameSinkVideoCapturer> receiver_{this};
};

// Represents the FrameSinkVideoConsumerFrameCallbacks instance in the VIZ
// process.
class MockFrameSinkVideoConsumerFrameCallbacks
    : public viz::mojom::FrameSinkVideoConsumerFrameCallbacks {
 public:
  MockFrameSinkVideoConsumerFrameCallbacks() = default;

  void Bind(
      mojo::PendingReceiver<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
          receiver) {
    receiver_.Bind(std::move(receiver));
  }

  MOCK_METHOD0(Done, void());
  MOCK_METHOD1(ProvideFeedback, void(const media::VideoCaptureFeedback&));

 private:
  mojo::Receiver<viz::mojom::FrameSinkVideoConsumerFrameCallbacks> receiver_{
      this};
};

// Mock for the classes like TracingHandler that receive frames from
// DevToolsVideoConsumer via the OnFrameCapturedCallback.
class MockDevToolsVideoFrameReceiver {
 public:
  MOCK_METHOD1(OnFrameFromVideoConsumerMock,
               void(scoped_refptr<media::VideoFrame> frame));

  MockDevToolsVideoFrameReceiver() {}

  scoped_refptr<media::VideoFrame> TakeFrameAt(int i) {
    return std::move(frames_[i]);
  }

  void OnFrameFromVideoConsumer(scoped_refptr<media::VideoFrame> frame) {
    OnFrameFromVideoConsumerMock(frame);
    frames_.push_back(std::move(frame));
  }

  std::unique_ptr<DevToolsVideoConsumer> CreateDevToolsVideoConsumer() {
    return std::make_unique<DevToolsVideoConsumer>(base::BindRepeating(
        &MockDevToolsVideoFrameReceiver::OnFrameFromVideoConsumer,
        weak_factory_.GetWeakPtr()));
  }

 private:
  std::vector<scoped_refptr<media::VideoFrame>> frames_;
  base::WeakPtrFactory<MockDevToolsVideoFrameReceiver> weak_factory_{this};
};

class DevToolsVideoConsumerTest : public testing::Test {
 public:
  DevToolsVideoConsumerTest() {}

  void SetUp() override {
    consumer_ = receiver_.CreateDevToolsVideoConsumer();

    consumer_->SetFrameSinkId(kInitialFrameSinkId);
  }

  void SimulateFrameCapture(base::ReadOnlySharedMemoryRegion data) {
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks_remote;
    callbacks.Bind(callbacks_remote.InitWithNewPipeAndPassReceiver());

    media::mojom::VideoFrameInfoPtr info = media::mojom::VideoFrameInfo::New(
        base::TimeDelta(), media::VideoFrameMetadata(), kFormat, kResolution,
        gfx::Rect(kResolution), kNotPremapped, gfx::ColorSpace::CreateREC709(),
        nullptr);

    consumer_->OnFrameCaptured(
        media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
            std::move(data)),
        std::move(info), gfx::Rect(kResolution), std::move(callbacks_remote));
  }

  void StartCaptureWithMockCapturer() {
    consumer_->InnerStartCapture(CreateMockCapturer());
  }

  bool IsValidMinAndMaxFrameSize(gfx::Size min_frame_size,
                                 gfx::Size max_frame_size) {
    return consumer_->IsValidMinAndMaxFrameSize(min_frame_size, max_frame_size);
  }

  static gfx::Size GetVideoConsumerDefaultMinFrameSize() {
    return DevToolsVideoConsumer::kDefaultMinFrameSize;
  }

  static gfx::Size GetVideoConsumerDefaultMaxFrameSize() {
    return DevToolsVideoConsumer::kDefaultMaxFrameSize;
  }

  // Getters for |consumer_|'s private variables.
  base::TimeDelta GetMinCapturePeriod() const {
    return consumer_->min_capture_period_;
  }
  gfx::Size GetMinFrameSize() const { return consumer_->min_frame_size_; }
  gfx::Size GetMaxFrameSize() const { return consumer_->max_frame_size_; }
  viz::FrameSinkId GetFrameSinkId() const { return consumer_->frame_sink_id_; }

 protected:
  MockFrameSinkVideoCapturer capturer_;
  MockFrameSinkVideoConsumerFrameCallbacks callbacks;
  MockDevToolsVideoFrameReceiver receiver_;
  std::unique_ptr<DevToolsVideoConsumer> consumer_;

 private:
  std::unique_ptr<viz::ClientFrameSinkVideoCapturer> CreateMockCapturer() {
    return std::make_unique<viz::ClientFrameSinkVideoCapturer>(
        base::BindRepeating(
            [](base::WeakPtr<DevToolsVideoConsumerTest> self,
               mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer>
                   receiver) { self->capturer_.Bind(std::move(receiver)); },
            weak_factory_.GetWeakPtr()));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::WeakPtrFactory<DevToolsVideoConsumerTest> weak_factory_{this};
};

// Tests that the OnFrameFromVideoConsumer callbacks is called when
// OnFrameCaptured is passed a valid buffer with valid mapping.
TEST_F(DevToolsVideoConsumerTest, CallbacksAreCalledWhenBufferValid) {
  // On valid buffer the |receiver_| gets a frame via OnFrameFromVideoConsumer.
  EXPECT_CALL(receiver_, OnFrameFromVideoConsumerMock(_)).Times(1);

  auto region = base::ReadOnlySharedMemoryRegion::Create(
                    media::VideoFrame::AllocationSize(kFormat, kResolution))
                    .region;
  ASSERT_TRUE(region.IsValid());
  SimulateFrameCapture(std::move(region));
  base::RunLoop().RunUntilIdle();
}

// Tests that the OnFrameFromVideoConsumer callback is not called when
// OnFrameCaptured is passed a buffer with less-than-expected size.
TEST_F(DevToolsVideoConsumerTest, CallbackIsNotCalledWhenBufferIsTooSmall) {
  // On invalid mapping, the |receiver_| doesn't get a frame.
  EXPECT_CALL(receiver_, OnFrameFromVideoConsumerMock(_)).Times(0);

  constexpr size_t too_few_number_of_bytes = 4;
  ASSERT_LT(too_few_number_of_bytes,
            media::VideoFrame::AllocationSize(kFormat, kResolution));
  auto region =
      base::ReadOnlySharedMemoryRegion::Create(too_few_number_of_bytes).region;
  ASSERT_TRUE(region.IsValid());
  SimulateFrameCapture(std::move(region));
  base::RunLoop().RunUntilIdle();
}

// Tests that starting capture calls |capturer_| functions, and capture can be
// restarted. This test is important as it ensures that when restarting capture,
// a FrameSinkVideoCapturerPtrInfo is bound to |capturer_| and it verifies that
// resources used in the previous StartCapture aren't reused.
TEST_F(DevToolsVideoConsumerTest, StartCaptureCallsSetFunctions) {
  // Starting capture should call these |capturer_| functions once.
  EXPECT_CALL(capturer_, MockSetMinCapturePeriod(_));
  EXPECT_CALL(capturer_, MockSetMinSizeChangePeriod(_));
  EXPECT_CALL(capturer_, MockSetResolutionConstraints(_, _, _));
  EXPECT_CALL(capturer_, MockChangeTarget(_, _));
  EXPECT_CALL(capturer_,
              MockStart(_, viz::mojom::BufferFormatPreference::kDefault));
  StartCaptureWithMockCapturer();
  base::RunLoop().RunUntilIdle();

  // Stop capturing.
  consumer_->StopCapture();

  // Reset the mock to allow the next consumer to connect.
  capturer_.Reset();

  // Start capturing again, and expect that these |capturer_| functions are
  // called once. This will re-bind the |capturer_| and ensures that destroyed
  // resources aren't being reused.
  EXPECT_CALL(capturer_, MockSetMinCapturePeriod(_));
  EXPECT_CALL(capturer_, MockSetMinSizeChangePeriod(_));
  EXPECT_CALL(capturer_, MockSetResolutionConstraints(_, _, _));
  EXPECT_CALL(capturer_, MockChangeTarget(_, _));
  EXPECT_CALL(capturer_,
              MockStart(_, viz::mojom::BufferFormatPreference::kDefault));
  StartCaptureWithMockCapturer();
  base::RunLoop().RunUntilIdle();
}

// Tests that calling 'Set' functions in DevToolsVideoConsumer before
// |capturer_| is initialized results in the passed values being cached.
// When capture is later started (and |capturer_| initialized), these cached
// values should be used and sent to the |capturer_|.
TEST_F(DevToolsVideoConsumerTest, CapturerIsPassedCachedValues) {
  // These values are chosen so that they are valid, and different from
  // the default values in DevToolsVideoConsumer.
  constexpr base::TimeDelta kNewMinCapturePeriod = base::TimeDelta();
  const gfx::Size kNewMinFrameSize =
      gfx::Size(GetVideoConsumerDefaultMinFrameSize().width() + 1,
                GetVideoConsumerDefaultMinFrameSize().height() + 1);
  const gfx::Size kNewMaxFrameSize =
      gfx::Size(GetVideoConsumerDefaultMaxFrameSize().width() + 1,
                GetVideoConsumerDefaultMaxFrameSize().width() + 1);
  constexpr viz::FrameSinkId kNewFrameSinkId = viz::FrameSinkId(2, 2);

  // Right now, |capturer_| has not been created via StartCapture, so
  // calling these functions should not call the |capturer_|, but the
  // values that are passed in should be cached.
  EXPECT_CALL(capturer_, MockSetMinCapturePeriod(_)).Times(0);
  EXPECT_CALL(capturer_, MockSetMinSizeChangePeriod(_)).Times(0);
  EXPECT_CALL(capturer_, MockSetResolutionConstraints(_, _, _)).Times(0);
  EXPECT_CALL(capturer_, MockChangeTarget(_, _)).Times(0);
  EXPECT_CALL(capturer_,
              MockStart(_, viz::mojom::BufferFormatPreference::kDefault))
      .Times(0);
  consumer_->SetMinCapturePeriod(kNewMinCapturePeriod);
  consumer_->SetMinAndMaxFrameSize(kNewMinFrameSize, kNewMaxFrameSize);
  consumer_->SetFrameSinkId(kNewFrameSinkId);
  base::RunLoop().RunUntilIdle();
  // Verify that new values are cached.
  EXPECT_EQ(GetMinCapturePeriod(), kNewMinCapturePeriod);
  EXPECT_EQ(GetMinFrameSize(), kNewMinFrameSize);
  EXPECT_EQ(GetMaxFrameSize(), kNewMaxFrameSize);
  EXPECT_EQ(GetFrameSinkId(), kNewFrameSinkId);

  // Starting capture now, will result in the cached values being sent to
  // |capturer_|. So, expect that these calls are made and verify the values.
  EXPECT_CALL(capturer_, MockSetMinCapturePeriod(_));
  EXPECT_CALL(capturer_, MockSetMinSizeChangePeriod(_));
  EXPECT_CALL(capturer_, MockSetResolutionConstraints(_, _, _));
  EXPECT_CALL(capturer_, MockChangeTarget(_, _));
  EXPECT_CALL(capturer_,
              MockStart(_, viz::mojom::BufferFormatPreference::kDefault));
  StartCaptureWithMockCapturer();
  base::RunLoop().RunUntilIdle();
  // Verify that the previously cached values are sent to |capturer_|.
  EXPECT_EQ(capturer_.min_capture_period(), kNewMinCapturePeriod);
  EXPECT_EQ(capturer_.min_frame_size(), kNewMinFrameSize);
  EXPECT_EQ(capturer_.max_frame_size(), kNewMaxFrameSize);
  EXPECT_EQ(capturer_.frame_sink_id(), kNewFrameSinkId);
}

// Tests that DevToolsVideoConsumer::IsValidMinAndMaxFrameSize adheres to the
// limits set by media::limits::kMaxDimension
TEST_F(DevToolsVideoConsumerTest, IsValidMinAndMaxFrameSize) {
  // Choosing valid frame sizes with
  // kNormalMinSize.height() > kNormalMaxSize.width() so that width
  // and height are not interchanged in this test.
  constexpr gfx::Size kNormalMinSize = gfx::Size(50, 150);
  constexpr gfx::Size kNormalMaxSize = gfx::Size(100, 200);

  // Testing success cases.
  EXPECT_TRUE(IsValidMinAndMaxFrameSize(kNormalMinSize, kNormalMaxSize));
  // Non-zero frames that are equal should pass.
  EXPECT_TRUE(IsValidMinAndMaxFrameSize(kNormalMinSize, kNormalMaxSize));
  // Swapping width and height of frames should pass.
  EXPECT_TRUE(IsValidMinAndMaxFrameSize(
      gfx::Size(kNormalMinSize.height(), kNormalMinSize.width()),
      gfx::Size(kNormalMaxSize.height(), kNormalMaxSize.width())));

  // Testing failure cases.
  // |min_frame_size|.width() should be > 0
  EXPECT_FALSE(IsValidMinAndMaxFrameSize(gfx::Size(0, kNormalMinSize.height()),
                                         kNormalMaxSize));
  // |min_frame_size|.height() should be > 0
  EXPECT_FALSE(IsValidMinAndMaxFrameSize(gfx::Size(kNormalMinSize.width(), 0),
                                         kNormalMaxSize));
  // |min_frame_size|.width() should be <= |max_frame_size|.width()
  EXPECT_FALSE(IsValidMinAndMaxFrameSize(
      gfx::Size(kNormalMaxSize.width() + 1, kNormalMinSize.height()),
      kNormalMaxSize));
  // |max_frame_size|.height() should be <= |max_frame_size|.height()
  EXPECT_FALSE(IsValidMinAndMaxFrameSize(
      gfx::Size(kNormalMinSize.width(), kNormalMaxSize.height() + 1),
      kNormalMaxSize));
  // |max_frame_size|.height() should be <= media::limits::kMaxDimension
  EXPECT_FALSE(IsValidMinAndMaxFrameSize(
      kNormalMinSize,
      gfx::Size(kNormalMaxSize.width(), media::limits::kMaxDimension + 1)));
  // |max_frame_size|.width() should be <= media::limits::kMaxDimension
  EXPECT_FALSE(IsValidMinAndMaxFrameSize(
      kNormalMinSize,
      gfx::Size(media::limits::kMaxDimension + 1, kNormalMaxSize.height())));
}

}  // namespace content
