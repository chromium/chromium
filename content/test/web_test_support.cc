// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/web_test_support.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "build/build_config.h"
#include "content/browser/bluetooth/bluetooth_device_chooser_controller.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/browser/worker_host/test_shared_worker_service_impl.h"
#include "content/common/renderer.mojom.h"
#include "content/common/unique_name_helper.h"
#include "content/public/browser/storage_partition.h"
#include "content/renderer/compositor/layer_tree_view.h"
#include "content/renderer/input/render_widget_input_handler_delegate.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/web_worker_fetch_context_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/common/web_test/web_test_switches.h"
#include "content/shell/renderer/web_test/blink_test_runner.h"
#include "content/shell/renderer/web_test/web_test_render_thread_observer.h"
#include "content/shell/test_runner/test_common.h"
#include "content/shell/test_runner/web_frame_test_proxy.h"
#include "content/shell/test_runner/web_test_interfaces.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "content/shell/test_runner/web_widget_test_proxy.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/web/web_manifest_manager.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/test/icc_profiles.h"

#if defined(OS_MACOSX)
#include "content/browser/frame_host/popup_menu_helper_mac.h"
#elif defined(OS_WIN)
#include "content/child/font_warmup_win.h"
#include "third_party/blink/public/web/win/web_font_rendering.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"
#endif

using blink::WebRect;
using blink::WebSize;

