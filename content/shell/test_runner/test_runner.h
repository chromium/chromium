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
#include "content/shell/test_runner/layout_test_runtime_flags.h"
#include "content/shell/test_runner/test_runner_export.h"
#include "content/shell/test_runner/web_test_runner.h"
#include "media/midi/midi_service.mojom.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/public/platform/web_image.h"
#include "v8/include/v8.h"

class GURL;
class SkBitmap;

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
// 1. It implements |testRunner| javascript bindings for "global" / "ambient".
//    Examples:
//    - testRunner.dumpAsText (test flag affecting test behavior)
//    - testRunner.setAllowRunningOfInsecureContent (test flag affecting product
//      behavior)
//    - testRunner.setTextSubpixelPositioning (directly interacts with product).
//    Note that "per-view" (non-"global") bindings are handled by
//    instances of TestRunnerForSpecificView class.
// 2. It manages global test state.  Example:
//    - Tracking topLoadingFrame that can finish the test when it loads.
//    - WorkQueue holding load requests from the TestInterfaces
//    - LayoutTestRuntimeFlags
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

  bool UseMockTheme() const { return use_mock_theme_; }

  // WebTestRunner implementation.
  bool ShouldGeneratePixelResults() override;
  bool ShouldDumpAsAudio() const override;
  void GetAudioData(std::vector<unsigned char>* buffer_view) const override;
  bool IsRecursiveLayoutDumpRequested() override;
  std::string DumpLayout(blink::WebLocalFrame* frame) override;
  bool ShouldDumpSelectionRect() const override;
  // Returns true if the browser should capture the pixels instead.
  bool DumpPixelsAsync(
      blink::WebLocalFrame* frame,
      base::OnceCallback<void(const SkBitmap&)> callback) override;
  void ReplicateLayoutTestRuntimeFlagsChanges(
      const base::DictionaryValue& changed_values) override;
  bool HasCustomTextDump(std::string* custom_text_dump) const override;
  bool ShouldDumpBackForwardList() const override;
  blink::WebContentSettingsClient* GetWebContentSettings() const override;
  blink::WebTextCheckClient* GetWebTextCheckClient() const override;
  void SetFocus(blink::WebView* web_view, bool focus) override;

  // Methods used by WebViewTestClient and WebFrameTestClient.
  std::string GetAcceptLanguages() const;
  bool shouldStayOnPageAfterHandlingBeforeUnload() const;
  MockScreenOrientationClient* getMockScreenOrientationClient();
  bool isPrinting() const;
  bool shouldDumpAsCustomText() const;
  std::string customDumpText() const;
  void ShowDevTools(const std::string& settings,
                    const std::string& frontend_url);
  void SetV8CacheDisabled(bool);
  void setShouldDumpAsText(bool);
  void setShouldDumpAsMarkup(bool);
  void setShouldDumpAsLayout(bool);
  void setCustomTextOutput(const std::string& text);
  void setShouldGeneratePixelResults(bool);
  void setShouldDumpFrameLoadCallbacks(bool);
  void setShouldEnableViewSource(bool);
  bool shouldDumpEditingCallbacks() const;
  bool shouldDumpFrameLoadCallbacks() const;
  bool shouldDumpPingLoaderCallbacks() const;
  bool shouldDumpUserGestureInFrameLoadCallbacks() const;
  bool shouldDumpTitleChanges() const;
  bool shouldDumpIconChanges() const;
  bool shouldDumpCreateView() const;
  bool canOpenWindows() const;
  bool shouldDumpResourceLoadCallbacks() const;
  bool shouldDumpResourceResponseMIMETypes() const;
  bool shouldDumpSpellCheckCallbacks() const;
  bool shouldWaitUntilExternalURLLoad() const;
  const std::set<std::string>* httpHeadersToClear() const;
  bool is_web_platform_tests_mode() const {
    return is_web_platform_tests_mode_;
  }
  void set_is_web_platform_tests_mode() { is_web_platform_tests_mode_ = true; }
  const base::Optional<std::vector<std::string>>& file_chooser_paths() const {
    return file_chooser_paths_;
  }
  bool animation_requires_raster() const { return animation_requires_raster_; }
  void SetAnimationRequiresRaster(bool do_raster);

  // To be called when |frame| starts loading - TestRunner will check if
  // there is currently no top-loading-frame being tracked and if so, then it
  // will return true and start tracking |frame| as the top-loading-frame.
  bool tryToSetTopLoadingFrame(blink::WebFrame* frame);

  // To be called when |frame| finishes loading - TestRunner will check if
  // |frame| is currently tracked as the top-loading-frame, and if yes, then it
  // will return true, stop top-loading-frame tracking, and potentially finish
  // the test (unless testRunner.waitUntilDone() was called and/or there are
  // pending load requests in WorkQueue).
  bool tryToClearTopLoadingFrame(blink::WebFrame*);

  blink::WebFrame* mainFrame() const;
  blink::WebFrame* topLoadingFrame() const;
  void policyDelegateDone();
  bool policyDelegateEnabled() const;
  bool policyDelegateIsPermissive() const;
  bool policyDelegateShouldNotifyDone() const;
  void setToolTipText(const blink::WebString&);
  void setDragImage(const SkBitmap& drag_image);
  bool shouldDumpNavigationPolicy() const;

  midi::mojom::Result midiAccessorResult();

  bool ShouldDumpConsoleMessages() const;
  // Controls whether console messages produced by the page are dumped
  // to test output.
  void SetDumpConsoleMessages(bool value);

  bool ShouldDumpJavaScriptDialogs() const;

  void SetShouldUseInnerTextDump(bool value);

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

  // Helper class for managing events queued by methods like queueLoad or
  // queueScript.
  class WorkQueue {
   public:
    explicit WorkQueue(TestRunner* controller);
    virtual ~WorkQueue();
    void ProcessWorkSoon();

    // Reset the state of the class between tests.
    void Reset();

    void AddWork(WorkItem*);

    void set_frozen(bool frozen) { frozen_ = frozen; }
    bool is_empty() { return queue_.empty(); }

   private:
    void ProcessWork();

    base::circular_deque<WorkItem*> queue_;
    bool frozen_;
    TestRunner* controller_;

    base::WeakPtrFactory<WorkQueue> weak_factory_;
  };

  ///////////////////////////////////////////////////////////////////////////
  // Methods dealing with the test logic

  // By default, tests end when page load is complete. These methods are used
  // to delay the completion of the test until notifyDone is called.
  void NotifyDone();
  void WaitUntilDone();

  // Methods for adding actions to the work queue. Used in conjunction with
  // waitUntilDone/notifyDone above.
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

  // Allows layout tests to manage origins' allow list.
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

  bool EnableAutoResizeMode(int min_width,
                            int min_height,
                            int max_width,
                            int max_height);
  bool DisableAutoResizeMode(int new_width, int new_height);

  void SetMockScreenOrientation(const std::string& orientation);
  void DisableMockScreenOrientation();

  ///////////////////////////////////////////////////////////////////////////
  // Methods modifying WebPreferences.

  // Set the WebPreference that controls webkit's popup blocking.
  void SetPopupBlockingEnabled(bool block_popups);

  void SetJavaScriptCanAccessClipboard(bool can_access);
  void SetXSSAuditorEnabled(bool enabled);
  void SetAllowUniversalAccessFromFileURLs(bool allow);
  void SetAllowFileAccessFromFileURLs(bool allow);
  void OverridePreference(gin::Arguments* arguments);

  // Modify accept_languages in RendererPreferences.
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

  // This function sets a flag that tells the test runner to dump a descriptive
  // line for each resource load callback. It takes no arguments, and ignores
  // any that may be present.
  void DumpResourceLoadCallbacks();

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
  // contains the empty string, all subresource loads will be disallowed.
  void SetDisallowedSubresourcePathSuffixes(
      const std::vector<std::string>& suffixes);

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

  // Sets a flag to enable the mock theme.
  void SetUseMockTheme(bool use);

  // Sets a flag that causes the test to be marked as completed when the
  // WebLocalFrameClient receives a loadURLExternally() call.
  void WaitUntilExternalURLLoad();

  // This function sets a flag to dump the drag image when the next drag&drop is
  // initiated. It is equivalent to DumpAsTextWithPixelResults but the pixel
  // results will be the drag image instead of a snapshot of the page.
  void DumpDragImage();

  // Sets a flag that tells the WebViewTestProxy to dump the default navigation
  // policy passed to the decidePolicyForNavigation callback.
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

  // Allows layout tests to exec scripts at WebInspector side.
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

  // MIDI function to control permission handling.
  void SetMIDIAccessorResult(midi::mojom::Result result);

  // Simulates a click on a Web Notification.
  void SimulateWebNotificationClick(
      const std::string& title,
      const base::Optional<int>& action_index,
      const base::Optional<base::string16>& reply);

  // Simulates closing a Web Notification.
  void SimulateWebNotificationClose(const std::string& title, bool by_user);

  // Takes care of notifying the delegate after a change to layout test runtime
  // flags.
  void OnLayoutTestRuntimeFlagsChanged();

  // Sets a list of file paths to be selected in the next file chooser session.
  // If an empty list is specified, the next file chooser will be canceled.
  void SetFileChooserPaths(const std::vector<std::string>& paths);

  ///////////////////////////////////////////////////////////////////////////
  // Internal helpers

  bool IsFramePartOfMainTestWindow(blink::WebFrame*) const;

  void CheckResponseMimeType();

  // In the Mac code, this is called to trigger the end of a test after the
  // page has finished loading. From here, we can generate the dump for the
  // test.
  void LocationChangeDone();

  bool test_is_running_;

  // When reset is called, go through and close all but the main test shell
  // window. By default, set to true but toggled to false using
  // setCloseRemainingWindowsWhenComplete().
  bool close_remaining_windows_;

  WorkQueue work_queue_;

  // Bound variable to return the name of this platform (chromium).
  std::string platform_name_;

  // Bound variable to store the last tooltip text
  std::string tooltip_text_;

  // Bound variable counting the number of top URLs visited.
  int web_history_item_count_;

  // Flags controlling what content gets dumped as a layout text result.
  LayoutTestRuntimeFlags layout_test_runtime_flags_;

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

  // startSession() result of MockWebMIDIAccessor for testing.
  midi::mojom::Result midi_accessor_result_;

  std::set<std::string> http_headers_to_clear_;

  // WAV audio data is stored here.
  std::vector<unsigned char> audio_data_;

  TestInterfaces* test_interfaces_;
  WebTestDelegate* delegate_;
  blink::WebView* main_view_;

  // This is non-0 IFF a load is in progress.
  blink::WebFrame* top_loading_frame_;

  // WebContentSettingsClient mock object.
  std::unique_ptr<MockContentSettingsClient> mock_content_settings_client_;

  bool use_mock_theme_;

  std::unique_ptr<MockScreenOrientationClient> mock_screen_orientation_client_;
  std::unique_ptr<SpellCheckClient> spellcheck_;

  // Number of currently active color choosers.
  int chooser_count_;

  // Captured drag image.
  SkBitmap drag_image_;

  // View that was focused by a previous call to TestRunner::SetFocus method.
  // Note - this can be a dangling pointer to an already destroyed WebView (this
  // is ok, because this is taken care of in WebTestDelegate::SetFocus).
  blink::WebView* previously_focused_view_;

  // True when running a test in LayoutTests/external/wpt/.
  bool is_web_platform_tests_mode_;

  // True if rasterization should be performed during tests that examine
  // fling-style animations. This includes middle-click auto-scroll behaviors.
  // This does not include most "ordinary" animations, such as CSS animations.
  bool animation_requires_raster_;

  // An effective connection type settable by layout tests.
  blink::WebEffectiveConnectionType effective_connection_type_;

  // Forces v8 compilation cache to be disabled (used for inspector tests).
  bool disable_v8_cache_ = false;

  base::Optional<std::vector<std::string>> file_chooser_paths_;

  base::WeakPtrFactory<TestRunner> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestRunner);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_TEST_RUNNER_H_
