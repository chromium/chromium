// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_TEST_RUNNER_H_
#define CONTENT_WEB_TEST_RENDERER_TEST_RUNNER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/web_test/common/web_test.mojom.h"
#include "content/web_test/common/web_test_bluetooth_fake_adapter_setter.mojom.h"
#include "content/web_test/common/web_test_constants.h"
#include "content/web_test/common/web_test_runtime_flags.h"
#include "content/web_test/renderer/fake_screen_orientation_impl.h"
#include "content/web_test/renderer/gamepad_controller.h"
#include "content/web_test/renderer/layout_dump.h"
#include "content/web_test/renderer/web_test_content_settings_client.h"
#include "printing/buildflags/buildflags.h"
#include "printing/page_range.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "v8/include/v8.h"

class SkBitmap;

namespace blink {
class WebContentSettingsClient;
class WebFrame;
class WebFrameWidget;
class WebString;
class WebView;
}  // namespace blink

namespace gin {
class ArrayBufferView;
class Arguments;
}  // namespace gin

namespace content {
class RenderFrame;
class SpellCheckClient;
class TestRunnerBindings;
class WebFrameTestProxy;
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

  TestRunner(const TestRunner&) = delete;
  TestRunner& operator=(const TestRunner&) = delete;

  virtual ~TestRunner();

  void Install(WebFrameTestProxy* frame, SpellCheckClient* spell_check);

  // Resets global TestRunner state for the next test.
  void Reset();

  // Resets state on the |web_view| for the next test.
  void ResetWebView(blink::WebView* web_view);
  // Resets state on the |web_frame_widget| for the next test.
  void ResetWebFrameWidget(blink::WebFrameWidget* web_frame_widget);

  void SetTestIsRunning(bool);
  bool TestIsRunning() const { return test_is_running_; }

  // Finishes the test if it is ready. This should be called before running
  // tasks that will change state, so that the test can capture the current
  // state. Specifically, should run before the BeginMainFrame step which does
  // layout and animation etc.
  // This does *not* run as part of loading finishing because that happens in
  // the middle of blink call stacks that have inconsistent state.
  void FinishTestIfReady(blink::WebLocalFrame& source);
  void FinishTestIfReady(WebFrameTestProxy& source);
  // Notification that another renderer has explicitly asked the test to end.
  void TestFinishedFromSecondaryRenderer(WebFrameTestProxy& source);

  // Track the set of all main frames in the process, which is also the set of
  // windows rooted in this process.
  void AddMainFrame(WebFrameTestProxy& frame);
  void RemoveMainFrame(WebFrameTestProxy& frame);

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

  bool IsPrinting() const;

#if BUILDFLAG(ENABLE_PRINTING)
  // Returns the default page size to be used for printing. This is either the
  // size that was explicitly set via SetPrintingSize or the size of the frame
  // if no size was set.
  gfx::Size GetPrintingPageSize(blink::WebLocalFrame* frame) const;

  // Returns the default page margin size to be used for printing. The value
  // applies to all four sides of the page.
  int GetPrintingMargin() const;

  // Returns the page ranges to be printed. This is specified in the document
  // via a tag of the form <meta name=reftest-pages content="1,2-3,5-">. If no
  // tag is found, print all pages.
  printing::PageRanges GetPrintingPageRanges(blink::WebLocalFrame* frame) const;

  // Go through a test-only path to dump the frame's pixel output as if it was
  // printed.
  SkBitmap PrintFrameToBitmap(blink::WebLocalFrame* frame);
#endif

  // Snapshots the content of |main_frame| using the mode requested by the
  // current test.
  SkBitmap DumpPixelsInRenderer(blink::WebLocalFrame* main_frame);

  // Replicates changes to web test runtime flags (i.e. changes that happened in
  // another renderer). See also `OnWebTestRuntimeFlagsChanged()`.
  void ReplicateWebTestRuntimeFlagsChanges(
      const base::Value::Dict& changed_values);

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
  bool ShouldStayOnPageAfterHandlingBeforeUnload() const;
  bool ShouldDumpAsCustomText() const;
  std::string CustomDumpText() const;
  void ShowDevTools(const std::string& settings,
                    const std::string& frontend_url);
  void SetShouldDumpAsLayout(bool, WebFrameTestProxy& source);
  void SetCustomTextOutput(const std::string& text, WebFrameTestProxy& source);
  void SetShouldGeneratePixelResults(bool, WebFrameTestProxy& source);
  void SetShouldDumpFrameLoadCallbacks(bool, WebFrameTestProxy& source);
  bool ShouldDumpEditingCallbacks() const;
  bool ShouldDumpFrameLoadCallbacks() const;
  bool ShouldDumpPingLoaderCallbacks() const;
  bool ShouldDumpUserGestureInFrameLoadCallbacks() const;
  bool ShouldDumpTitleChanges() const;
  bool ShouldDumpIconChanges() const;
  bool CanOpenWindows() const;
  bool ShouldWaitUntilExternalURLLoad() const;
  const std::set<std::string>* HttpHeadersToClear() const;
  bool ClearReferrer() const;
  bool IsWebPlatformTestsMode() const;
  void SetIsWebPlatformTestsMode(WebFrameTestProxy& source);
  bool animation_requires_raster() const { return animation_requires_raster_; }
  void SetAnimationRequiresRaster(bool do_raster);

