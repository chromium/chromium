// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/repeating_test_future.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/video_capture_target.h"
#include "components/viz/service/frame_sinks/video_capture/shared_memory_video_frame_pool.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "remoting/host/chromeos/ash_proxy.h"
#include "remoting/host/chromeos/frame_sink_desktop_capturer.h"
#include "remoting/host/chromeos/scoped_fake_ash_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace remoting {
using gfx::ColorSpace;
using gfx::Point;
using gfx::Rect;
using gfx::Size;
using media::VideoCaptureFeedback;
using media::VideoFrame;
using media::VideoPixelFormat;
using testing::Eq;
using testing::IsNull;
using webrtc::DesktopCapturer;
using webrtc::DesktopFrame;
using webrtc::DesktopRect;
using webrtc::DesktopRegion;
using webrtc::DesktopSize;

namespace {
constexpr int kAnyWidth = 800;
constexpr int kAnyHeight = 600;

const DisplayId kPrimarySourceId = 12345678;
constexpr int kDesignLimitMaxFrames = 20;
constexpr int kFramePoolCapacity = kDesignLimitMaxFrames + 1;

const auto kPixelFormat = VideoPixelFormat::PIXEL_FORMAT_ARGB;
constexpr int kMaxFrameRate = 60;
constexpr auto kMinResolution = Size(320, 180);
constexpr auto kMaxResolution = Size(3840, 2160);
constexpr bool kFixedAspectRatio = false;

const char kUmaKeyForCapturerCreated[] =
    "Enterprise.DeviceRemoteCommand.Crd.Capturer.FrameSink.Created";
const char kUmaKeyForCapturerDestroyed[] =
    "Enterprise.DeviceRemoteCommand.Crd.Capturer.FrameSink.Destroyed";

std::unique_ptr<viz::VideoFramePool> GetVideoFramePool(int capacity) {
  return std::make_unique<viz::SharedMemoryVideoFramePool>(capacity);
}

std::string ToString(DesktopSize desktop_size) {
  return base::StringPrintf("%dX%d", desktop_size.width(),
                            desktop_size.height());
}

DesktopRect ToDesktopRect(Rect rect) {
  return DesktopRect::MakeLTRB(rect.x(), rect.y(), rect.right(), rect.bottom());
}

auto HasUpdatedRegion(std::vector<DesktopRect> expected_rects) {
  DesktopRegion expected_region;
  for (DesktopRect rect : expected_rects) {
    expected_region.AddRect(rect);
  }

  return testing::Property(
      "updated_region", &DesktopFrame::updated_region,
      testing::Truly([expected_region](const DesktopRegion& actual_region) {
        return actual_region.Equals(expected_region);
      }));
}

struct CaptureResult {
  DesktopCapturer::Result result;
  std::unique_ptr<DesktopFrame> frame;
};

class FrameParameters {
 public:
  FrameParameters() = default;

  FrameParameters WithSize(Size size) {
    size_ = size;
    return *this;
  }

  FrameParameters WithUpdatedRegion(Rect updated_region) {
    updated_region_ = updated_region;
    return *this;
  }

  FrameParameters WithScaleFactor(float scale_factor) {
    scale_factor_ = scale_factor;
    return *this;
  }

  const Size& size() { return size_; }
  const Rect& updated_region() { return updated_region_; }
  float scale_factor() { return scale_factor_; }

 private:
  Size size_{kAnyWidth, kAnyHeight};
  Rect updated_region_{0, 0, kAnyWidth, kAnyHeight};
  float scale_factor_ = 1.0;
};

class DesktopCapturerCallback : public DesktopCapturer::Callback {
 public:
  DesktopCapturerCallback() = default;
  DesktopCapturerCallback(const DesktopCapturerCallback&) = delete;
  DesktopCapturerCallback& operator=(const DesktopCapturerCallback&) = delete;
  ~DesktopCapturerCallback() override = default;

  CaptureResult WaitForResult() {
    EXPECT_TRUE(result_.Wait());
    return result_.Take();
  }

  void OnCaptureResult(DesktopCapturer::Result result,
                       std::unique_ptr<DesktopFrame> frame) override {
    result_.AddValue(CaptureResult{result, std::move(frame)});
  }

