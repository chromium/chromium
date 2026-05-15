// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/viz/host/gpu_client.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/spare_render_process_host_manager_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "gpu/config/gpu_finch_features.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

namespace {

std::string GetForceCompositeScript() {
  return R"(
    new Promise(resolve => {
      // Force a visual update to ensure Viz composites a frame.
      const div = document.createElement('div');
      div.style.width = '100px';
      div.style.height = '100px';
      div.style.backgroundColor = 'green';
      document.body.appendChild(div);

      // Wait for Viz to composite the frames.
      requestAnimationFrame(() => {
        requestAnimationFrame(() => {
          resolve(true);
        });
      });
    });
  )";
}

}  // namespace

class InitialGpuChannelBrowserTest : public ContentBrowserTest {
 public:
  InitialGpuChannelBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kSendGPUChannelEarly);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(InitialGpuChannelBrowserTest,
                       StandardRendererUsesEarlyChannel) {
  // Navigate to a page using a standard (non-spare) renderer process.
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  // Verify that the renderer is able to use the early channel to submit frames.
  RenderFrameSubmissionObserver frame_observer(shell()->web_contents());
  frame_observer.SetWaitForNextFrame();

  // Verify that rendering works.
  EXPECT_EQ(true, EvalJs(shell(), GetForceCompositeScript()));

  // Wait for Viz to composite the frames.
  frame_observer.WaitForNextFrameSubmission();

  content::FetchHistogramsFromChildProcesses();

  // Initial channel was requested (multiple renderers may be launched, e.g.
  // spare).
  EXPECT_FALSE(
      histogram_tester
          .GetAllSamples("GPU.EstablishGpuChannel.Browser.InitAsyncLatency")
          .empty());
  histogram_tester.ExpectTotalCount(
      "GPU.EstablishGpuChannel.Browser.RegularAsyncLatency", 0);
  EXPECT_FALSE(
      histogram_tester
          .GetAllSamples(
              "GPU.EstablishGpuChannel.InitialChannelBrowserToRendererLatency")
          .empty());
  EXPECT_FALSE(
      histogram_tester
          .GetAllSamples("GPU.EstablishGpuChannel.InitialChannelLatency")
          .empty());
}

IN_PROC_BROWSER_TEST_F(InitialGpuChannelBrowserTest,
                       SpareRendererUsesEarlyChannel) {
  // Ensure a spare renderer is created.
  auto* browser_context = shell()->web_contents()->GetBrowserContext();
  SpareRenderProcessHostManagerImpl::Get().WarmupSpare(browser_context);

  // Verify that the spare renderer has been created.
  EXPECT_TRUE(SpareRenderProcessHostManagerImpl::Get().HasSpareRenderer());

  // Navigate to a page. This should adopt the spare renderer.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  // Verify that the renderer is able to use the early channel to submit frames.
  RenderFrameSubmissionObserver frame_observer(shell()->web_contents());
  frame_observer.SetWaitForNextFrame();

  // Verify that rendering works.
  EXPECT_EQ(true, EvalJs(shell(), GetForceCompositeScript()));

  // Wait for Viz to composite the frames.
  frame_observer.WaitForNextFrameSubmission();
}

