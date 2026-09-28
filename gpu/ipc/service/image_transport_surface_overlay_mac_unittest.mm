// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface_overlay_mac.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "build/buildflag.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accelerated_widget_mac/ca_layer_tree_coordinator.h"
#include "ui/accelerated_widget_mac/ca_renderer_layer_tree.h"

using testing::_;
using testing::NiceMock;

namespace gpu {
namespace {

// Mock for CALayerTreeCoordinator to verify interactions.
class MockCALayerTreeCoordinator : public ui::CALayerTreeCoordinator {
 public:
  MockCALayerTreeCoordinator()
      : CALayerTreeCoordinator(false,
                               base::DoNothing(),
                               base::BindRepeating([] { return true; }),
                               nil),
        ca_renderer_layer_tree_(new ui::CARendererLayerTree(true, true)) {}

  ~MockCALayerTreeCoordinator() override = default;

  MOCK_METHOD(void,
              CommitPresentedFrameToCA,
              (base::TimeDelta, base::TimeTicks),
              (override));

  void Present(
      gl::Presenter::SwapCompletionCallback completion_callback,
      gl::Presenter::PresentationCallback presentation_callback) override {
    num_pending_swaps_++;
  }

  int NumPendingSwaps() const override { return num_pending_swaps_; }

  void DecreasePendingSwaps() { num_pending_swaps_--; }

 private:
  std::unique_ptr<ui::CARendererLayerTree> ca_renderer_layer_tree_;
  int num_pending_swaps_ = 0;
};

}  // namespace
}  // namespace gpu

namespace gpu {

class ImageTransportSurfaceOverlayMacTest : public testing::Test {
 public:
  ImageTransportSurfaceOverlayMacTest() = default;
  ~ImageTransportSurfaceOverlayMacTest() override = default;

  // testing::Test:default
  void SetUp() override {
#if BUILDFLAG(IS_MAC)
    std::unique_ptr<ui::VSyncCallbackMac> vsync_callback_mac = base::WrapUnique(
        new ui::VSyncCallbackMac(base::DoNothing(), base::DoNothing(), false));
#endif

    auto* mock = new MockCALayerTreeCoordinator();
    mock_ca_layer_tree_coordinator_ = mock;

    surface_overlay_mac_ =
        base::MakeRefCounted<ImageTransportSurfaceOverlayMacEGL>(
#if BUILDFLAG(IS_MAC)
            base::WrapUnique(mock), std::move(vsync_callback_mac));
#else
            base::WrapUnique(mock));
#endif

    surface_overlay_mac_->SetMaxPendingSwaps(kMaxPendingSwaps);
  }

  void TearDown() override {
    // To prevent a dandling raw_ptr.
    mock_ca_layer_tree_coordinator_ = nullptr;
  }

 protected:
  static constexpr int kMaxPendingSwaps = 2;

  void PresentFrame(bool is_handling_interaction) {
    gfx::FrameData data;
#if BUILDFLAG(IS_MAC)
    data.is_handling_interaction = is_handling_interaction;
#endif
    surface_overlay_mac_->Present(base::DoNothing(), base::DoNothing(),
                                  std::move(data));
  }

  raw_ptr<MockCALayerTreeCoordinator> mock_ca_layer_tree_coordinator_;
  scoped_refptr<ImageTransportSurfaceOverlayMacEGL> surface_overlay_mac_;
};

TEST_F(ImageTransportSurfaceOverlayMacTest,
       PresentCommitsImmediatelyWhenNotInteracting) {
  // On IOS, it always commits immediately.
  // On Mac, it will commit immediately when it's not handling interaction.

  // The first frame is committed immediately.
  EXPECT_CALL(*mock_ca_layer_tree_coordinator_, CommitPresentedFrameToCA(_, _))
      .WillOnce([this]() {
        mock_ca_layer_tree_coordinator_->DecreasePendingSwaps();
      });
  PresentFrame(/*is_handling_interaction=*/false);
  testing::Mock::VerifyAndClearExpectations(mock_ca_layer_tree_coordinator_);

  // The second frame is committed immediately.
  EXPECT_CALL(*mock_ca_layer_tree_coordinator_, CommitPresentedFrameToCA(_, _))
      .WillOnce([this]() {
        mock_ca_layer_tree_coordinator_->DecreasePendingSwaps();
      });
  PresentFrame(/*is_handling_interaction=*/false);
  testing::Mock::VerifyAndClearExpectations(mock_ca_layer_tree_coordinator_);
}

