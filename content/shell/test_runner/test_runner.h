// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_TEST_RUNNER_H_
#define CONTENT_SHELL_TEST_RUNNER_TEST_RUNNER_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "content/shell/test_runner/mock_screen_orientation_client.h"
#include "content/shell/test_runner/test_runner_export.h"
#include "content/shell/test_runner/web_test_runner.h"
#include "content/shell/test_runner/web_test_runtime_flags.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "v8/include/v8.h"

class GURL;

namespace blink {
class WebContentSettingsClient;
class WebFrame;
class WebLocalFrame;
class WebString;
class WebView;
}

namespace gin {
class ArrayBufferView;
class Arguments;
}

namespace test_runner {
class MockContentSettingsClient;
class MockScreenOrientationClient;
class SpellCheckClient;
class TestInterfaces;
class TestRunnerForSpecificView;
class WebTestDelegate;

// TestRunner class currently has dual purpose:
// 1. It implements TestRunner javascript bindings for "global" / "ambient".
//    Examples:
//    - TestRunner.DumpAsText (test flag affecting test behavior)
//    - TestRunner.SetAllowRunningOfInsecureContent (test flag affecting product
//      behavior)
//    - TestRunner.SetTextSubpixelPositioning (directly interacts with product).
//    Note that "per-view" (non-"global") bindings are handled by
//    instances of TestRunnerForSpecificView class.
// 2. It manages global test state.  Example:
//    - Tracking topLoadingFrame that can finish the test when it loads.
//    - WorkQueue holding load requests from the TestInterfaces
//    - WebTestRuntimeFlags
class TestRunner : public WebTestRunner {
 public:
  explicit TestRunner(TestInterfaces*);
  virtual ~TestRunner();

  void Install(blink::WebLocalFrame* frame,
               base::WeakPtr<TestRunnerForSpecificView> view_test_runner);

  void SetDelegate(WebTestDelegate*);
  void SetMainView(blink::WebView*);

  void Reset();

  void SetTestIsRunning(bool);
  bool TestIsRunning() const { return test_is_running_; }

  // Finishes the test if it is ready. This should be called before running
  // tasks that will change state, so that the test can capture the current
  // state. Specifically, should run before the BeginMainFrame step which does
  // layout and animation etc.
  // This does *not* run as part of loading finishing because that happens in
  // the middle of blink call stacks that have inconsistent state.
  void FinishTestIfReady();

  // WebTestRunner implementation.
  bool ShouldGeneratePixelResults() override;
  bool ShouldDumpAsAudio() const override;
  void GetAudioData(std::vector<unsigned char>* buffer_view) const override;
  bool IsRecursiveLayoutDumpRequested() override;
  std::string DumpLayout(blink::WebLocalFrame* frame) override;
  bool ShouldDumpSelectionRect() const override;
  bool CanDumpPixelsFromRenderer() const override;
  void DumpPixelsAsync(
      content::RenderView* render_view,
      base::OnceCallback<void(const SkBitmap&)> callback) override;
  void ReplicateWebTestRuntimeFlagsChanges(
      const base::DictionaryValue& changed_values) override;
  bool HasCustomTextDump(std::string* custom_text_dump) const override;
  bool ShouldDumpBackForwardList() const override;
  blink::WebContentSettingsClient* GetWebContentSettings() const override;
  blink::WebTextCheckClient* GetWebTextCheckClient() const override;
  void SetFocus(blink::WebView* web_view, bool focus) override;

  // Methods used by WebViewTestClient and WebFrameTestClient.
  std::string GetAcceptLanguages() const;
  bool ShouldStayOnPageAfterHandlingBeforeUnload() const;
  MockScreenOrientationClient* GetMockScreenOrientationClient();
  bool ShouldDumpAsCustomText() const;
  std::string CustomDumpText() const;
  void ShowDevTools(const std::string& settings,
                    const std::string& frontend_url);
  void SetV8CacheDisabled(bool);
  void SetShouldDumpAsText(bool);
  void SetShouldDumpAsMarkup(bool);
  void SetShouldDumpAsLayout(bool);
  void SetCustomTextOutput(const std::string& text);
  void SetShouldGeneratePixelResults(bool);
  void SetShouldDumpFrameLoadCallbacks(bool);
  void SetShouldEnableViewSource(bool);
  bool ShouldDumpEditingCallbacks() const;
  bool ShouldDumpFrameLoadCallbacks() const;
  bool ShouldDumpPingLoaderCallbacks() const;
  bool ShouldDumpUserGestureInFrameLoadCallbacks() const;
  bool ShouldDumpTitleChanges() const;
  bool ShouldDumpIconChanges() const;
  bool ShouldDumpCreateView() const;
  bool CanOpenWindows() const;
  bool ShouldDumpSpellCheckCallbacks() const;
  bool ShouldWaitUntilExternalURLLoad() const;
  const std::set<std::string>* HttpHeadersToClear() const;
  bool is_web_platform_tests_mode() const {
    return is_web_platform_tests_mode_;
  }
  void set_is_web_platform_tests_mode() { is_web_platform_tests_mode_ = true; }
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

