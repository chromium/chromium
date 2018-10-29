// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/reflector_impl.h"

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display_embedder/compositor_overlay_candidate_validator.h"
#include "components/viz/test/test_context_provider.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
#include "content/browser/compositor/reflector_texture.h"
#include "content/browser/compositor/test/test_image_transport_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/context_factories_for_test.h"

#if defined(USE_OZONE)
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display_embedder/compositor_overlay_candidate_validator_ozone.h"
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
    (*surfaces)[0].overlay_handled = true;
  }
};
#endif  // defined(USE_OZONE)

std::unique_ptr<viz::CompositorOverlayCandidateValidator>
CreateTestValidatorOzone() {
#if defined(USE_OZONE)
  return std::unique_ptr<viz::CompositorOverlayCandidateValidator>(
      new viz::CompositorOverlayCandidateValidatorOzone(
          std::unique_ptr<ui::OverlayCandidatesOzone>(
              new TestOverlayCandidatesOzone()),
          ""));
#else
  return nullptr;
#endif  // defined(USE_OZONE)
}

class TestOutputSurface : public BrowserCompositorOutputSurface {
 public:
  TestOutputSurface(scoped_refptr<viz::ContextProvider> context_provider)
      : BrowserCompositorOutputSurface(std::move(context_provider),
                                       UpdateVSyncParametersCallback(),
                                       CreateTestValidatorOzone()) {}

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

#if BUILDFLAG(ENABLE_VULKAN)
  gpu::VulkanSurface* GetVulkanSurface() override { return nullptr; }
#endif
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
    bool enable_pixel_output = false;
    ui::ContextFactory* context_factory = nullptr;
    ui::ContextFactoryPrivate* context_factory_private = nullptr;

    ui::InitializeContextFactoryForTests(enable_pixel_output, &context_factory,
                                         &context_factory_private);
    ImageTransportFactory::SetFactory(
        std::make_unique<TestImageTransportFactory>());
    task_runner_ = base::ThreadTaskRunnerHandle::Get();
    compositor_task_runner_ = new FakeTaskRunner();
    begin_frame_source_ = std::make_unique<viz::DelayBasedBeginFrameSource>(
        std::make_unique<viz::DelayBasedTimeSource>(
            compositor_task_runner_.get()),
        viz::BeginFrameSource::kNotRestartableId);
    compositor_.reset(new ui::Compositor(
        context_factory_private->AllocateFrameSinkId(), context_factory,
        context_factory_private, compositor_task_runner_.get(),
        false /* enable_surface_synchronization */,
        false /* enable_pixel_canvas */));
    compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);

    auto context_provider = viz::TestContextProvider::Create();
    context_provider->BindToCurrentThread();
    output_surface_ =
        std::make_unique<TestOutputSurface>(std::move(context_provider));

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
    ui::TerminateContextFactoryForTests();
    ImageTransportFactory::Terminate();
  }

  void UpdateTexture() {
    reflector_->OnSourcePostSubBuffer(kSubRect, kSurfaceSize);
  }

 protected:
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  std::unique_ptr<viz::SyntheticBeginFrameSource> begin_frame_source_;
  base::test::ScopedTaskEnvironment task_environment_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<ui::Compositor> compositor_;
  std::unique_ptr<ui::Layer> root_layer_;
  std::unique_ptr<ui::Layer> mirroring_layer_;
  std::unique_ptr<ReflectorImpl> reflector_;
  std::unique_ptr<TestOutputSurface> output_surface_;
};

namespace {
TEST_F(ReflectorImplTest, CheckNormalOutputSurface) {
  // TODO(jonross): Re-enable once Reflector is re-written to work with
  // VizDisplayCompositor. https://crbug.com/601869
  if (base::FeatureList::IsEnabled(features::kVizDisplayCompositor))
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
  if (base::FeatureList::IsEnabled(features::kVizDisplayCompositor))
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
  if (base::FeatureList::IsEnabled(features::kVizDisplayCompositor))
    return;
  viz::OverlayCandidateList list;
  viz::OverlayCandidate plane_1, plane_2;
  plane_1.plane_z_order = 0;
  plane_2.plane_z_order = 1;
  list.push_back(plane_1);
  list.push_back(plane_2);
  output_surface_->GetOverlayCandidateValidator()->CheckOverlaySupport(&list);
  EXPECT_TRUE(list[0].overlay_handled);
}

TEST_F(ReflectorImplTest, CheckOverlaySWMirroring) {
  // TODO(jonross): Re-enable once Reflector is re-written to work with
  // VizDisplayCompositor. https://crbug.com/601869
  if (base::FeatureList::IsEnabled(features::kVizDisplayCompositor))
    return;
  SetUpReflector();
  viz::OverlayCandidateList list;
  viz::OverlayCandidate plane_1, plane_2;
  plane_1.plane_z_order = 0;
  plane_2.plane_z_order = 1;
  list.push_back(plane_1);
  list.push_back(plane_2);
  output_surface_->GetOverlayCandidateValidator()->CheckOverlaySupport(&list);
  EXPECT_FALSE(list[0].overlay_handled);
}
#endif  // defined(USE_OZONE)

}  // namespace
}  // namespace content