  // Add |frame| to the set of loading frames.
  //
  // Note: Only one renderer process is really tracking the loading frames. This
  //       is the first to observe one. Both local and remote frames are tracked
  //       by this process.
  void AddLoadingFrame(blink::WebLocalFrame* frame);

  // Remove |frame| from the set of loading frames.
  //
  // When there are no more loading frames, this potentially finishes the test,
  // unless TestRunner.WaitUntilDone() was called and/or there are pending load
  // requests in WorkQueue.
  void RemoveLoadingFrame(blink::WebLocalFrame* frame);

  // Called when a main frame has been navigated away.
  void OnFrameDeactivated(WebFrameTestProxy& frame);

  // Called when a main frame has been restored from backward/forward cache.
  void OnFrameReactivated(WebFrameTestProxy& frame);

  void PolicyDelegateDone(WebFrameTestProxy& source);
  bool PolicyDelegateEnabled() const;
  bool PolicyDelegateIsPermissive() const;
  bool PolicyDelegateShouldNotifyDone() const;
  void SetDragImage(const SkBitmap& drag_image);
  bool ShouldDumpNavigationPolicy() const;

  bool ShouldDumpConsoleMessages() const;
  // Controls whether console messages produced by the page are dumped
  // to test output.
  void SetDumpConsoleMessages(bool value, WebFrameTestProxy& source);

  // Add a message to the text dump for the web test.
  void PrintMessage(const std::string& message, WebFrameTestProxy& source);

  // Register a new isolated filesystem with the given files, and return the
  // new filesystem id.
  blink::WebString RegisterIsolatedFileSystem(
      const std::vector<base::FilePath>& file_paths,
      WebFrameTestProxy& source);

  void ProcessWorkItem(mojom::WorkItemPtr work_item, WebFrameTestProxy& source);
  void ReplicateWorkQueueStates(const base::Value::Dict& changed_values,
                                WebFrameTestProxy& source);

  blink::WebEffectiveConnectionType effective_connection_type() const {
    return effective_connection_type_;
  }

  // Determine the the frame is considered in the main window.
  bool IsFrameInMainWindow(blink::WebLocalFrame* frame);

  // Set the main window and test configuration.
  void SetMainWindowAndTestConfiguration(
      blink::WebLocalFrame* initial_local_root,
      mojom::WebTestRunTestConfigurationPtr config);
  const mojom::WebTestRunTestConfiguration& TestConfig() const;

  // Returns an asbsolute file path. This depends on the current test
  // configuration so it should only be called while a test is running.
  blink::WebString GetAbsoluteWebStringFromUTF8Path(
      const std::string& utf8_path);

  // Disables automatic drag and drop in web tests' web frame widget
  // (WebTestWebFrameWidgetImpl).
  //
  // In general, drag and drop will automatically be performed because web tests
  // do not have drag and drop enabled. If you need to control the drag and drop
  // lifecycle yourself, you can disable it here.
  void DisableAutomaticDragDrop(WebFrameTestProxy& source);
  bool AutomaticDragDropEnabled();

  const WebTestRuntimeFlags& GetFlags();

 private:
  friend class TestRunnerBindings;
  friend class WorkQueue;
  class MainWindowTracker;

  // Helper class for managing events queued by methods like QueueLoad or
  // QueueScript.
  class WorkQueue {
    static constexpr const char* kKeyFrozen = "frozen";

   public:
    explicit WorkQueue(TestRunner* controller);
    ~WorkQueue() = default;

    // Reset the state of the class between tests.
    void Reset();

    void AddWork(mojom::WorkItemPtr work_item, WebFrameTestProxy& source);
    void RequestWork(WebFrameTestProxy& source);
    void ProcessWorkItem(mojom::WorkItemPtr work_item,
                         WebFrameTestProxy& source);
    void ReplicateStates(const base::Value::Dict& values,
                         WebFrameTestProxy& source);

    // Takes care of notifying the browser after a change to the state.
    void OnStatesChanged(WebFrameTestProxy& source);

    void set_loading(bool value) { loading_ = value; }

    void set_frozen(bool value) { states_.SetBoolean(kKeyFrozen, value); }
    void set_has_items(bool value) {
      states_.SetBoolean(kDictKeyWorkQueueHasItems, value);
    }
    bool has_items() const { return GetStateValue(kDictKeyWorkQueueHasItems); }

