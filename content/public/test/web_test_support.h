// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_WEB_TEST_SUPPORT_H_
#define CONTENT_PUBLIC_TEST_WEB_TEST_SUPPORT_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"

namespace blink {
struct Manifest;
class WebInputEvent;
class WebLocalFrame;
struct WebSize;
class WebURL;
class WebView;
}  // namespace blink

namespace gfx {
class ColorSpace;
}

namespace test_runner {
class WebFrameTestProxy;
class WebViewTestProxy;
class WebWidgetTestProxy;
}  // namespace test_runner

namespace content {

class RenderFrame;
class RenderView;
class StoragePartition;

// Turn the browser process into web test mode.
void EnableBrowserWebTestMode();

// Replaces the SharedWorkerService implementation with a test-specific one that
// tracks running shared workers.
void InjectTestSharedWorkerService(StoragePartition* storage_partition);

// Terminates all workers and notifies when complete. This is used for
// testing when it is important to make sure that all shared worker activity
// has stopped. Can only be used if InjectTestSharedWorkerService() was called.
void TerminateAllSharedWorkers(StoragePartition* storage_partition,
                               base::OnceClosure callback);

///////////////////////////////////////////////////////////////////////////////
// The following methods are meant to be used from a renderer.

// Turn a renderer into web test mode.
void EnableRendererWebTestMode();

// Gets WebWidgetTestProxy associated with |frame| (either the view's widget
// or the local root's frame widget).  Caller has to ensure that prior to
// construction of |render_frame|, EnableWebTestProxyCreation was called.
test_runner::WebWidgetTestProxy* GetWebWidgetTestProxy(
    blink::WebLocalFrame* frame);

// Enable injecting of a WebViewTestProxy between WebViews and RenderViews,
// WebWidgetTestProxy between WebWidgets and RenderWidgets and WebFrameTestProxy
// between WebFrames and RenderFrames.
void EnableWebTestProxyCreation();

typedef base::OnceCallback<void(const blink::WebURL&, const blink::Manifest&)>
    FetchManifestCallback;
void FetchManifest(blink::WebView* view, FetchManifestCallback callback);

// Returns the length of the local session history of a render view.
int GetLocalSessionHistoryLength(RenderView* render_view);

// Sets the focus of the render view depending on |enable|. This only overrides
// the state of the renderer, and does not sync the focus to the browser
// process.
void SetFocusAndActivate(RenderView* render_view, bool enable);

// Changes the window rect of the given render view.
void ForceResizeRenderView(RenderView* render_view,
                           const blink::WebSize& new_size);

// Set the device scale factor and force the compositor to resize.
void SetDeviceScaleFactor(RenderView* render_view, float factor);

// Converts |event| from screen coordinates to coordinates used by the widget
// associated with the |web_widget_test_proxy|.  Returns nullptr if no
// transformation was necessary (e.g. for a keyboard event OR if widget requires
// no scaling and has coordinates starting at (0,0)).
std::unique_ptr<blink::WebInputEvent> TransformScreenToWidgetCoordinates(
    test_runner::WebWidgetTestProxy* web_widget_test_proxy,
    const blink::WebInputEvent& event);

// Get the color space for a given name string. This is not in the ColorSpace
// class to avoid bloating the shipping build.
gfx::ColorSpace GetTestingColorSpace(const std::string& name);

// Set the device color space.
void SetDeviceColorSpace(RenderView* render_view,
                         const gfx::ColorSpace& color_space);

// Sets the scan duration to reflect the given setting.
enum class BluetoothTestScanDurationSetting {
  kImmediateTimeout,  // Set the scan duration to 0 seconds.
  kNeverTimeout,  // Set the scan duration to base::TimeDelta::Max() seconds.
};
void SetTestBluetoothScanDuration(BluetoothTestScanDurationSetting setting);

// Enables or disables synchronous resize mode. When enabled, all window-sizing
// machinery is short-circuited inside the renderer. This mode is necessary for
// some tests that were written before browsers had multi-process architecture
// and rely on window resizes to happen synchronously.
// See http://crbug.com/309760 for details.
void UseSynchronousResizeMode(RenderView* render_view, bool enable);

// Control auto resize mode.
void EnableAutoResizeMode(RenderView* render_view,
                          const blink::WebSize& min_size,
                          const blink::WebSize& max_size);
void DisableAutoResizeMode(RenderView* render_view,
                           const blink::WebSize& new_size);

// Run all pending idle tasks immediately, and then invoke callback.
void SchedulerRunIdleTasks(base::OnceClosure callback);

// Causes the RenderWidget corresponding to |render_frame| to update its
// TextInputState.
void ForceTextInputStateUpdateForRenderFrame(RenderFrame* render_frame);

// RewriteURLFunction must be safe to call from any thread in the renderer
// process.
using RewriteURLFunction = blink::WebURL (*)(const std::string&,
                                             bool is_wpt_mode);
void SetWorkerRewriteURLFunction(RewriteURLFunction rewrite_url_function);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_WEB_TEST_SUPPORT_H_
