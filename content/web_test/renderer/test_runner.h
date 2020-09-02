// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_TEST_RUNNER_H_
#define CONTENT_WEB_TEST_RENDERER_TEST_RUNNER_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "content/web_test/common/web_test.mojom.h"
#include "content/web_test/common/web_test_bluetooth_fake_adapter_setter.mojom.h"
#include "content/web_test/renderer/fake_screen_orientation_impl.h"
#include "content/web_test/renderer/gamepad_controller.h"
#include "content/web_test/renderer/layout_dump.h"
#include "content/web_test/renderer/web_test_content_settings_client.h"
#include "content/web_test/renderer/web_test_runtime_flags.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "v8/include/v8.h"

class SkBitmap;

namespace base {
class DictionaryValue;
}

namespace blink {
class WebContentSettingsClient;
class WebFrame;
class WebString;
class WebView;
}  // namespace blink

namespace gin {
class ArrayBufferView;
class Arguments;
}  // namespace gin

namespace content {
class RenderFrame;
class RenderView;
class SpellCheckClient;
class TestRunnerBindings;
class WebFrameTestProxy;
class WebWidgetTestProxy;
class WebViewTestProxy;
struct TestPreferences;

// TestRunner class currently has dual purpose:
// 1. It implements TestRunner javascript bindings for "global" / "ambient".
//    Examples:
//    - TestRunner.DumpAsText (test flag affecting test behavior)
//    - TestRunner.SetAllowRunningOfInsecureContent (test flag affecting product
//      behavior)
//    - TestRunner.SetTextSubpixelPositioning (directly interacts with product).
// 2. It manages global test state.  Example:
//    - Tracking topLoadingFrame that can finish the test when it loads.
//    - WorkQueue holding load requests from web tests.
//    - WebTestRuntimeFlags
class TestRunner {
 public:
  TestRunner();
  virtual ~TestRunner();

  void Install(WebFrameTestProxy* frame, SpellCheckClient* spell_check);

  // Resets global TestRunner state for the next test.
  void Reset();

  // Resets state on the |web_view_test_proxy| for the next test.
  void ResetWebView(WebViewTestProxy* web_view_test_proxy);
  // Resets state on the |web_widget_test_proxy| for the next test.
  void ResetWebWidget(WebWidgetTestProxy* web_widget_test_proxy);

  void SetTestIsRunning(bool);
  bool TestIsRunning() const { return test_is_running_; }

  // Finishes the test if it is ready. This should be called before running
  // tasks that will change state, so that the test can capture the current
  // state. Specifically, should run before the BeginMainFrame step which does
  // layout and animation etc.
  // This does *not* run as part of loading finishing because that happens in
  // the middle of blink call stacks that have inconsistent state.
  void FinishTestIfReady();
  // Notification that another renderer has explicitly asked the test to end.
  void TestFinishedFromSecondaryRenderer();

  // Performs a reset at the end of a test, in order to prepare for the next
  // test. This includes a navigation to about:blank, which we hear about
  // through DidCommitNavigationInMainFrame().
  void ResetRendererAfterWebTest(base::OnceClosure done_callback);
  // Listener for navigations in order to hear about the navigation to
  // about:blank done for ResetRendererAfterWebTest().
  void DidCommitNavigationInMainFrame(WebFrameTestProxy* main_frame);

  // Track the set of all main frames in the process, which is also the set of
  // windows rooted in this process.
  void AddMainFrame(WebFrameTestProxy* frame);
  void RemoveMainFrame(WebFrameTestProxy* frame);

  // Track the set of all RenderViews in the process, which includes cross-site
  // frames/windows accessible from this process but homed in a different
  // renderer and parts of any windows' frame trees that share the same site.
  void AddRenderView(WebViewTestProxy* view);
  void RemoveRenderView(WebViewTestProxy* view);

  // Returns a mock WebContentSettings that is used for web tests. An
  // embedder should use this for all WebViews it creates.
  blink::WebContentSettingsClient* GetWebContentSettings();

  // Returns true if the test output should be an audio file, rather than text
  // or pixel results.
  bool ShouldDumpAsAudio() const;
  // Gets the audio test output for when audio test results are requested by
  // the current test.
  const std::vector<uint8_t>& GetAudioData() const;

  // Reports if tests requested a recursive layout dump of all frames
  // (i.e. by calling testRunner.dumpChildFramesAsText() from javascript).
  bool IsRecursiveLayoutDumpRequested();