#if BUILDFLAG(IS_MAC)
TEST_F(ImageTransportSurfaceOverlayMacTest,
       PresentDelaysCommitWhenHandlingInteraction) {
  // The first two frames will be pending.
  EXPECT_CALL(*mock_ca_layer_tree_coordinator_, CommitPresentedFrameToCA(_, _))
      .Times(0);
  PresentFrame(/*is_handling_interaction=*/true);
  PresentFrame(/*is_handling_interaction=*/true);
  testing::Mock::VerifyAndClearExpectations(mock_ca_layer_tree_coordinator_);

  // At the first VSync callback, the first frame is committed.
  EXPECT_CALL(*mock_ca_layer_tree_coordinator_, CommitPresentedFrameToCA(_, _))
      .WillOnce([this]() {
        mock_ca_layer_tree_coordinator_->DecreasePendingSwaps();
      });
  ui::VSyncParamsMac params;
  surface_overlay_mac_->OnVSyncPresentation(params);
  testing::Mock::VerifyAndClearExpectations(mock_ca_layer_tree_coordinator_);

  // At the second VSync callback, the second frame is committed.
  EXPECT_CALL(*mock_ca_layer_tree_coordinator_, CommitPresentedFrameToCA(_, _))
      .WillOnce([this]() {
        mock_ca_layer_tree_coordinator_->DecreasePendingSwaps();
      });
  surface_overlay_mac_->OnVSyncPresentation(params);
  testing::Mock::VerifyAndClearExpectations(mock_ca_layer_tree_coordinator_);
}

TEST_F(ImageTransportSurfaceOverlayMacTest,
       PresentDelaysCommitWhenFramesArePending) {
  // Simulate more than one pending frame.
  // The first frame is pending.
  EXPECT_CALL(*mock_ca_layer_tree_coordinator_, CommitPresentedFrameToCA(_, _))
      .Times(0);
  PresentFrame(/*is_handling_interaction=*/true);
  testing::Mock::VerifyAndClearExpectations(mock_ca_layer_tree_coordinator_);

  // With a previous pending frame, it can't commit the second frame
  // immediately even if it's not handling interaction.
  EXPECT_CALL(*mock_ca_layer_tree_coordinator_, CommitPresentedFrameToCA(_, _))
      .Times(0);
  PresentFrame(/*is_handling_interaction=*/false);
  testing::Mock::VerifyAndClearExpectations(mock_ca_layer_tree_coordinator_);
}

TEST_F(ImageTransportSurfaceOverlayMacTest,
       PresentCommitsImmediatelyIfTooManyFramesPending) {
  // Simulate that we have reached the maximum number of pending swaps.
  EXPECT_CALL(*mock_ca_layer_tree_coordinator_, CommitPresentedFrameToCA(_, _))
      .Times(0);

  PresentFrame(/*is_handling_interaction=*/true);
  PresentFrame(/*is_handling_interaction=*/true);
  testing::Mock::VerifyAndClearExpectations(mock_ca_layer_tree_coordinator_);

  // Even if interacting.
  EXPECT_CALL(*mock_ca_layer_tree_coordinator_, CommitPresentedFrameToCA(_, _))
      .Times(1);
  PresentFrame(/*is_handling_interaction=*/true);
  testing::Mock::VerifyAndClearExpectations(mock_ca_layer_tree_coordinator_);
}
#endif

}  // namespace gpu