   private:
    bool ProcessWorkItemInternal(mojom::WorkItemPtr work_item,
                                 WebFrameTestProxy& source);

    bool is_frozen() const { return GetStateValue(kKeyFrozen); }

    bool GetStateValue(const char* key) const {
      std::optional<bool> value =
          states_.current_values().FindBoolByDottedPath(key);
      DCHECK(value.has_value());
      return value.value();
    }

    bool loading_ = true;
    // Collection of flags to be synced with the browser process.
    TrackedDictionary states_;

    raw_ptr<TestRunner> controller_;
  };

  // If the main test window's main frame is hosted in this renderer process,
  // then this will return it. Otherwise, it is in another process and this
  // returns null.
  WebFrameTestProxy* FindInProcessMainWindowMainFrame();

  ///////////////////////////////////////////////////////////////////////////
  // Methods dealing with the test logic

  // By default, tests end when page load is complete. These methods are used
  // to delay the completion of the test until NotifyDone is called.
  void WaitUntilDone(WebFrameTestProxy& source);
  void NotifyDone(WebFrameTestProxy& source);

  // When there are no conditions left to wait for, this is called to cause the
  // test to end, collect results, and inform the browser.
  void FinishTest(WebFrameTestProxy& source);

  // Methods for adding actions to the work queue. Used in conjunction with
  // WaitUntilDone/NotifyDone above.
  void QueueBackNavigation(int how_far_back, WebFrameTestProxy& source);
  void QueueForwardNavigation(int how_far_forward, WebFrameTestProxy& source);
  void QueueReload(WebFrameTestProxy& source);
  void QueueLoadingScript(const std::string& script, WebFrameTestProxy& source);
  void QueueNonLoadingScript(const std::string& script,
                             WebFrameTestProxy& source);
  void QueueLoad(const GURL& current_url,
                 const std::string& relative_url,
                 const std::string& target,
                 WebFrameTestProxy& source);

  // Called from the TestRunnerBindings to inform that the test has modified
  // the TestPreferences. This will update the WebkitPreferences in the renderer
  // and the browser.
  void OnTestPreferencesChanged(const TestPreferences& test_prefs,
                                WebFrameTestProxy& frame);

  // Causes navigation actions just printout the intended navigation instead
  // of taking you to the page. This is used for cases like mailto, where you
  // don't actually want to open the mail program.
  void SetCustomPolicyDelegate(gin::Arguments* args, WebFrameTestProxy& source);

  // Delays completion of the test until the policy delegate runs.
  void WaitForPolicyDelegate(WebFrameTestProxy& source);

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

  // Set the mock orientation on |view| to |orientation|.
  void SetMockScreenOrientation(const std::string& orientation,
                                WebFrameTestProxy& frame);
  // Disable any mock orientation on |view| that is set.
  void DisableMockScreenOrientation(blink::WebView* view);

  ///////////////////////////////////////////////////////////////////////////
  // Methods that modify the state of TestRunner

  // This function sets a flag that tells the test runner to print a line of
  // descriptive text for each editing command. It takes no arguments, and
  // ignores any that may be present.
  void DumpEditingCallbacks(WebFrameTestProxy& source);

  // This function sets a flag that tells the test runner to dump pages as
  // plain text. The pixel results will not be generated for this test.
  // It has higher priority than DumpAsMarkup() and DumpAsLayout().
  void DumpAsText(WebFrameTestProxy& source);

  // This function sets a flag that tells the test runner to dump pages as
  // the DOM contents, rather than as a text representation of the renderer's
  // state. The pixel results will not be generated for this test. It has
  // higher priority than DumpAsLayout(), but lower than DumpAsText().
  void DumpAsMarkup(WebFrameTestProxy& source);

  // This function sets a flag that tells the test runner to dump pages as
  // plain text. It will also generate a pixel dump for the test.
  void DumpAsTextWithPixelResults(WebFrameTestProxy& source);

  // This function sets a flag that tells the test runner to dump pages as
  // text representation of the layout. The pixel results will not be generated
  // for this test. It has lower priority than DumpAsText() and DumpAsMarkup().
  void DumpAsLayout(WebFrameTestProxy& source);

  // This function sets a flag that tells the test runner to dump pages as
  // text representation of the layout. It will also generate a pixel dump for
  // the test.
  void DumpAsLayoutWithPixelResults(WebFrameTestProxy& source);

  // This function sets a flag that tells the test runner to recursively dump
  // all frames as text, markup or layout depending on which of DumpAsText,
  // DumpAsMarkup and DumpAsLayout is effective.
  void DumpChildFrames(WebFrameTestProxy& source);

  // This function sets a flag that tells the test runner to print out the
  // information about icon changes notifications from WebKit.
  void DumpIconChanges(WebFrameTestProxy& source);