  blink::WebFrame* MainFrame() const;
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

  bool ShouldDumpJavaScriptDialogs() const;

  blink::WebEffectiveConnectionType effective_connection_type() const {
    return effective_connection_type_;
  }

  // A single item in the work queue.
  class WorkItem {
   public:
    virtual ~WorkItem() {}

    // Returns true if this started a load.
    virtual bool Run(WebTestDelegate*, blink::WebView*) = 0;
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

  ///////////////////////////////////////////////////////////////////////////
  // Methods dealing with the test logic

  // By default, tests end when page load is complete. These methods are used
  // to delay the completion of the test until NotifyDone is called.
  void NotifyDone();
  void WaitUntilDone();

  // Methods for adding actions to the work queue. Used in conjunction with
  // WaitUntilDone/NotifyDone above.
  void QueueBackNavigation(int how_far_back);
  void QueueForwardNavigation(int how_far_forward);
  void QueueReload();
  void QueueLoadingScript(const std::string& script);
  void QueueNonLoadingScript(const std::string& script);
  void QueueLoad(const std::string& url, const std::string& target);

  // Causes navigation actions just printout the intended navigation instead
  // of taking you to the page. This is used for cases like mailto, where you
  // don't actually want to open the mail program.
  void SetCustomPolicyDelegate(gin::Arguments* args);

  // Delays completion of the test until the policy delegate runs.
  void WaitForPolicyDelegate();

  // Functions for dealing with windows. By default we block all new windows.
  int WindowCount();
  void SetCloseRemainingWindowsWhenComplete(bool close_remaining_windows);
  void ResetTestHelperControllers();

  // Allows web tests to manage origins' allow list.
  void AddOriginAccessAllowListEntry(const std::string& source_origin,
                                     const std::string& destination_protocol,
                                     const std::string& destination_host,
                                     bool allow_destination_subdomains);

  // Add |source_code| as an injected stylesheet to the active document of the
  // window of the current V8 context.
  void InsertStyleSheet(const std::string& source_code);

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

  void EnableAutoResizeMode(int min_width,
                            int min_height,
                            int max_width,
                            int max_height);
  void DisableAutoResizeMode(int new_width, int new_height);

  void SetMockScreenOrientation(const std::string& orientation);
  void DisableMockScreenOrientation();

  ///////////////////////////////////////////////////////////////////////////
  // Methods modifying WebPreferences.

  // Set the WebPreference that controls webkit's popup blocking.
  void SetPopupBlockingEnabled(bool block_popups);

  void SetJavaScriptCanAccessClipboard(bool can_access);
  void SetAllowFileAccessFromFileURLs(bool allow);
  void OverridePreference(gin::Arguments* arguments);

  // Modify accept_languages in blink::mojom::RendererPreferences.
  void SetAcceptLanguages(const std::string& accept_languages);

  // Enable or disable plugins.
  void SetPluginsEnabled(bool enabled);

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
  void SetPluginsAllowed(bool allowed);
  void SetAllowRunningOfInsecureContent(bool allowed);
  void SetAutoplayAllowed(bool allowed);
  void DumpPermissionClientCallbacks();

  // Sets up a mock DocumentSubresourceFilter to disallow subsequent subresource
  // loads within the current document with the given path |suffixes|. The
  // filter is created and injected even if |suffixes| is empty. If |suffixes|
  // contains the empty string, all subresource loads will be disallowed. If
  // |block_subresources| is false, matching resources will not be blocked but
  // instead marked as matching a disallowed resource.
  void SetDisallowedSubresourcePathSuffixes(
      const std::vector<std::string>& suffixes,
      bool block_subresources);

  // This function sets a flag that tells the test runner to dump all
  // the lines of descriptive text about spellcheck execution.
  void DumpSpellCheckCallbacks();

  // This function sets a flag that tells the test runner to print out a text
  // representation of the back/forward list. It ignores all arguments.
  void DumpBackForwardList();

  void DumpSelectionRect();

  // Causes layout to happen as if targetted to printed pages.
  void SetPrinting();
  void SetPrintingForFrame(const std::string& frame_name);

  // Clears the state from SetPrinting().
  void ClearPrinting();

  void SetShouldStayOnPageAfterHandlingBeforeUnload(bool value);

  // Causes WillSendRequest to clear certain headers.
  void SetWillSendRequestClearHeader(const std::string& header);

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

  // Controls whether the mock spell checker is enabled.
  void SetMockSpellCheckerEnabled(bool enabled);

  ///////////////////////////////////////////////////////////////////////////
  // Methods interacting with the WebViewTestProxy

  ///////////////////////////////////////////////////////////////////////////
  // Methods forwarding to the WebTestDelegate