  // Returns true if the selection window should be painted onto captured
  // pixels.
  bool ShouldDumpSelectionRect() const;

  // Returns false if the browser should capture the pixel output, true if it
  // can be done locally in the renderer via DumpPixelsInRenderer().
  bool CanDumpPixelsFromRenderer() const;

  // Snapshots the content of |render_view| using the mode requested by the
  // current test and calls |callback| with the result.  Caller needs to ensure
  // that |render_view| stays alive until |callback| is called.
  SkBitmap DumpPixelsInRenderer(content::RenderView* render_view);

  // Replicates changes to web test runtime flags (i.e. changes that happened in
  // another renderer). See also `OnWebTestRuntimeFlagsChanged()`.
  void ReplicateWebTestRuntimeFlagsChanges(
      const base::DictionaryValue& changed_values);

  // If custom text dump is present (i.e. if testRunner.setCustomTextOutput has
  // been called from javascript), then returns |true| and populates the
  // |custom_text_dump| argument.  Otherwise returns |false|.
  bool HasCustomTextDump(std::string* custom_text_dump) const;

  // Returns true if the history should be included in text results generated at
  // the end of the test.
  bool ShouldDumpBackForwardList() const;

  // Returns true if pixel results should be generated at the end of the test.
  bool ShouldGeneratePixelResults();

  TextResultType ShouldGenerateTextResults();

  // Activate the window holding the given main frame, and set focus on the
  // frame's widget.
  void FocusWindow(RenderFrame* main_frame, bool focus);

  // Methods used by WebViewTestClient and WebFrameTestClient.
  std::string GetAcceptLanguages() const;
  bool ShouldStayOnPageAfterHandlingBeforeUnload() const;
  bool ShouldDumpAsCustomText() const;
  std::string CustomDumpText() const;
  void ShowDevTools(const std::string& settings,
                    const std::string& frontend_url);
  void SetShouldDumpAsLayout(bool);
  void SetCustomTextOutput(const std::string& text);
  void SetShouldGeneratePixelResults(bool);
  void SetShouldDumpFrameLoadCallbacks(bool);
  bool ShouldDumpEditingCallbacks() const;
  bool ShouldDumpFrameLoadCallbacks() const;
  bool ShouldDumpPingLoaderCallbacks() const;
  bool ShouldDumpUserGestureInFrameLoadCallbacks() const;
  bool ShouldDumpTitleChanges() const;
  bool ShouldDumpIconChanges() const;
  bool ShouldDumpCreateView() const;
  bool CanOpenWindows() const;
  bool ShouldWaitUntilExternalURLLoad() const;
  const std::set<std::string>* HttpHeadersToClear() const;
  bool ClearReferrer() const;
  bool IsWebPlatformTestsMode() const;
  void SetIsWebPlatformTestsMode();
  bool animation_requires_raster() const { return animation_requires_raster_; }
  void SetAnimationRequiresRaster(bool do_raster);

  // Add |frame| to the set of loading frames.
  //
  // Note: Only one renderer process is really tracking the loading frames. This
  //       is the first to observe one. Both local and remote frames are tracked
  //       by this process.
  void AddLoadingFrame(blink::WebFrame* frame);

  // Remove |frame| from the set of loading frames.
  //
  // When there are no more loading frames, this potentially finishes the test,
  // unless TestRunner.WaitUntilDone() was called and/or there are pending load
  // requests in WorkQueue.
  void RemoveLoadingFrame(blink::WebFrame* frame);

  void PolicyDelegateDone();
  bool PolicyDelegateEnabled() const;
  bool PolicyDelegateIsPermissive() const;
  bool PolicyDelegateShouldNotifyDone() const;
  void SetToolTipText(const blink::WebString&);
  void SetDragImage(const SkBitmap& drag_image);
  bool ShouldDumpNavigationPolicy() const;

  bool ShouldDumpConsoleMessages() const;
  // Controls whether console messages produced by the page are dumped
  // to test output.
  void SetDumpConsoleMessages(bool value);

  // The following trigger navigations on the main WebView.
  void GoToOffset(int offset);
  void Reload();
  void LoadURLForFrame(const GURL& url, const std::string& frame_name);

  // Add a message to the text dump for the web test.
  void PrintMessage(const std::string& message);
  // Add a message to stderr (not saved to expected output files, for debugging
  // only).
  void PrintMessageToStderr(const std::string& message);