 private:
  base::test::RepeatingTestFuture<CaptureResult> result_;
};

// Helper class that keeps the ref_ptr to a video frame alive until the Done()
// callback is invoked.
class FakeFrameDeliveryCallback final
    : public viz::mojom::FrameSinkVideoConsumerFrameCallbacks {
 public:
  explicit FakeFrameDeliveryCallback(scoped_refptr<VideoFrame> frame)
      : frame_(frame) {}

  FakeFrameDeliveryCallback(const FakeFrameDeliveryCallback&) = delete;
  FakeFrameDeliveryCallback& operator=(const FakeFrameDeliveryCallback&) =
      delete;
  ~FakeFrameDeliveryCallback() override = default;

  void Done() override { frame_.reset(); }
  void ProvideFeedback(const VideoCaptureFeedback& feedback) override {}

  mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  scoped_refptr<VideoFrame> frame_;
  mojo::Receiver<viz::mojom::FrameSinkVideoConsumerFrameCallbacks> receiver_{
      this};
};

class MockFrameSinkVideoCapturer : public viz::mojom::FrameSinkVideoCapturer {
 public:
  explicit MockFrameSinkVideoCapturer(
      mojo::Remote<viz::mojom::FrameSinkVideoConsumer>& consumer_remote) {
    this->remote_ = std::move(consumer_remote);
    this->frame_pool_ = GetVideoFramePool(kFramePoolCapacity);

    ON_CALL(*this, Start)
        .WillByDefault(
            [this](mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumer>
                       consumer_remote,
                   viz::mojom::BufferFormatPreference) {
              this->remote_.Bind(std::move(consumer_remote));
            });
  }

  MOCK_METHOD(void, SetFormat, (VideoPixelFormat format));

  MOCK_METHOD(void, SetMinCapturePeriod, (base::TimeDelta min_period));

  MOCK_METHOD(void, SetMinSizeChangePeriod, (base::TimeDelta min_period));

  MOCK_METHOD(void, SetAutoThrottlingEnabled, (bool enabled));

  MOCK_METHOD(void,
              SetResolutionConstraints,
              (const Size& min_size,
               const Size& max_size,
               bool use_fixed_aspect_ratio));

  MOCK_METHOD(void,
              ChangeTarget,
              (const absl::optional<viz::VideoCaptureTarget>& target,
               uint32_t crop_version));

  MOCK_METHOD(void,
              Start,
              (mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumer> consumer,
               viz::mojom::BufferFormatPreference buffer_format_preference));

  MOCK_METHOD(void, Stop, ());

  MOCK_METHOD(void, RequestRefreshFrame, ());

  MOCK_METHOD(void,
              CreateOverlay,
              (int32_t stacking_index,
               mojo::PendingReceiver<viz::mojom::FrameSinkVideoCaptureOverlay>
                   receiver));

  MOCK_METHOD(void, FrameReceived, (scoped_refptr<VideoFrame> frame));

  void SendFrame(FrameParameters params = FrameParameters()) {
    scoped_refptr<VideoFrame> frame =
        frame_pool_->ReserveVideoFrame(kPixelFormat, params.size());
    frame->set_color_space(ColorSpace(ColorSpace::PrimaryID::ADOBE_RGB,
                                      ColorSpace::TransferID::LINEAR));

    auto handle = frame_pool_->CloneHandleForDelivery(*frame);
    ASSERT_TRUE(handle);
    ASSERT_TRUE(!handle->is_read_only_shmem_region() ||
                handle->get_read_only_shmem_region().IsValid());

    auto metadata = frame->metadata();
    metadata.device_scale_factor = params.scale_factor();
    metadata.capture_update_rect = params.updated_region();

    // Assemble frame layout, format, and metadata into a mojo struct to send to
    // the consumer.
    auto info = media::mojom::VideoFrameInfo::New(
        frame->timestamp(), metadata, frame->format(), frame->coded_size(),
        /*visible_rect=*/gfx::Rect(params.size()),
        /*is_premapped=*/false, frame->ColorSpace(),
        /*strides=*/nullptr);

    auto done_callback = std::make_unique<FakeFrameDeliveryCallback>(frame);
    Rect content_rect{params.size().width(), params.size().height()};

    remote_->OnFrameCaptured(std::move(handle), std::move(info), content_rect,
                             done_callback->BindNewPipeAndPassRemote());

    done_callbacks_.push_back(std::move(done_callback));
  }

