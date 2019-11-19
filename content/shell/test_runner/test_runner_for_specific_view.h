// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_TEST_RUNNER_FOR_SPECIFIC_VIEW_H_
#define CONTENT_SHELL_TEST_RUNNER_TEST_RUNNER_FOR_SPECIFIC_VIEW_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/shell/test_runner/test_runner_export.h"
#include "v8/include/v8.h"

class SkBitmap;

namespace blink {
struct Manifest;
class WebLocalFrame;
class WebView;
class WebURL;
}

namespace gfx {
struct PresentationFeedback;
}

namespace gin {
class Arguments;
}

namespace test_runner {
class WebTestDelegate;
class WebWidgetTestProxy;
class WebViewTestProxy;

// TestRunnerForSpecificView implements part of |testRunner| javascript bindings
// that work with a view where the javascript call originated from.  Examples:
// - testRunner.capturePixelsAsyncThen
// - testRunner.setPageVisibility
// Note that "global" bindings are handled by TestRunner class.
class TEST_RUNNER_EXPORT TestRunnerForSpecificView {
 public:
  explicit TestRunnerForSpecificView(WebViewTestProxy* web_view_test_proxy);
  ~TestRunnerForSpecificView();

  // Installs view-specific bindings (handled by |this|) and *also* global
  // TestRunner bindings (both kinds of bindings are exposed via a single
  // |testRunner| object in javascript).
  void Install(blink::WebLocalFrame* frame);

  void Reset();

  // Pointer lock methods used by WebViewTestClient.
  bool RequestPointerLock();
  void RequestPointerUnlock();
  bool isPointerLocked();

 private:
  friend class TestRunnerBindings;

  // Helpers for working with base and V8 callbacks.
  void PostTask(base::OnceClosure callback);
  void PostV8Callback(const v8::Local<v8::Function>& callback);
  void PostV8CallbackWithArgs(v8::UniquePersistent<v8::Function> callback,
                              int argc,
                              v8::Local<v8::Value> argv[]);
  void InvokeV8Callback(const v8::UniquePersistent<v8::Function>& callback);
  void InvokeV8CallbackWithArgs(
      const v8::UniquePersistent<v8::Function>& callback,
      const std::vector<v8::UniquePersistent<v8::Value>>& args);
  base::OnceClosure CreateClosureThatPostsV8Callback(
      const v8::Local<v8::Function>& callback);

  void UpdateAllLifecyclePhasesAndComposite();
  void UpdateAllLifecyclePhasesAndCompositeThen(
      v8::Local<v8::Function> callback);

  // The callback will be called after the next full frame update and raster,
  // with the captured snapshot as the parameters (width, height, snapshot).
  // The snapshot is in uint8_t RGBA format.
  void CapturePixelsAsyncThen(v8::Local<v8::Function> callback);

  void RunJSCallbackAfterCompositorLifecycle(
      v8::UniquePersistent<v8::Function> callback,
      const gfx::PresentationFeedback&);
  void RunJSCallbackWithBitmap(v8::UniquePersistent<v8::Function> callback,
                               const SkBitmap& snapshot);

  // Similar to CapturePixelsAsyncThen(). Copies to the clipboard the image
  // located at a particular point in the WebView (if there is such an image),
  // reads back its pixels, and provides the snapshot to the callback. If there
  // is no image at that point, calls the callback with (0, 0, empty_snapshot).
  void CopyImageAtAndCapturePixelsAsyncThen(
      int x,
      int y,
      const v8::Local<v8::Function> callback);

  void GetManifestThen(v8::Local<v8::Function> callback);
  void GetManifestCallback(v8::UniquePersistent<v8::Function> callback,
                           const blink::WebURL& manifest_url,
                           const blink::Manifest& manifest);

  // Calls |callback| with a DOMString[] representing the events recorded since
  // the last call to this function.
  void GetBluetoothManualChooserEvents(v8::Local<v8::Function> callback);
  void GetBluetoothManualChooserEventsCallback(
      v8::UniquePersistent<v8::Function> callback,
      const std::vector<std::string>& events);

  // Change the bluetooth test data while running a web test.
  void SetBluetoothFakeAdapter(const std::string& adapter_name,
                               v8::Local<v8::Function> callback);

  // If |enable| is true, makes the Bluetooth chooser record its input and wait
  // for instructions from the test program on how to proceed. Otherwise falls
  // back to the browser's default chooser.
  void SetBluetoothManualChooser(bool enable);