  // Register a new isolated filesystem with the given files, and return the
  // new filesystem id.
  blink::WebString RegisterIsolatedFileSystem(
      const std::vector<base::FilePath>& file_paths);

  blink::WebEffectiveConnectionType effective_connection_type() const {
    return effective_connection_type_;
  }

  // A single item in the work queue.
  class WorkItem {
   public:
    virtual ~WorkItem() {}

    // Returns true if this started a load.
    virtual bool Run(TestRunner*) = 0;
  };

 private:
  friend class TestRunnerBindings;
  friend class WorkQueue;

  // Helper class for managing events queued by methods like QueueLoad or
  // QueueScript.
  class WorkQueue {
   public:
    explicit WorkQueue(TestRunner* controller);
    virtual ~WorkQueue();
    void ProcessWorkSoon();

    // Reset the state of the class between tests.
    void Reset();

    void AddWork(WorkItem*);

    void set_frozen(bool frozen) { frozen_ = frozen; }
    bool is_empty() const { return queue_.empty(); }

    void set_finished_loading() { finished_loading_ = true; }

   private:
    void ProcessWork();

    base::circular_deque<WorkItem*> queue_;
    bool frozen_ = false;
    bool finished_loading_ = false;
    TestRunner* controller_;

    base::WeakPtrFactory<WorkQueue> weak_factory_{this};
  };

  // If the main test window's main frame is hosted in this renderer process,
  // then this will return it. Otherwise, it is in another process and this
  // returns null.
  WebFrameTestProxy* FindInProcessMainWindowMainFrame();

  ///////////////////////////////////////////////////////////////////////////
  // Methods dealing with the test logic

  // By default, tests end when page load is complete. These methods are used
  // to delay the completion of the test until NotifyDone is called.
  void WaitUntilDone();
  void NotifyDone();

  // When there are no conditions left to wait for, this is called to cause the
  // test to end, collect results, and inform the browser.
  void FinishTest();

  // Methods for adding actions to the work queue. Used in conjunction with
  // WaitUntilDone/NotifyDone above.
  void QueueBackNavigation(int how_far_back);
  void QueueForwardNavigation(int how_far_forward);
  void QueueReload();
  void QueueLoadingScript(const std::string& script,
                          base::WeakPtr<TestRunnerBindings> bindings);
  void QueueNonLoadingScript(const std::string& script,
                             base::WeakPtr<TestRunnerBindings> bindings);
  void QueueLoad(const GURL& current_url,
                 const std::string& relative_url,
                 const std::string& target);

  // Called from the TestRunnerBindings to inform that the test has modified
  // the TestPreferences. This will update the WebkitPreferences in the renderer
  // and the browser.
  void OnTestPreferencesChanged(const TestPreferences& test_prefs,
                                RenderFrame* frame);

  // Causes navigation actions just printout the intended navigation instead
  // of taking you to the page. This is used for cases like mailto, where you
  // don't actually want to open the mail program.
  void SetCustomPolicyDelegate(gin::Arguments* args);

  // Delays completion of the test until the policy delegate runs.
  void WaitForPolicyDelegate();

  // This is the count of windows which have their main frame in this renderer
  // process. A cross-origin window would not appear in this count.
  int InProcessWindowCount();

  // Allows web tests to manage origins' allow list.
  void AddOriginAccessAllowListEntry(const std::string& source_origin,
                                     const std::string& destination_protocol,
                                     const std::string& destination_host,
                                     bool allow_destination_subdomains);

  // Enables or disables subpixel positioning (i.e. fractional X positions for
  // glyphs) in text rendering on Linux. Since this method changes global
  // settings, tests that call it must use their own custom font family for
  // all text that they render. If not, an already-cached style will be used,
  // resulting in the changed setting being ignored.
  void SetTextSubpixelPositioning(bool value);

  // After this function is called, all window-sizing machinery is
  // short-circuited inside the renderer. This mode is necessary for
  // some tests that were written before browsers had multi-process architecture
  // and rely on window resizes to happen synchronously.
  // The function has "unfortunate" it its name because we must strive to remove
  // all tests that rely on this... well, unfortunate behavior. See
  // http://crbug.com/309760 for the plan.
  void UseUnfortunateSynchronousResizeMode();

  // Set the mock orientation on |view| to |orientation|.
  void SetMockScreenOrientation(WebViewTestProxy* view,
                                const std::string& orientation);
  // Disable any mock orientation on |view| that is set.
  void DisableMockScreenOrientation(WebViewTestProxy* view);