namespace content {

namespace {

RenderViewImpl* CreateWebViewTestProxy(CompositorDependencies* compositor_deps,
                                       const mojom::CreateViewParams& params) {
  test_runner::WebTestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();

  auto* render_view_proxy =
      new test_runner::WebViewTestProxy(compositor_deps, params);

  auto test_runner = std::make_unique<BlinkTestRunner>(render_view_proxy);
  // TODO(lukasza): Using the 1st BlinkTestRunner as the main delegate is wrong,
  // but it is difficult to change because this behavior has been baked for a
  // long time into test assumptions (i.e. which PrintMessage gets delivered to
  // the browser depends on this).
  static bool first_test_runner = true;
  if (first_test_runner) {
    first_test_runner = false;
    interfaces->SetDelegate(test_runner.get());
  }

  render_view_proxy->Initialize(interfaces, std::move(test_runner));
  return render_view_proxy;
}

std::unique_ptr<RenderWidget> CreateRenderWidgetForFrame(
    int32_t routing_id,
    CompositorDependencies* compositor_deps,
    blink::mojom::DisplayMode display_mode,
    bool swapped_out,
    bool never_visible,
    mojo::PendingReceiver<mojom::Widget> widget_receiver) {
  return std::make_unique<test_runner::WebWidgetTestProxy>(
      routing_id, compositor_deps, display_mode, swapped_out,
      /*hidden=*/true, never_visible, std::move(widget_receiver));
}

RenderFrameImpl* CreateWebFrameTestProxy(RenderFrameImpl::CreateParams params) {
  test_runner::WebTestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();

  // RenderFrameImpl always has a RenderViewImpl for it.
  RenderViewImpl* render_view_impl = params.render_view;

  auto* render_frame_proxy =
      new test_runner::WebFrameTestProxy(std::move(params));
  render_frame_proxy->Initialize(interfaces, render_view_impl);
  return render_frame_proxy;
}

#if defined(OS_WIN)
// DirectWrite only has access to %WINDIR%\Fonts by default. For developer
// side-loading, support kRegisterFontFiles to allow access to additional fonts.
void RegisterSideloadedTypefaces(SkFontMgr* fontmgr) {
  for (const auto& file : switches::GetSideloadFontFiles()) {
    blink::WebFontRendering::AddSideloadedFontForTesting(
        fontmgr->makeFromFile(file.c_str()));
  }
}
#endif  // OS_WIN

}  // namespace

test_runner::WebWidgetTestProxy* GetWebWidgetTestProxy(
    blink::WebLocalFrame* frame) {
  DCHECK(frame);
  RenderFrame* local_root = RenderFrame::FromWebFrame(frame->LocalRoot());
  RenderFrameImpl* local_root_impl = static_cast<RenderFrameImpl*>(local_root);
  DCHECK(local_root);

  return static_cast<test_runner::WebWidgetTestProxy*>(
      local_root_impl->GetLocalRootRenderWidget());
}

void EnableWebTestProxyCreation() {
  RenderViewImpl::InstallCreateHook(CreateWebViewTestProxy);
  RenderWidget::InstallCreateForFrameHook(CreateRenderWidgetForFrame);
  RenderFrameImpl::InstallCreateHook(CreateWebFrameTestProxy);
}

void FetchManifest(blink::WebView* view, FetchManifestCallback callback) {
  blink::WebManifestManager::RequestManifestForTesting(
      RenderFrameImpl::FromWebFrame(view->MainFrame())->GetWebFrame(),
      std::move(callback));
}

void SetWorkerRewriteURLFunction(RewriteURLFunction rewrite_url_function) {
  WebWorkerFetchContextImpl::InstallRewriteURLFunction(rewrite_url_function);
}

void EnableRendererWebTestMode() {
  RenderThreadImpl::current()->enable_web_test_mode();

  UniqueNameHelper::PreserveStableUniqueNameForTesting();

#if defined(OS_WIN)
  RegisterSideloadedTypefaces(SkFontMgr_New_DirectWrite().get());
#endif
}

void EnableBrowserWebTestMode() {
#if defined(OS_MACOSX)
  PopupMenuHelper::DontShowPopupMenuForTesting();
#endif
  RenderWidgetHostImpl::DisableResizeAckCheckForTesting();
}

void InjectTestSharedWorkerService(StoragePartition* storage_partition) {
  auto* storage_partition_impl =
      static_cast<StoragePartitionImpl*>(storage_partition);

  storage_partition_impl->OverrideSharedWorkerServiceForTesting(
      std::make_unique<TestSharedWorkerServiceImpl>(
          storage_partition_impl,
          storage_partition_impl->GetServiceWorkerContext(),
          storage_partition_impl->GetAppCacheService()));
}

void TerminateAllSharedWorkers(StoragePartition* storage_partition,
                               base::OnceClosure callback) {
  static_cast<TestSharedWorkerServiceImpl*>(
      storage_partition->GetSharedWorkerService())
      ->TerminateAllWorkers(std::move(callback));
}

int GetLocalSessionHistoryLength(RenderView* render_view) {
  return static_cast<RenderViewImpl*>(render_view)
      ->GetLocalSessionHistoryLengthForTesting();
}

void SetFocusAndActivate(RenderView* render_view, bool enable) {
  static_cast<RenderViewImpl*>(render_view)
      ->SetFocusAndActivateForTesting(enable);
}

void ForceResizeRenderView(RenderView* render_view, const WebSize& new_size) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  if (!render_view_impl->GetMainRenderFrame())
    return;
  RenderWidget* render_widget = render_view_impl->GetWidget();
  gfx::Rect window_rect(render_widget->WindowRect().x,
                        render_widget->WindowRect().y, new_size.width,
                        new_size.height);
  render_widget->SetWindowRectSynchronouslyForTesting(window_rect);
}

void SetDeviceScaleFactor(RenderView* render_view, float factor) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  if (!render_view_impl->GetMainRenderFrame())
    return;
  render_view_impl->GetWidget()->SetDeviceScaleFactorForTesting(factor);
}

