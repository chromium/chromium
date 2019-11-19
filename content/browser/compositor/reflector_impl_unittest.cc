// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/reflector_impl.h"

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_candidate_validator.h"
#include "components/viz/test/test_context_provider.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
#include "content/browser/compositor/reflector_texture.h"
#include "content/browser/compositor/test/test_image_transport_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/test_context_factories.h"

#if defined(USE_OZONE)
#include "components/viz/service/display/overlay_candidate_list.h"
#include "components/viz/service/display_embedder/overlay_candidate_validator_ozone.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#endif  // defined(USE_OZONE)

namespace content {
namespace {
class FakeTaskRunner : public base::SingleThreadTaskRunner {
 public:
  FakeTaskRunner() {}

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    return true;
  }
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    return true;
  }
  bool RunsTasksInCurrentSequence() const override { return true; }

 protected:
  ~FakeTaskRunner() override {}
};

#if defined(USE_OZONE)
class TestOverlayCandidatesOzone : public ui::OverlayCandidatesOzone {
 public:
  TestOverlayCandidatesOzone() {}
  ~TestOverlayCandidatesOzone() override {}

  void CheckOverlaySupport(OverlaySurfaceCandidateList* surfaces) override {
    for (auto& surface : *surfaces)
      surface.overlay_handled = true;
  }
};
#endif  // defined(USE_OZONE)

std::unique_ptr<viz::OverlayCandidateValidator> CreateTestValidator() {
#if defined(USE_OZONE)
  std::vector<viz::OverlayStrategy> strategies = {
      viz::OverlayStrategy::kSingleOnTop, viz::OverlayStrategy::kUnderlay};
  return std::make_unique<viz::OverlayCandidateValidatorOzone>(
      std::make_unique<TestOverlayCandidatesOzone>(), std::move(strategies));
#else
  return nullptr;
#endif  // defined(USE_OZONE)
}

class TestOverlayProcessor : public viz::OverlayProcessor {
 public:
  TestOverlayProcessor() : OverlayProcessor(CreateTestValidator()) {}

  viz::OverlayCandidateValidator* get_overlay_validator() const {
    return overlay_validator_.get();
  }
};

class TestOutputSurface : public BrowserCompositorOutputSurface {
 public:
  TestOutputSurface(scoped_refptr<viz::ContextProvider> context_provider)
      : BrowserCompositorOutputSurface(std::move(context_provider)) {}

  void SetFlip(bool flip) { capabilities_.flipped_output_surface = flip; }

  void BindToClient(viz::OutputSurfaceClient* client) override {}
  void EnsureBackbuffer() override {}
  void DiscardBackbuffer() override {}
  void BindFramebuffer() override {}
  void SetDrawRectangle(const gfx::Rect& draw_rectangle) override {}
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override {}
  void SwapBuffers(viz::OutputSurfaceFrame frame) override {}
  uint32_t GetFramebufferCopyTextureFormat() override { return GL_RGB; }
  bool IsDisplayedAsOverlayPlane() const override { return false; }
  unsigned GetOverlayTextureId() const override { return 0; }
  gfx::BufferFormat GetOverlayBufferFormat() const override {
    return gfx::BufferFormat::RGBX_8888;
  }

  void OnReflectorChanged() override {
    if (!reflector_) {
      reflector_texture_.reset();
    } else {
      reflector_texture_.reset(new ReflectorTexture(context_provider()));
      reflector_->OnSourceTextureMailboxUpdated(reflector_texture_->mailbox());
    }
  }

  unsigned UpdateGpuFence() override { return 0; }

 private:
  std::unique_ptr<ReflectorTexture> reflector_texture_;
};

const gfx::Rect kSubRect(0, 0, 64, 64);
const gfx::Size kSurfaceSize(256, 256);

}  // namespace

class ReflectorImplTest : public testing::Test {
 public:
  void SetUp() override {
    const bool enable_pixel_output = false;
    context_factories_ =
        std::make_unique<ui::TestContextFactories>(enable_pixel_output);

    ImageTransportFactory::SetFactory(
        std::make_unique<TestImageTransportFactory>());
    task_runner_ = base::ThreadTaskRunnerHandle::Get();
    compositor_task_runner_ = new FakeTaskRunner();
    begin_frame_source_ = std::make_unique<viz::DelayBasedBeginFrameSource>(
        std::make_unique<viz::DelayBasedTimeSource>(
            compositor_task_runner_.get()),
        viz::BeginFrameSource::kNotRestartableId);
    compositor_ = std::make_unique<ui::Compositor>(
        context_factories_->GetContextFactoryPrivate()->AllocateFrameSinkId(),
        context_factories_->GetContextFactory(),
        context_factories_->GetContextFactoryPrivate(),
        compositor_task_runner_.get(), false /* enable_pixel_canvas */);
    compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);

    auto context_provider = viz::TestContextProvider::Create();
    context_provider->BindToCurrentThread();
    output_surface_ =
        std::make_unique<TestOutputSurface>(std::move(context_provider));
    overlay_processor_ = std::make_unique<TestOverlayProcessor>();