  // Modify accept_languages in blink::mojom::RendererPreferences.
  void SetAcceptLanguages(const std::string& accept_languages);

  ///////////////////////////////////////////////////////////////////////////
  // Methods that modify the state of TestRunner

  // This function sets a flag that tells the test runner to print a line of
  // descriptive text for each editing command. It takes no arguments, and
  // ignores any that may be present.
  void DumpEditingCallbacks();

  // This function sets a flag that tells the test runner to dump pages as
  // plain text. The pixel results will not be generated for this test.
  // It has higher priority than DumpAsMarkup() and DumpAsLayout().
  void DumpAsText();

  // This function sets a flag that tells the test runner to dump pages as
  // the DOM contents, rather than as a text representation of the renderer's
  // state. The pixel results will not be generated for this test. It has
  // higher priority than DumpAsLayout(), but lower than DumpAsText().
  void DumpAsMarkup();

  // This function sets a flag that tells the test runner to dump pages as
  // plain text. It will also generate a pixel dump for the test.
  void DumpAsTextWithPixelResults();

  // This function sets a flag that tells the test runner to dump pages as
  // text representation of the layout. The pixel results will not be generated
  // for this test. It has lower priority than DumpAsText() and DumpAsMarkup().
  void DumpAsLayout();

  // This function sets a flag that tells the test runner to dump pages as
  // text representation of the layout. It will also generate a pixel dump for
  // the test.
  void DumpAsLayoutWithPixelResults();

  // This function sets a flag that tells the test runner to recursively dump
  // all frames as text, markup or layout depending on which of DumpAsText,
  // DumpAsMarkup and DumpAsLayout is effective.
  void DumpChildFrames();

  // This function sets a flag that tells the test runner to print out the
  // information about icon changes notifications from WebKit.
  void DumpIconChanges();

  // Deals with Web Audio WAV file data.
  void SetAudioData(const gin::ArrayBufferView& view);

  // This function sets a flag that tells the test runner to print a line of
  // descriptive text for each frame load callback. It takes no arguments, and
  // ignores any that may be present.
  void DumpFrameLoadCallbacks();

  // This function sets a flag that tells the test runner to print a line of
  // descriptive text for each PingLoader dispatch. It takes no arguments, and
  // ignores any that may be present.
  void DumpPingLoaderCallbacks();

  // This function sets a flag that tells the test runner to print a line of
  // user gesture status text for some frame load callbacks. It takes no
  // arguments, and ignores any that may be present.
  void DumpUserGestureInFrameLoadCallbacks();

  void DumpTitleChanges();

  // This function sets a flag that tells the test runner to dump all calls to
  // WebViewClient::createView().
  // It takes no arguments, and ignores any that may be present.
  void DumpCreateView();

  void SetCanOpenWindows();

  // This function sets a flag that tells the test runner to dump the MIME type
  // for each resource that was loaded. It takes no arguments, and ignores any
  // that may be present.
  void DumpResourceResponseMIMETypes();

  // WebContentSettingsClient related.
  void SetImagesAllowed(bool allowed);
  void SetScriptsAllowed(bool allowed);
  void SetStorageAllowed(bool allowed);
  void SetAllowRunningOfInsecureContent(bool allowed);
  void DumpPermissionClientCallbacks();

  // This function sets a flag that tells the test runner to print out a text
  // representation of the back/forward list. It ignores all arguments.
  void DumpBackForwardList();

  void DumpSelectionRect();

  // Causes layout to happen as if targetted to printed pages.
  void SetPrinting();
  void SetPrintingForFrame(const std::string& frame_name);

  void SetShouldStayOnPageAfterHandlingBeforeUnload(bool value);

  // Causes WillSendRequest to clear certain headers.
  // Note: This cannot be used to clear the request's `Referer` header, as this
  // header is computed later given its referrer string member. To clear it, use
  // SetWillSendRequestClearReferrer() below.
  void SetWillSendRequestClearHeader(const std::string& header);

  // Causes WillSendRequest to clear the request's referrer string and set its
  // referrer policy to the default.
  void SetWillSendRequestClearReferrer();

  // Sets a flag that causes the test to be marked as completed when the
  // WebLocalFrameClient receives a LoadURLExternally() call.
  void WaitUntilExternalURLLoad();

  // This function sets a flag to dump the drag image when the next drag&drop is
  // initiated. It is equivalent to DumpAsTextWithPixelResults but the pixel
  // results will be the drag image instead of a snapshot of the page.
  void DumpDragImage();