  // Calls the BluetoothChooser::EventHandler with the arguments here. Valid
  // event strings are:
  //  * "cancel" - simulates the user canceling the chooser.
  //  * "select" - simulates the user selecting a device whose device ID is in
  //               |argument|.
  void SendBluetoothManualChooserEvent(const std::string& event,
                                       const std::string& argument);

  // Used to set the device scale factor.
  void SetBackingScaleFactor(double value, v8::Local<v8::Function> callback);

  // Enable zoom-for-dsf option.
  // TODO(oshima): Remove this once all platforms migrated.
  void EnableUseZoomForDSF(v8::Local<v8::Function> callback);

  // Change the device color profile while running a web test.
  void SetColorProfile(const std::string& name,
                       v8::Local<v8::Function> callback);

  // Causes the beforeinstallprompt event to be sent to the renderer.
  void DispatchBeforeInstallPromptEvent(
      const std::vector<std::string>& event_platforms,
      v8::Local<v8::Function> callback);
  void DispatchBeforeInstallPromptCallback(
      v8::UniquePersistent<v8::Function> callback,
      bool canceled);

  // Immediately run all pending idle tasks, including all pending
  // requestIdleCallback calls.  Invoke the callback when all
  // idle tasks are complete.
  void RunIdleTasks(v8::Local<v8::Function> callback);

  // Method that controls whether pressing Tab key cycles through page elements
  // or inserts a '\t' char in text area
  void SetTabKeyCyclesThroughElements(bool tab_key_cycles_through_elements);

  // Executes an internal command (superset of document.execCommand() commands).
  void ExecCommand(gin::Arguments* args);

  // Checks if an internal command is currently available.
  bool IsCommandEnabled(const std::string& command);

  // Returns true if the current page box has custom page size style for
  // printing.
  bool HasCustomPageSizeStyle(int page_index);

  // Forces the selection colors for testing under Linux.
  void ForceRedSelectionColors();

  // Switch the visibility of the page.
  void SetPageVisibility(const std::string& new_visibility);

  // Changes the direction of the focused element.
  void SetTextDirection(const std::string& direction_name);

  // Permits the adding and removing of only one opaque overlay.
  void AddWebPageOverlay();
  void RemoveWebPageOverlay();

  void SetHighlightAds(bool);

  // Sets a flag causing the next call to WebGLRenderingContext::create to fail.
  void ForceNextWebGLContextCreationToFail();

  // Sets a flag causing the next call to DrawingBuffer::create to fail.
  void ForceNextDrawingBufferCreationToFail();

  // Gives focus to the view associated with TestRunnerForSpecificView.
  void SetWindowIsKey(bool value);

  // Pointer lock handling.
  void DidAcquirePointerLock();
  void DidNotAcquirePointerLock();
  void DidLosePointerLock();
  void SetPointerLockWillFailSynchronously();
  void SetPointerLockWillRespondAsynchronously();
  void DidAcquirePointerLockInternal();
  void DidNotAcquirePointerLockInternal();
  void DidLosePointerLockInternal();
  bool pointer_locked_;
  enum {
    PointerLockWillSucceed,
    PointerLockWillRespondAsync,
    PointerLockWillFailSync,
  } pointer_lock_planned_result_;

  void SetDomainRelaxationForbiddenForURLScheme(bool forbidden,
                                                const std::string& scheme);
  v8::Local<v8::Value> EvaluateScriptInIsolatedWorldAndReturnValue(
      int32_t world_id,
      const std::string& script);
  void EvaluateScriptInIsolatedWorld(int32_t world_id,
                                     const std::string& script);
  void SetIsolatedWorldInfo(int32_t world_id,
                            v8::Local<v8::Value> security_origin,
                            v8::Local<v8::Value> content_security_policy);
  bool FindString(const std::string& search_text,
                  const std::vector<std::string>& options_array);
  std::string SelectionAsMarkup();
  void SetViewSourceForFrame(const std::string& name, bool enabled);

  // Many parts of the web test harness assume that the main frame is local.
  // Having all of them go through the helper below makes it easier to catch
  // scenarios that require breaking this assumption.
  blink::WebLocalFrame* GetLocalMainFrame();

  // Helpers for accessing pointers exposed by |web_view_test_proxy_|.
  WebWidgetTestProxy* main_frame_render_widget();
  blink::WebView* web_view();
  WebTestDelegate* delegate();

  WebViewTestProxy* web_view_test_proxy_;

  base::WeakPtrFactory<TestRunnerForSpecificView> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TestRunnerForSpecificView);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_TEST_RUNNER_FOR_SPECIFIC_VIEW_H_