    root_layer_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));
    compositor_->SetRootLayer(root_layer_.get());
    mirroring_layer_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));
    compositor_->root_layer()->Add(mirroring_layer_.get());
    output_surface_->Reshape(kSurfaceSize, 1.f, gfx::ColorSpace(), false,
                             false);
    mirroring_layer_->SetBounds(gfx::Rect(kSurfaceSize));
  }

  void SetUpReflector() {
    reflector_ = std::make_unique<ReflectorImpl>(compositor_.get(),
                                                 mirroring_layer_.get());
    reflector_->OnSourceSurfaceReady(output_surface_.get());
  }

  void TearDown() override {
    if (reflector_)
      reflector_->RemoveMirroringLayer(mirroring_layer_.get());
    viz::TransferableResource resource;
    std::unique_ptr<viz::SingleReleaseCallback> release;
    if (mirroring_layer_->PrepareTransferableResource(nullptr, &resource,
                                                      &release)) {
      release->Run(gpu::SyncToken(), false);
    }
    compositor_.reset();
    context_factories_.reset();
    ImageTransportFactory::Terminate();
  }

  void UpdateTexture() {
    reflector_->OnSourcePostSubBuffer(kSubRect, kSurfaceSize);
  }

#if defined(USE_OZONE)
  void ProcessForOverlays(
      viz::OverlayProcessor::OutputSurfaceOverlayPlane* output_surface_plane,
      viz::OverlayCandidateList* surfaces) {
    overlay_processor_->SetSoftwareMirrorMode(
        output_surface_->IsSoftwareMirrorMode());
    DCHECK(overlay_processor_->get_overlay_validator());
    overlay_processor_->get_overlay_validator()->CheckOverlaySupport(
        output_surface_plane, surfaces);
  }
#endif  // defined(USE_OZONE)

 protected:
  std::unique_ptr<ui::TestContextFactories> context_factories_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  std::unique_ptr<viz::SyntheticBeginFrameSource> begin_frame_source_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<ui::Compositor> compositor_;
  std::unique_ptr<ui::Layer> root_layer_;
  std::unique_ptr<ui::Layer> mirroring_layer_;
  std::unique_ptr<ReflectorImpl> reflector_;
  std::unique_ptr<TestOutputSurface> output_surface_;
  std::unique_ptr<TestOverlayProcessor> overlay_processor_;
};

namespace {
TEST_F(ReflectorImplTest, CheckNormalOutputSurface) {
  // TODO(jonross): Re-enable once Reflector is re-written to work with
  // VizDisplayCompositor. https://crbug.com/601869
  if (features::IsVizDisplayCompositorEnabled())
    return;
  output_surface_->SetFlip(false);
  SetUpReflector();
  UpdateTexture();
  EXPECT_TRUE(mirroring_layer_->TextureFlipped());
  gfx::Rect expected_rect = kSubRect + gfx::Vector2d(0, kSurfaceSize.height()) -
                            gfx::Vector2d(0, kSubRect.height());
  EXPECT_EQ(expected_rect, mirroring_layer_->damaged_region());
}

TEST_F(ReflectorImplTest, CheckInvertedOutputSurface) {
  // TODO(jonross): Re-enable once Reflector is re-written to work with
  // VizDisplayCompositor. https://crbug.com/601869
  if (features::IsVizDisplayCompositorEnabled())
    return;
  output_surface_->SetFlip(true);
  SetUpReflector();
  UpdateTexture();
  EXPECT_FALSE(mirroring_layer_->TextureFlipped());
  EXPECT_EQ(kSubRect, mirroring_layer_->damaged_region());
}

#if defined(USE_OZONE)
TEST_F(ReflectorImplTest, CheckOverlayNoReflector) {
  // TODO(jonross): Re-enable once Reflector is re-written to work with
  // VizDisplayCompositor. https://crbug.com/601869
  if (features::IsVizDisplayCompositorEnabled())
    return;
  viz::OverlayCandidateList list;
  viz::OverlayProcessor::OutputSurfaceOverlayPlane output_surface_plane;
  viz::OverlayCandidate overlay_plane;
  overlay_plane.plane_z_order = 1;
  list.push_back(overlay_plane);
  ProcessForOverlays(&output_surface_plane, &list);
  EXPECT_TRUE(list[0].overlay_handled);
}

TEST_F(ReflectorImplTest, CheckOverlaySWMirroring) {
  // TODO(jonross): Re-enable once Reflector is re-written to work with
  // VizDisplayCompositor. https://crbug.com/601869
  if (features::IsVizDisplayCompositorEnabled())
    return;
  SetUpReflector();
  viz::OverlayCandidateList list;
  viz::OverlayProcessor::OutputSurfaceOverlayPlane output_surface_plane;
  viz::OverlayCandidate overlay_plane;
  overlay_plane.plane_z_order = 1;
  list.push_back(overlay_plane);
  ProcessForOverlays(&output_surface_plane, &list);
  EXPECT_FALSE(list[0].overlay_handled);
}
#endif  // defined(USE_OZONE)

}  // namespace
}  // namespace content