  // Sets a flag that tells the WebViewTestProxy to dump the default navigation
  // policy passed to the DecidePolicyForNavigation callback.
  void DumpNavigationPolicy();

  // Controls whether JavaScript dialogs such as alert() are dumped to test
  // output.
  void SetDumpJavaScriptDialogs(bool value);

  // Overrides the NetworkQualityEstimator's estimated network type. If |type|
  // is TypeUnknown the NQE's value is used. Be sure to call this with
  // TypeUnknown at the end of your test if you use this.
  void SetEffectiveConnectionType(
      blink::WebEffectiveConnectionType connection_type);

  // Takes care of notifying the browser after a change to web test runtime
  // flags.
  void OnWebTestRuntimeFlagsChanged();

  ///////////////////////////////////////////////////////////////////////////
  // Internal helpers

  mojo::AssociatedRemote<mojom::WebTestControlHost>&
  GetWebTestControlHostRemote();
  void HandleWebTestControlHostDisconnected();
  mojo::AssociatedRemote<mojom::WebTestControlHost>
      web_test_control_host_remote_;

  mojom::WebTestBluetoothFakeAdapterSetter& GetBluetoothFakeAdapterSetter();
  void HandleBluetoothFakeAdapterSetterDisconnected();
  mojo::Remote<mojom::WebTestBluetoothFakeAdapterSetter>
      bluetooth_fake_adapter_setter_;

  bool test_is_running_ = false;

  WorkQueue work_queue_;

  // Bound variable to return the name of this platform (chromium).
  std::string platform_name_;

  // Flags controlling what content gets dumped as a layout text result.
  WebTestRuntimeFlags web_test_runtime_flags_;

  // If true, the test runner will output a base64 encoded WAVE file.
  bool dump_as_audio_;

  // If true, the test runner will produce a dump of the back forward list as
  // well.
  bool dump_back_forward_list_;

  // If true, pixel dump will be produced as a series of 1px-tall, view-wide
  // individual paints over the height of the view.
  bool test_repaint_;

  // If true and test_repaint_ is true as well, pixel dump will be produced as
  // a series of 1px-wide, view-tall paints across the width of the view.
  bool sweep_horizontally_;

  std::set<std::string> http_headers_to_clear_;
  bool clear_referrer_ = false;

  // WAV audio data is stored here.
  std::vector<uint8_t> audio_data_;

  base::flat_set<WebFrameTestProxy*> main_frames_;
  // The set of all render views in this renderer process. This may include
  // cross-site windows accessible from this process, or parts of same-site
  // windows opened from any renderer process.
  base::flat_set<WebViewTestProxy*> render_views_;

  // This is non empty when a load is in progress.
  std::vector<blink::WebFrame*> loading_frames_;
  // We do not want the test to end until the main frame finishes loading. This
  // starts as true at the beginning of the test, and will be set to false once
  // we run out of frames to load at any time.
  bool main_frame_loaded_ = false;
  // When a loading task is started, this bool is set until all loading_frames_
  // are completed and removed. This bool becomes true earlier than
  // loading_frames_ becomes non-empty.
  bool frame_will_start_load_ = true;
  // When NotifyDone() occurs, if loading is still working, it is delayed, and
  // this bool tracks that NotifyDone() was called. This differentiates from a
  // test that was not waiting for NotifyDone() at all.
  bool did_notify_done_ = false;

  WebTestContentSettingsClient test_content_settings_client_;
  FakeScreenOrientationImpl fake_screen_orientation_impl_;
  GamepadController gamepad_controller_;

  // Captured drag image.
  SkBitmap drag_image_;

  // True if rasterization should be performed during tests that examine
  // fling-style animations. This includes middle-click auto-scroll behaviors.
  // This does not include most "ordinary" animations, such as CSS animations.
  bool animation_requires_raster_ = false;

  // An effective connection type settable by web tests.
  blink::WebEffectiveConnectionType effective_connection_type_ =
      blink::WebEffectiveConnectionType::kTypeUnknown;

  // Set to ack callback when the browser asks the renderer to reset at the end
  // of a test. Part of reset involves performing a navigation to about:blank
  // and this tracks that the navigation is in progress, and is called to inform
  // the browser that the reset is complete.
  base::OnceClosure waiting_for_reset_navigation_to_about_blank_;

  base::WeakPtrFactory<TestRunner> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TestRunner);
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_TEST_RUNNER_H_
