// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/delegated_frame_host.h"

#include <cstddef>
#include <memory>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/client/frame_evictor.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_image_transport_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/test_context_factories.h"

namespace content {

class MockDelegatedFrameHostClient : public DelegatedFrameHostClient {
 public:
  MockDelegatedFrameHostClient() = default;
  ~MockDelegatedFrameHostClient() override = default;

  MOCK_METHOD(ui::Layer*, DelegatedFrameHostGetLayer, (), (const, override));
  MOCK_METHOD(bool, DelegatedFrameHostIsVisible, (), (const, override));
  MOCK_METHOD(SkColor, DelegatedFrameHostGetGutterColor, (), (const, override));
  MOCK_METHOD2(OnFrameTokenChanged,
               void(uint32_t frame_token, base::TimeTicks activation_time));
  MOCK_METHOD(float, GetDeviceScaleFactor, (), (const, override));
  MOCK_METHOD0(InvalidateLocalSurfaceIdOnEviction, void());
  MOCK_METHOD(viz::FrameEvictorClient::EvictIds,
              CollectSurfaceIdsForEviction,
              (),
              (override));
  MOCK_METHOD0(ShouldShowStaleContentOnEviction, bool());
};

class DelegatedFrameHostTest : public testing::Test {
 public:
  DelegatedFrameHostTest() = default;
  ~DelegatedFrameHostTest() override = default;

  DelegatedFrameHost* delegated_frame_host() {
    return delegated_frame_host_.get();
  }

  void SetUp() override;

 private:
  BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS,
      base::test::TaskEnvironment::MainThreadType::UI};
  testing::StrictMock<MockDelegatedFrameHostClient> mock_client_;
  std::unique_ptr<DelegatedFrameHost> delegated_frame_host_;
  ui::TestContextFactories context_factory_{/*enable_pixel_output=*/false};
  std::unique_ptr<ui::Compositor> compositor_;
};

void DelegatedFrameHostTest::SetUp() {
  ImageTransportFactory::SetFactory(
      std::make_unique<TestImageTransportFactory>());
  viz::FrameSinkId frame_sink_id =
      context_factory_.GetContextFactory()->AllocateFrameSinkId();
  compositor_ = std::make_unique<ui::Compositor>(
      frame_sink_id, context_factory_.GetContextFactory(),
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      /*enable_pixel_canvas=*/false);

  EXPECT_CALL(mock_client_, DelegatedFrameHostIsVisible)
      .Times(1)
      .WillOnce(testing::Return(false));
  delegated_frame_host_ = std::make_unique<DelegatedFrameHost>(
      frame_sink_id, &mock_client_, /*should_register_frame_sink_id=*/false);
  delegated_frame_host_->AttachToCompositor(compositor_.get());
}

TEST_F(DelegatedFrameHostTest, NoCopyOutputRequestWithNoValidSurface) {
  auto* dfh = delegated_frame_host();
  EXPECT_FALSE(dfh->CanCopyFromCompositingSurface());

  // Navigating while hidden would not lead to a call to `EmbedSurface` so we
  // should remain unable to perform a copy.
  dfh->DidNavigateMainFramePreCommit();
  dfh->DidNavigate();
  EXPECT_FALSE(dfh->CanCopyFromCompositingSurface());

  // Since we have not called `DelegatedFrameHost::EmbedSurface` we have no
  // valid `viz::Surface` to perform readback for. The callback given to
  // `CopyFromCompositingSurface` is expected to run with an empty `SkBitmap`.
  //
  // `CopyFromCompositingSurface1 creates a
  // `ui::Compositor::ScopedKeepSurfaceAliveCallback` to ensure that we keep the
  // surface alive until the copy completes. This callback is bound on the UI
  // thread. `viz::CopyOutputRequest` tries to use background threadpools for
  // copying. The following will crash on DCHECK builds if we run on the wrong
  // sequence.
  base::RunLoop run_loop;
  dfh->CopyFromCompositingSurface(
      /*src_subrect=*/gfx::Rect(),
      /*output_size=*/gfx::Size(),
      base::BindOnce(
          [](base::RepeatingClosure quit_closure,
             const viz::CopyOutputBitmapWithMetadata& result) {
            EXPECT_TRUE(result.bitmap.empty());
            quit_closure.Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace content
