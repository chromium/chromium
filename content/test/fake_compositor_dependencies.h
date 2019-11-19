// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FAKE_COMPOSITOR_DEPENDENCIES_H_
#define CONTENT_TEST_FAKE_COMPOSITOR_DEPENDENCIES_H_

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "cc/test/test_task_graph_runner.h"
#include "content/renderer/compositor/compositor_dependencies.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/platform/scheduler/test/web_fake_thread_scheduler.h"

namespace content {

class FakeCompositorDependencies : public CompositorDependencies {
 public:
  FakeCompositorDependencies();
  ~FakeCompositorDependencies() override;

  // CompositorDependencies implementation.
  bool IsGpuRasterizationForced() override;
  int GetGpuRasterizationMSAASampleCount() override;
  bool IsLcdTextEnabled() override;
  bool IsZeroCopyEnabled() override;
  bool IsPartialRasterEnabled() override;
  bool IsGpuMemoryBufferCompositorResourcesEnabled() override;
  bool IsElasticOverscrollEnabled() override;
  bool IsUseZoomForDSFEnabled() override;
  scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorMainThreadTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorImplThreadTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetCleanupTaskRunner() override;
  blink::scheduler::WebThreadScheduler* GetWebMainThreadScheduler() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  bool IsScrollAnimatorEnabled() override;
  std::unique_ptr<cc::UkmRecorderFactory> CreateUkmRecorderFactory() override;
  void RequestNewLayerTreeFrameSink(
      RenderWidget* render_widget,
      scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue,
      const GURL& url,
      LayerTreeFrameSinkCallback callback,
      mojo::PendingReceiver<mojom::RenderFrameMetadataObserverClient>
          render_frame_metadata_observer_client_receiver,
      mojo::PendingRemote<mojom::RenderFrameMetadataObserver>
          render_frame_metadata_observer_remote,
      const char* client_name) override;
#ifdef OS_ANDROID
  bool UsingSynchronousCompositing() override;
#endif

  void set_use_zoom_for_dsf_enabled(bool enabled) {
    use_zoom_for_dsf_ = enabled;
  }

 private:
  cc::TestTaskGraphRunner task_graph_runner_;
  blink::scheduler::WebFakeThreadScheduler main_thread_scheduler_;
  bool use_zoom_for_dsf_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeCompositorDependencies);
};

}  // namespace content

#endif  // CONTENT_TEST_FAKE_COMPOSITOR_DEPENDENCIES_H_