std::unique_ptr<blink::WebInputEvent> TransformScreenToWidgetCoordinates(
    test_runner::WebWidgetTestProxy* web_widget_test_proxy,
    const blink::WebInputEvent& event) {
  DCHECK(web_widget_test_proxy);

  RenderWidget* render_widget =
      static_cast<RenderWidget*>(web_widget_test_proxy);

  // Compute the scale from window (dsf-independent) to blink (dsf-dependent
  // under UseZoomForDSF).
  blink::WebFloatRect rect(0, 0, 1.0f, 0.0);
  render_widget->ConvertWindowToViewport(&rect);
  float scale = rect.width;

  blink::WebRect view_rect = render_widget->ViewRect();
  gfx::Vector2d delta(-view_rect.x, -view_rect.y);

  // The coordinates are given in terms of the root widget, so adjust for the
  // position of the main frame.
  // TODO(sgilhuly): This doesn't work for events sent to OOPIFs because the
  // main frame is remote, and doesn't have a corresponding RenderWidget.
  // Currently none of those tests are run out of headless mode.
  blink::WebFrame* frame =
      web_widget_test_proxy->GetWebViewTestProxy()->webview()->MainFrame();
  if (frame->IsWebLocalFrame()) {
    test_runner::WebWidgetTestProxy* root_widget =
        GetWebWidgetTestProxy(frame->ToWebLocalFrame());
    blink::WebRect root_rect = root_widget->ViewRect();
    gfx::Vector2d root_delta(root_rect.x, root_rect.y);
    delta.Add(root_delta);
  }

  return ui::TranslateAndScaleWebInputEvent(event, delta, scale);
}

gfx::ColorSpace GetTestingColorSpace(const std::string& name) {
  if (name == "genericRGB") {
    return gfx::ICCProfileForTestingGenericRGB().GetColorSpace();
  } else if (name == "sRGB") {
    return gfx::ColorSpace::CreateSRGB();
  } else if (name == "test" || name == "colorSpin") {
    return gfx::ICCProfileForTestingColorSpin().GetColorSpace();
  } else if (name == "adobeRGB") {
    return gfx::ICCProfileForTestingAdobeRGB().GetColorSpace();
  } else if (name == "reset") {
    return display::Display::GetForcedDisplayColorProfile();
  }
  return gfx::ColorSpace();
}

void SetDeviceColorSpace(RenderView* render_view,
                         const gfx::ColorSpace& color_space) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  if (!render_view_impl->GetMainRenderFrame())
    return;
  render_view_impl->GetWidget()->SetDeviceColorSpaceForTesting(color_space);
}

void SetTestBluetoothScanDuration(BluetoothTestScanDurationSetting setting) {
  switch (setting) {
    case BluetoothTestScanDurationSetting::kImmediateTimeout:
      BluetoothDeviceChooserController::SetTestScanDurationForTesting(
          BluetoothDeviceChooserController::TestScanDurationSetting::
              IMMEDIATE_TIMEOUT);
      break;
    case BluetoothTestScanDurationSetting::kNeverTimeout:
      BluetoothDeviceChooserController::SetTestScanDurationForTesting(
          BluetoothDeviceChooserController::TestScanDurationSetting::
              NEVER_TIMEOUT);
      break;
  }
}

void UseSynchronousResizeMode(RenderView* render_view, bool enable) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  if (!render_view_impl->GetMainRenderFrame())
    return;
  render_view_impl->GetWidget()->UseSynchronousResizeModeForTesting(enable);
}

void EnableAutoResizeMode(RenderView* render_view,
                          const WebSize& min_size,
                          const WebSize& max_size) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  if (!render_view_impl->GetMainRenderFrame())
    return;
  render_view_impl->GetWidget()->EnableAutoResizeForTesting(min_size, max_size);
}

void DisableAutoResizeMode(RenderView* render_view, const WebSize& new_size) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  if (!render_view_impl->GetMainRenderFrame())
    return;
  render_view_impl->GetWidget()->DisableAutoResizeForTesting(new_size);
}

void SchedulerRunIdleTasks(base::OnceClosure callback) {
  blink::scheduler::WebThreadScheduler* scheduler =
      content::RenderThreadImpl::current()->GetWebMainThreadScheduler();
  blink::scheduler::RunIdleTasksForTesting(scheduler, std::move(callback));
}

void ForceTextInputStateUpdateForRenderFrame(RenderFrame* frame) {
  RenderWidget* render_widget =
      static_cast<RenderFrameImpl*>(frame)->GetLocalRootRenderWidget();
  render_widget->ShowVirtualKeyboard();
}

}  // namespace content