IN_PROC_BROWSER_TEST_F(InitialGpuChannelBrowserTest,
                       GpuChannelCancellationScenarios) {
  // Navigate to a page to ensure renderer process is started.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  // Hide WebContents to prevent renderer from using gpu channel right away.
  shell()->web_contents()->WasHidden();

  auto* rphi = static_cast<RenderProcessHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess());
  auto* gpu_client = rphi->GetGpuClient();

  // Helper to ignore arguments of EstablishGpuChannel callback.
  auto ignore_args = [](base::OnceClosure closure) {
    return base::BindOnce(
        [](base::OnceClosure closure, int32_t client_id,
           mojo::ScopedMessagePipeHandle channel_handle,
           const gpu::GPUInfo& gpu_info,
           const gpu::GpuFeatureInfo& gpu_feature_info,
           const gpu::SharedImageCapabilities& shared_image_capabilities) {
          std::move(closure).Run();
        },
        std::move(closure));
  };

  // Scenario 1: InitializeGpuChannelForNewRenderer cancels
  // InitializeGpuChannelForNewRenderer
  {
    mojo::MessagePipe pipe1;
    gpu_client->InitializeGpuChannelForNewRenderer(std::move(pipe1.handle0));

    mojo::MessagePipe pipe2;
    gpu_client->InitializeGpuChannelForNewRenderer(std::move(pipe2.handle0));

    // The first request should be cancelled. We can verify by checking if we
    // can still establish a channel.
    base::test::TestFuture<void> establish_future;
    gpu_client->EstablishGpuChannel(
        ignore_args(establish_future.GetCallback()));
    EXPECT_TRUE(establish_future.Wait());
  }

  // Scenario 2: Establish cancels InitializeGpuChannelForNewRenderer
  {
    mojo::MessagePipe pipe;
    gpu_client->InitializeGpuChannelForNewRenderer(std::move(pipe.handle0));

    base::test::TestFuture<void> establish_future;
    gpu_client->EstablishGpuChannel(
        ignore_args(establish_future.GetCallback()));
    EXPECT_TRUE(establish_future.Wait());
  }

  // Scenario 3: InitializeGpuChannelForNewRenderer cancels Establish
  {
    base::test::TestFuture<bool> establish_future;
    gpu_client->EstablishGpuChannel(base::BindOnce(
        [](base::OnceCallback<void(bool)> callback, int32_t client_id,
           mojo::ScopedMessagePipeHandle channel_handle,
           const gpu::GPUInfo& gpu_info,
           const gpu::GpuFeatureInfo& gpu_feature_info,
           const gpu::SharedImageCapabilities& shared_image_capabilities) {
          std::move(callback).Run(channel_handle.is_valid());
        },
        establish_future.GetCallback()));

    EXPECT_FALSE(establish_future.IsReady());

    mojo::MessagePipe pipe;
    gpu_client->InitializeGpuChannelForNewRenderer(std::move(pipe.handle0));

    // The pending EstablishGpuChannel request's callback should be called with
    // failure.
    EXPECT_FALSE(establish_future.Get());

    // Wait for the InitializeGpuChannelForNewRenderer request to complete.
    base::test::TestFuture<bool> success_future;
    gpu_client->SetEstablishGpuChannelCallbackForTesting(
        success_future.GetCallback());
    EXPECT_TRUE(success_future.Get());
  }
}

IN_PROC_BROWSER_TEST_F(InitialGpuChannelBrowserTest,
                       EarlyFrameSinkUsed_InitiallyHidden) {
  // Hide the WebContents before navigation.
  shell()->web_contents()->WasHidden();

  // Navigate to a page.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  RenderFrameSubmissionObserver frame_observer(shell()->web_contents());
  frame_observer.SetWaitForNextFrame();

  auto* rwhi = static_cast<RenderWidgetHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());

  // On non-ChromeOS platforms, the frame sink pipes should be created but
  // deferred because we are hidden.
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_FALSE(rwhi->has_initial_frame_sink_for_testing());
#else
  // Note that there might be some racy Show() call before this, which might
  // trigger the sending of the initial frame sink pipe earlier.
  EXPECT_EQ(rwhi->is_hidden_for_testing(),
            rwhi->has_initial_frame_sink_for_testing());
#endif

  // Show the WebContents and focus it.
  shell()->web_contents()->WasShown();
  shell()->web_contents()->Focus();

  // The pipes should have been used and cleared!
  EXPECT_FALSE(rwhi->has_initial_frame_sink_for_testing());

  // Ping the renderer to ensure WasShown() and UpdateVisualProperties() Mojo
  // messages are fully processed, and the early frame sink is bound.
  EXPECT_EQ(true, EvalJs(shell(), "true"));

  // Force a repaint on a new surface. This guarantees a new frame is drawn
  // and submitted to Viz even if there is no other page damage.
  EXPECT_TRUE(rwhi->RequestRepaintOnNewSurface());

  // Wait for a frame to be submitted. This ensures the renderer has processed
  // the WasShown() call and requested the frame sink.
  frame_observer.WaitForNextFrameSubmission();
}

class InitialGpuChannelDisabledBrowserTest : public ContentBrowserTest {
 public:
  InitialGpuChannelDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(features::kSendGPUChannelEarly);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(InitialGpuChannelDisabledBrowserTest,
                       FallsBackToStandardInitialization) {
  // Navigate to a page.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  RenderFrameSubmissionObserver frame_observer(shell()->web_contents());
  frame_observer.SetWaitForNextFrame();

  // Verify rendering still works.
  EXPECT_EQ(true, EvalJs(shell(), GetForceCompositeScript()));

  // Wait for Viz to composite the frames.
  frame_observer.WaitForNextFrameSubmission();
}

}  // namespace content