  std::unique_ptr<viz::VideoFramePool> frame_pool_;

  mojo::Receiver<viz::mojom::FrameSinkVideoCapturer> receiver_{this};
  mojo::Remote<viz::mojom::FrameSinkVideoConsumer> remote_;

  // Store the done callback receiver that we create for each frame we send
  // (so it can receive the Done() callback).
  std::vector<std::unique_ptr<FakeFrameDeliveryCallback>> done_callbacks_;

  base::WeakPtrFactory<MockFrameSinkVideoCapturer> fake_capturer_factory_{this};
};

class FrameSinkDesktopCapturerTest : public testing::Test {
 public:
  FrameSinkDesktopCapturerTest() = default;

  test::ScopedFakeAshProxy& ash_proxy() { return ash_proxy_; }

  DesktopCapturerCallback& desktop_capturer_callback() { return callback_; }

  void SetUp() override {
    ash_proxy().SetVideoCapturerReceiver(&video_capturer_.receiver_);
    ash_proxy().AddPrimaryDisplay(kPrimarySourceId);
  }

  void StartCapturerForTesting() {
    capturer_.Start(&desktop_capturer_callback());
    FlushForTesting();
  }

  CaptureResult CaptureFrame() {
    FlushForTesting();
    capturer_.CaptureFrame();

    return desktop_capturer_callback().WaitForResult();
  }

  CaptureResult SendAndCaptureSingleFrame(
      FrameParameters params = FrameParameters()) {
    video_capturer_.SendFrame(params);
    FlushForTesting();
    capturer_.CaptureFrame();

    return desktop_capturer_callback().WaitForResult();
  }

  // We really want to flush the mojom pipe, but we can't as the mojom remote is
  // hidden inside ClientFrameSinkDesktopCapturer, so we simply RunUntilIdle()
  // instead.
  void FlushForTesting() { base::RunLoop().RunUntilIdle(); }

  FrameParameters params() { return FrameParameters(); }

  void AddMultipleDisplays() {
    ash_proxy_.AddDisplayWithId(111);
    ash_proxy_.AddDisplayWithId(222);
  }

 protected:
  base::test::SingleThreadTaskEnvironment environment_;
  DesktopCapturerCallback callback_;
  test::ScopedFakeAshProxy ash_proxy_;
  FrameSinkDesktopCapturer capturer_{ash_proxy_};

  mojo::Remote<viz::mojom::FrameSinkVideoConsumer> video_consumer_remote_;

  MockFrameSinkVideoCapturer video_capturer_{video_consumer_remote_};
};

}  // namespace

TEST_F(FrameSinkDesktopCapturerTest,
       ShouldReturnTemporaryErrorIfNoFrameWasReceived) {
  StartCapturerForTesting();

  CaptureResult result = CaptureFrame();

  EXPECT_THAT(result.result, Eq(DesktopCapturer::Result::ERROR_TEMPORARY));
  EXPECT_THAT(result.frame, Eq(nullptr));
}

TEST_F(FrameSinkDesktopCapturerTest, ShouldBindVideoCapturerInStart) {
  EXPECT_CALL(video_capturer_, Start);

  StartCapturerForTesting();
}

TEST_F(FrameSinkDesktopCapturerTest, ShouldSetParamsInStart) {
  EXPECT_CALL(video_capturer_, SetFormat(kPixelFormat));
  EXPECT_CALL(video_capturer_, SetMinCapturePeriod(base::Hertz(kMaxFrameRate)));
  EXPECT_CALL(video_capturer_, SetMinSizeChangePeriod(base::Seconds(0)));
  EXPECT_CALL(video_capturer_, SetAutoThrottlingEnabled(/*enabled=*/false));
  EXPECT_CALL(video_capturer_,
              SetResolutionConstraints(kMinResolution, kMaxResolution,
                                       kFixedAspectRatio));

  StartCapturerForTesting();
}

