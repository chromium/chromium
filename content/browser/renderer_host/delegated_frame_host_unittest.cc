// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/delegated_frame_host.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "components/viz/client/frame_evictor.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/test/test_frame_sink_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_image_transport_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_surface.h"
#include "ui/compositor/test/test_context_factories.h"

namespace content {

namespace {

class CapturingFrameSinkManager : public viz::TestFrameSinkManagerImpl {
 public:
  void RequestCopyOfOutput(const viz::SurfaceId& surface_id,
                           std::unique_ptr<viz::CopyOutputRequest> request,
                           bool capture_exact_surface_id,
                           base::TimeDelta timeout) override {
    last_request_is_secure_ = request->is_secure();
  }

  std::optional<bool> TakeLastRequestIsSecure() {
    return std::exchange(last_request_is_secure_, std::nullopt);
  }

 private:
  std::optional<bool> last_request_is_secure_;
};

class CapturingImageTransportFactory : public TestImageTransportFactory {
 public:
  CapturingImageTransportFactory() {
    host_frame_sink_manager_.SetLocalManager(&frame_sink_manager_);
  }

  viz::HostFrameSinkManager* GetHostFrameSinkManager() override {
    return &host_frame_sink_manager_;
  }

  CapturingFrameSinkManager& frame_sink_manager() {
    return frame_sink_manager_;
  }

 private:
  CapturingFrameSinkManager frame_sink_manager_;
  viz::HostFrameSinkManager host_frame_sink_manager_;
};

}  // namespace

class MockDelegatedFrameHostClient : public DelegatedFrameHostClient {
 public:
  MockDelegatedFrameHostClient() = default;
  ~MockDelegatedFrameHostClient() override = default;

  MOCK_METHOD(ui::LayerSurface*,
              GetDelegatedFrameHostLayer,
              (),
              (const, override));
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

  void TearDown() override {
    if (delegated_frame_host_) {
      delegated_frame_host_->DetachFromCompositor();
    }
    delegated_frame_host_.reset();
    compositor_.reset();
    image_transport_factory_ = nullptr;
    ImageTransportFactory::Terminate();
  }

 protected:
  testing::StrictMock<MockDelegatedFrameHostClient> mock_client_;
  raw_ptr<CapturingImageTransportFactory> image_transport_factory_ = nullptr;

 private:
  BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS,
      base::test::TaskEnvironment::MainThreadType::UI};
  std::unique_ptr<DelegatedFrameHost> delegated_frame_host_;
  ui::TestContextFactories context_factory_{/*enable_pixel_output=*/false};
  std::unique_ptr<ui::Compositor> compositor_;
};

void DelegatedFrameHostTest::SetUp() {
  auto factory = std::make_unique<CapturingImageTransportFactory>();
  image_transport_factory_ = factory.get();
  ImageTransportFactory::SetFactory(std::move(factory));
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
      /*is_copy_request_secure=*/false, base::TimeDelta(),
      base::BindOnce(
          [](base::RepeatingClosure quit_closure,
             const content::CopyFromSurfaceResult& result) {
            EXPECT_FALSE(result.has_value());
            quit_closure.Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(
      std::nullopt,
      image_transport_factory_->frame_sink_manager().TakeLastRequestIsSecure());
}

TEST_F(DelegatedFrameHostTest, CopyFromCompositingSurfaceSetsIsSecure) {
  auto* dfh = delegated_frame_host();
  ui::LayerSurface layer;
  EXPECT_CALL(mock_client_, GetDelegatedFrameHostLayer)
      .WillOnce(testing::Return(&layer));
  EXPECT_CALL(mock_client_, DelegatedFrameHostIsVisible)
      .WillOnce(testing::Return(false));

  const viz::LocalSurfaceId local_surface_id(1, 1,
                                             base::UnguessableToken::Create());
  dfh->EmbedSurface(local_surface_id, gfx::Size(100, 100),
                    cc::DeadlinePolicy::UseDefaultDeadline());
  EXPECT_TRUE(dfh->CanCopyFromCompositingSurface());

  for (bool is_secure : {false, true}) {
    base::RunLoop run_loop;
    dfh->CopyFromCompositingSurface(
        /*src_subrect=*/gfx::Rect(),
        /*output_size=*/gfx::Size(),
        /*is_copy_request_secure=*/is_secure, base::TimeDelta(),
        base::BindOnce(
            [](base::RepeatingClosure quit_closure,
               const content::CopyFromSurfaceResult& result) {
              EXPECT_FALSE(result.has_value());
              quit_closure.Run();
            },
            run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(is_secure, image_transport_factory_->frame_sink_manager()
                             .TakeLastRequestIsSecure());
  }
}

TEST_F(DelegatedFrameHostTest, ForceSpecifiedDeadline) {
  auto* dfh = delegated_frame_host();
  EXPECT_EQ(std::nullopt, dfh->GetForceSpecifiedDeadlineForTesting());
  dfh->SetForceSpecifiedDeadline(5);
  EXPECT_EQ(5u, dfh->GetForceSpecifiedDeadlineForTesting());
  dfh->SetForceSpecifiedDeadline(std::nullopt);
  EXPECT_EQ(std::nullopt, dfh->GetForceSpecifiedDeadlineForTesting());
}

}  // namespace content