  // Deals with Web Audio WAV file data.
  void SetAudioData(const gin::ArrayBufferView& view);

  // This function sets a flag that tells the test runner to print a line of
  // descriptive text for each frame load callback. It takes no arguments, and
  // ignores any that may be present.
  void DumpFrameLoadCallbacks(WebFrameTestProxy& source);

  // This function sets a flag that tells the test runner to print a line of
  // descriptive text for each PingLoader dispatch. It takes no arguments, and
  // ignores any that may be present.
  void DumpPingLoaderCallbacks(WebFrameTestProxy& source);

  // This function sets a flag that tells the test runner to print a line of
  // user gesture status text for some frame load callbacks. It takes no
  // arguments, and ignores any that may be present.
  void DumpUserGestureInFrameLoadCallbacks(WebFrameTestProxy& source);

  void DumpTitleChanges(WebFrameTestProxy& source);

  // This function sets a flag that tells the test runner to dump the MIME type
  // for each resource that was loaded. It takes no arguments, and ignores any
  // that may be present.
  void DumpResourceResponseMIMETypes();

  // WebContentSettingsClient related.
  void SetStorageAllowed(bool allowed, WebFrameTestProxy& source);
  void SetAllowRunningOfInsecureContent(bool allowed,
                                        WebFrameTestProxy& source);
  void DumpPermissionClientCallbacks(WebFrameTestProxy& source);

  // This function sets a flag that tells the test runner to print out a text
  // representation of the back/forward list. It ignores all arguments.
  void DumpBackForwardList();

  void DumpSelectionRect(WebFrameTestProxy& source);

  // Causes layout to happen as if targeted to printed pages.
  void SetPrinting(WebFrameTestProxy& source);
  void SetPrintingForFrame(const std::string& frame_name,
                           WebFrameTestProxy& source);
  void SetPrintingSize(int width, int height, WebFrameTestProxy& source);
  void SetPrintingMargin(int size, WebFrameTestProxy& source);
  void SetShouldCenterAndShrinkToFitPaper(bool b) {
    should_center_and_shrink_to_fit_paper_ = b;
  }
  void SetPrintingScaleFactor(float factor) { printing_scale_factor_ = factor; }

  void SetShouldStayOnPageAfterHandlingBeforeUnload(bool value,
                                                    WebFrameTestProxy& source);

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
  void WaitUntilExternalURLLoad(WebFrameTestProxy& source);

  // This function sets a flag to dump the drag image when the next drag&drop is
  // initiated. It is equivalent to DumpAsTextWithPixelResults but the pixel
  // results will be the drag image instead of a snapshot of the page.
  void DumpDragImage(WebFrameTestProxy& source);

  // Sets a flag that sets a flag to dump the default navigation policy passed
  // to the DecidePolicyForNavigation callback.
  void DumpNavigationPolicy(WebFrameTestProxy& source);

  // Controls whether JavaScript dialogs such as alert() are dumped to test
  // output.
  void SetDumpJavaScriptDialogs(bool value, WebFrameTestProxy& source);

  // Overrides the NetworkQualityEstimator's estimated network type. If |type|
  // is TypeUnknown the NQE's value is used. Be sure to call this with
  // TypeUnknown at the end of your test if you use this.
  void SetEffectiveConnectionType(
      blink::WebEffectiveConnectionType connection_type);

  // Takes care of notifying the browser after a change to web test runtime
  // flags.
  void OnWebTestRuntimeFlagsChanged(WebFrameTestProxy& source);

  ///////////////////////////////////////////////////////////////////////////
  // Internal helpers

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

  // If set, pretend that the specified default page size when printing (see
  // `SetPrintingSize()`) is also the size of the imaginary paper, so that any
  // CSS @page size that overrides the default page size will be centered on
  // paper, and scaled down to fit if required. This is the default behavior
  // when printing to an actual printer (as opposed to generating a PDF) in
  // Chrome.
  bool should_center_and_shrink_to_fit_paper_ = false;

  // The scale factor to apply to printed content.
  float printing_scale_factor_ = 1.0;

  std::set<std::string> http_headers_to_clear_;
  bool clear_referrer_ = false;

  // WAV audio data is stored here.
  std::vector<uint8_t> audio_data_;

  base::flat_set<raw_ptr<WebFrameTestProxy, CtnExperimental>> main_frames_;

  // Keeps track of which WebViews that are main windows.
  std::vector<std::unique_ptr<MainWindowTracker>> main_windows_;

  // This is non empty when a load is in progress.
  std::vector<raw_ptr<blink::WebFrame, VectorExperimental>> loading_frames_;
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

  mojom::WebTestRunTestConfiguration test_config_;

  base::WeakPtrFactory<TestRunner> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_TEST_RUNNER_H_