TEST_F(FrameSinkDesktopCapturerTest, ShouldStartByCapturingThePrimaryDisplay) {
  AddMultipleDisplays();

  DisplayId primary_display_id = ash_proxy().GetPrimaryDisplayId();
  const auto expected_target = absl::optional<viz::VideoCaptureTarget>(
      ash_proxy().GetFrameSinkId(primary_display_id));

  EXPECT_CALL(video_capturer_, ChangeTarget(expected_target, 0));

  StartCapturerForTesting();
}
TEST_F(FrameSinkDesktopCapturerTest, ShouldReturnSuccessIfFrameWasReceived) {
  StartCapturerForTesting();

  CaptureResult result = SendAndCaptureSingleFrame();

  EXPECT_THAT(result.result, Eq(DesktopCapturer::Result::SUCCESS));
}

TEST_F(FrameSinkDesktopCapturerTest,
       ShouldReturnTheFrameSentByTheVideoCapturer) {
  StartCapturerForTesting();

  CaptureResult result =
      SendAndCaptureSingleFrame(params().WithSize({110, 220}));
  auto frame_size = result.frame->size();

  // Note we don't test the actual frame data but instead we just look at the
  // resolution.
  EXPECT_EQ(ToString(frame_size), "110X220");
}

TEST_F(FrameSinkDesktopCapturerTest, ShouldReleaseMemoryOfUnusedFrames) {
  StartCapturerForTesting();

  for (int i = 0; i < 10; i++) {
    video_capturer_.SendFrame();
    FlushForTesting();
  }

  // At this point the memory of all but the last frame should have been
  // released.
  EXPECT_EQ(video_capturer_.frame_pool_->GetNumberOfReservedFrames(), 1u);
}

TEST_F(FrameSinkDesktopCapturerTest, ShouldSetDpiOfFrame) {
  StartCapturerForTesting();
  int scale_factor = 2.0;
  int dpi = ash_proxy().ScaleFactorToDpi(scale_factor);

  CaptureResult result =
      SendAndCaptureSingleFrame(params().WithScaleFactor(scale_factor));

  EXPECT_THAT(result.frame->dpi().x(), Eq(dpi));
  EXPECT_THAT(result.frame->dpi().y(), Eq(dpi));
}

TEST_F(FrameSinkDesktopCapturerTest, ShouldSetUpdatedRegionOfFrame) {
  StartCapturerForTesting();

  Rect updated_rect{50, 50, 250, 150};
  // Ignore the first frame sent, as that will have the full frame size as
  // updated rect.
  SendAndCaptureSingleFrame(params().WithUpdatedRegion(Rect{10, 10, 200, 200}));

  CaptureResult result =
      SendAndCaptureSingleFrame(params().WithUpdatedRegion(updated_rect));

  EXPECT_THAT(*result.frame, HasUpdatedRegion({ToDesktopRect(updated_rect)}));
}

TEST_F(FrameSinkDesktopCapturerTest,
       ShouldSetWholeFrameAsUpdatedRegionForFirstFrameCaptured) {
  StartCapturerForTesting();

  Rect updated_rect{50, 50, 250, 150};
  Size frame_size{800, 600};

  CaptureResult result = SendAndCaptureSingleFrame(
      params().WithSize(frame_size).WithUpdatedRegion(updated_rect));

  EXPECT_THAT(*result.frame,
              HasUpdatedRegion({ToDesktopRect(Rect{frame_size})}));
}

TEST_F(FrameSinkDesktopCapturerTest,
       ShouldHaveAnEmptyUpdatedRegionWhenSameFrameIsCapturedTwice) {
  StartCapturerForTesting();
  Rect updated_rect{50, 50, 250, 150};

  SendAndCaptureSingleFrame(params().WithUpdatedRegion(updated_rect));
  CaptureResult result = CaptureFrame();

  auto updated_region = result.frame->updated_region();

  // No method to get the dimensions of updated region.
  EXPECT_TRUE(updated_region.is_empty());
}

TEST_F(FrameSinkDesktopCapturerTest,
       ShouldAggregateUpdatedRegionOfUnconsumedFrames) {
  StartCapturerForTesting();
  Rect updated_rect_1{50, 50, 250, 150};
  Rect updated_rect_2{400, 600, 50, 50};
  Size frame_size{800, 600};

  SendAndCaptureSingleFrame(params().WithSize(frame_size));
  video_capturer_.SendFrame(
      params().WithSize(frame_size).WithUpdatedRegion(updated_rect_1));
  video_capturer_.SendFrame(
      params().WithSize(frame_size).WithUpdatedRegion(updated_rect_2));
  CaptureResult result = CaptureFrame();

  EXPECT_THAT(*result.frame, HasUpdatedRegion({ToDesktopRect(updated_rect_1),
                                               ToDesktopRect(updated_rect_2)}));
}