  // Shows DevTools window.
  void ShowWebInspector(const std::string& str,
                        const std::string& frontend_url);
  void CloseWebInspector();

  void NavigateSecondaryWindow(const GURL& url);
  void InspectSecondaryWindow();

  // Inspect chooser state
  bool IsChooserShown();

  // Allows web tests to exec scripts at WebInspector side.
  void EvaluateInWebInspector(int call_id, const std::string& script);

  // Clears all databases.
  void ClearAllDatabases();
  // Sets the default quota for all origins
  void SetDatabaseQuota(int quota);

  // Sets the cookie policy to:
  // - allow all cookies when |block| is false
  // - block only third-party cookies when |block| is true
  void SetBlockThirdPartyCookies(bool block);

  // Converts a URL starting with file:///tmp/ to the local mapping.
  std::string PathToLocalResource(const std::string& path);

  // Sets the permission's |name| to |value| for a given {origin, embedder}
  // tuple.
  void SetPermission(const std::string& name,
                     const std::string& value,
                     const GURL& origin,
                     const GURL& embedding_origin);

  // Resolve the in-flight beforeinstallprompt event.
  void ResolveBeforeInstallPromptPromise(const std::string& platform);

  // Calls setlocale(LC_ALL, ...) for a specified locale.
  // Resets between tests.
  void SetPOSIXLocale(const std::string& locale);

  // Simulates a click on a Web Notification.
  void SimulateWebNotificationClick(
      const std::string& title,
      const base::Optional<int>& action_index,
      const base::Optional<base::string16>& reply);

  // Simulates closing a Web Notification.
  void SimulateWebNotificationClose(const std::string& title, bool by_user);

  // Simulates a user deleting a content index entry.
  void SimulateWebContentIndexDelete(const std::string& id);

  // Takes care of notifying the delegate after a change to web test runtime
  // flags.
  void OnWebTestRuntimeFlagsChanged();

  ///////////////////////////////////////////////////////////////////////////
  // Internal helpers

  bool IsFramePartOfMainTestWindow(blink::WebFrame*) const;

  void CheckResponseMimeType();

  bool test_is_running_ = false;

  // When reset is called, go through and close all but the main test shell
  // window. By default, set to true but toggled to false using
  // SetCloseRemainingWindowsWhenComplete().
  bool close_remaining_windows_ = false;

  WorkQueue work_queue_;

  // Bound variable to return the name of this platform (chromium).
  std::string platform_name_;

  // Bound variable to store the last tooltip text
  std::string tooltip_text_;

  // Bound variable counting the number of top URLs visited.
  int web_history_item_count_ = 0;

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

  // WAV audio data is stored here.
  std::vector<unsigned char> audio_data_;

  TestInterfaces* test_interfaces_;
  WebTestDelegate* delegate_ = nullptr;
  blink::WebView* main_view_ = nullptr;

  // This is non empty when a load is in progress.
  std::vector<blink::WebFrame*> loading_frames_;
  // When a loading task is started, this bool is set until all loading_frames_
  // are completed and removed. This bool becomes true earlier than
  // loading_frames_ becomes non-empty. Starts as true for the initial load
  // which does not come from the WorkQueue.
  bool running_load_ = true;
  // When NotifyDone() occurs, if loading is still working, it is delayed, and
  // this bool tracks that NotifyDone() was called. This differentiates from a
  // test that was not waiting for NotifyDone() at all.
  bool did_notify_done_ = false;

  // WebContentSettingsClient mock object.
  std::unique_ptr<MockContentSettingsClient> mock_content_settings_client_;

  bool use_mock_theme_ = false;

  MockScreenOrientationClient mock_screen_orientation_client_;
  std::unique_ptr<SpellCheckClient> spellcheck_;

  // Number of currently active color choosers.
  int chooser_count_ = 0;

  // Captured drag image.
  SkBitmap drag_image_;

  // View that was focused by a previous call to TestRunner::SetFocus method.
  // Note - this can be a dangling pointer to an already destroyed WebView (this
  // is ok, because this is taken care of in WebTestDelegate::SetFocus).
  blink::WebView* previously_focused_view_ = nullptr;

  // True when running a test in web_tests/external/wpt/.
  bool is_web_platform_tests_mode_ = false;

  // True if rasterization should be performed during tests that examine
  // fling-style animations. This includes middle-click auto-scroll behaviors.
  // This does not include most "ordinary" animations, such as CSS animations.
  bool animation_requires_raster_ = false;

  // An effective connection type settable by web tests.
  blink::WebEffectiveConnectionType effective_connection_type_ =
      blink::WebEffectiveConnectionType::kTypeUnknown;

  // Forces v8 compilation cache to be disabled (used for inspector tests).
  bool disable_v8_cache_ = false;

  base::WeakPtrFactory<TestRunner> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TestRunner);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_TEST_RUNNER_H_