TEST_F(FrameSinkDesktopCapturerTest,
       ShouldSetWholeFrameAsUpdatedRegionIfFrameSizeChanges) {
  StartCapturerForTesting();
  Size first_frame_size{800, 600};
  Rect first_frame_updated_rect{50, 50, 250, 150};
  Size second_frame_size{600, 400};
  Rect second_frame_updated_rect = {10, 50, 250, 150};

  video_capturer_.SendFrame(params()
                                .WithSize(first_frame_size)
                                .WithUpdatedRegion(first_frame_updated_rect));
  CaptureResult result = SendAndCaptureSingleFrame(
      params()
          .WithSize(second_frame_size)
          .WithUpdatedRegion(second_frame_updated_rect));

  // No method to get the dimensions of updated region.
  EXPECT_THAT(*result.frame,
              HasUpdatedRegion({ToDesktopRect(Rect{second_frame_size})}));
}

TEST_F(FrameSinkDesktopCapturerTest, ShouldSetDisplayBounds) {
  int width = 666, height = 400;

  StartCapturerForTesting();

  CaptureResult result =
      SendAndCaptureSingleFrame(params().WithSize({width, height}));

  auto frame_rect = result.frame->rect();
  EXPECT_EQ(frame_rect.width(), width);
  EXPECT_EQ(frame_rect.height(), height);
}

TEST_F(FrameSinkDesktopCapturerTest, ShouldUpdateSourceOnDisplayChange) {
  DisplayId new_display_id = 2222;
  ash_proxy().AddDisplayWithId(new_display_id);

  StartCapturerForTesting();
  const absl::optional<viz::VideoCaptureTarget> expected_target(
      ash_proxy().GetFrameSinkId(new_display_id));

  EXPECT_CALL(video_capturer_, ChangeTarget(expected_target, 0));
  bool source_updated = capturer_.SelectSource(2222);
  EXPECT_TRUE(source_updated);
  FlushForTesting();
  EXPECT_EQ(capturer_.GetSourceDisplay()->id(), new_display_id);
}

TEST_F(FrameSinkDesktopCapturerTest, ShouldFailToSwitchToAnInvalidDisplay) {
  StartCapturerForTesting();
  const DisplayId previous_source_id = capturer_.GetSourceDisplay()->id();

  bool source_updated = capturer_.SelectSource(222);

  EXPECT_FALSE(source_updated);
  EXPECT_EQ(capturer_.GetSourceDisplay()->id(), previous_source_id);
}

TEST_F(FrameSinkDesktopCapturerTest,
       ShouldReturnTemporaryErrorIfDisplayNotAvailable) {
  StartCapturerForTesting();
  ash_proxy().AddDisplayWithId(222);
  bool source_updated = capturer_.SelectSource(222);

  ash_proxy().RemoveDisplay(222);

  CaptureResult result = SendAndCaptureSingleFrame();

  EXPECT_TRUE(source_updated);
  EXPECT_THAT(result.result,
              Eq(webrtc::DesktopCapturer::Result::ERROR_TEMPORARY));
  EXPECT_THAT(result.frame, IsNull());
}

TEST_F(FrameSinkDesktopCapturerTest,
       ShouldSendUmaLogsOnCapturerConstructionAndDestruction) {
  base::HistogramTester histogram_tester;

  auto my_capturer = std::make_unique<FrameSinkDesktopCapturer>();
  histogram_tester.ExpectUniqueSample(kUmaKeyForCapturerCreated, true, 1);

  my_capturer = nullptr;
  histogram_tester.ExpectUniqueSample(kUmaKeyForCapturerDestroyed, true, 1);
}

TEST_F(FrameSinkDesktopCapturerTest, ShouldNotCrashIfStartIsNeverCalled) {
  auto my_capturer = std::make_unique<FrameSinkDesktopCapturer>();
  my_capturer = nullptr;
}

}  // namespace remoting
