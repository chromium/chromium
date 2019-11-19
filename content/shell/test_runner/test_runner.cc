// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/test_runner.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "content/shell/common/web_test/web_test_switches.h"
#include "content/shell/test_runner/layout_dump.h"
#include "content/shell/test_runner/mock_content_settings_client.h"
#include "content/shell/test_runner/mock_web_document_subresource_filter.h"
#include "content/shell/test_runner/pixel_dump.h"
#include "content/shell/test_runner/spell_check_client.h"
#include "content/shell/test_runner/test_common.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/test_preferences.h"
#include "content/shell/test_runner/test_runner_for_specific_view.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "gin/arguments.h"
#include "gin/array_buffer.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "services/network/public/mojom/cors.mojom.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_array_buffer.h"
#include "third_party/blink/public/web/web_array_buffer_converter.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_page_importance_signals.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"

#if defined(OS_LINUX) || defined(OS_FUCHSIA)
#include "third_party/blink/public/platform/web_font_render_style.h"
#endif

namespace test_runner {

namespace {

double GetDefaultDeviceScaleFactor() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kForceDeviceScaleFactor)) {
    double scale;
    std::string value =
        command_line->GetSwitchValueASCII(switches::kForceDeviceScaleFactor);
    if (base::StringToDouble(value, &scale))
      return scale;
  }
  return 1.f;
}

void ConvertAndSet(gin::Arguments* args, int* set_param) {
  v8::Local<v8::Value> value = args->PeekNext();
  v8::Maybe<int> result = value->Int32Value(args->GetHolderCreationContext());

  if (result.IsNothing()) {
    // Skip so the error is thrown for the correct argument as PeekNext doesn't
    // update the current argument pointer.
    args->Skip();
    args->ThrowError();
    return;
  }

  *set_param = result.ToChecked();
}

void ConvertAndSet(gin::Arguments* args, bool* set_param) {
  v8::Local<v8::Value> value = args->PeekNext();
  *set_param = value->BooleanValue(args->isolate());
}

void ConvertAndSet(gin::Arguments* args, blink::WebString* set_param) {
  v8::Local<v8::Value> value = args->PeekNext();
  v8::MaybeLocal<v8::String> result =
      value->ToString(args->GetHolderCreationContext());

  if (result.IsEmpty()) {
    // Skip so the error is thrown for the correct argument as PeekNext doesn't
    // update the current argument pointer.
    args->Skip();
    args->ThrowError();
    return;
  }

  *set_param = V8StringToWebString(args->isolate(), result.ToLocalChecked());
}

}  // namespace

class TestRunnerBindings : public gin::Wrappable<TestRunnerBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static void Install(base::WeakPtr<TestRunner> test_runner,
                      base::WeakPtr<TestRunnerForSpecificView> view_test_runner,
                      blink::WebLocalFrame* frame,
                      bool is_wpt_reftest,
                      bool is_frame_part_of_main_test_window);

 private:
  explicit TestRunnerBindings(
      base::WeakPtr<TestRunner> test_runner,
      base::WeakPtr<TestRunnerForSpecificView> view_test_runner);
  ~TestRunnerBindings() override;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  void AddOriginAccessAllowListEntry(const std::string& source_origin,
                                     const std::string& destination_protocol,
                                     const std::string& destination_host,
                                     bool allow_destination_subdomains);
  void AddWebPageOverlay();
  void SetHighlightAds();
  void CapturePixelsAsyncThen(v8::Local<v8::Function> callback);
  void ClearAllDatabases();
  void ClearPrinting();
  void CopyImageAtAndCapturePixelsAsyncThen(int x,
                                            int y,
                                            v8::Local<v8::Function> callback);
  void DidAcquirePointerLock();
  void DidLosePointerLock();
  void DidNotAcquirePointerLock();
  void DisableMockScreenOrientation();
  void DispatchBeforeInstallPromptEvent(
      const std::vector<std::string>& event_platforms,
      v8::Local<v8::Function> callback);
  void DumpAsMarkup();
  void DumpAsText();
  void DumpAsTextWithPixelResults();
  void DumpAsLayout();
  void DumpAsLayoutWithPixelResults();
  void DumpChildFrames();
  void DumpBackForwardList();
  void DumpCreateView();
  void DumpDragImage();
  void DumpEditingCallbacks();
  void DumpFrameLoadCallbacks();
  void DumpIconChanges();
  void DumpNavigationPolicy();
  void DumpPermissionClientCallbacks();
  void DumpPingLoaderCallbacks();
  void DumpSelectionRect();
  void DumpSpellCheckCallbacks();
  void DumpTitleChanges();
  void DumpUserGestureInFrameLoadCallbacks();
  void EnableUseZoomForDSF(v8::Local<v8::Function> callback);
  void EvaluateScriptInIsolatedWorld(int world_id, const std::string& script);
  void ExecCommand(gin::Arguments* args);
  void ForceNextDrawingBufferCreationToFail();
  void ForceNextWebGLContextCreationToFail();
  void ForceRedSelectionColors();
  void GetBluetoothManualChooserEvents(v8::Local<v8::Function> callback);
  void GetManifestThen(v8::Local<v8::Function> callback);
  void InsertStyleSheet(const std::string& source_code);
  void UpdateAllLifecyclePhasesAndComposite();
  void UpdateAllLifecyclePhasesAndCompositeThen(
      v8::Local<v8::Function> callback);
  void SetAnimationRequiresRaster(bool do_raster);
  void LogToStderr(const std::string& output);
  void NotImplemented(const gin::Arguments& args);
  void NotifyDone();
  void OverridePreference(gin::Arguments* args);
  void QueueBackNavigation(int how_far_back);
  void QueueForwardNavigation(int how_far_forward);
  void QueueLoad(gin::Arguments* args);
  void QueueLoadingScript(const std::string& script);
  void QueueNonLoadingScript(const std::string& script);
  void QueueReload();
  void RemoveSpellCheckResolvedCallback();
  void RemoveWebPageOverlay();
  void ResetTestHelperControllers();
  void ResolveBeforeInstallPromptPromise(const std::string& platform);
  void RunIdleTasks(v8::Local<v8::Function> callback);
  void SendBluetoothManualChooserEvent(const std::string& event,
                                       const std::string& argument);
  void SetAcceptLanguages(const std::string& accept_languages);
  void SetAllowFileAccessFromFileURLs(bool allow);
  void SetAllowRunningOfInsecureContent(bool allowed);
  void SetAutoplayAllowed(bool allowed);
  void SetBlockThirdPartyCookies(bool block);
  void SetAudioData(const gin::ArrayBufferView& view);
  void SetBackingScaleFactor(double value, v8::Local<v8::Function> callback);
  void SetBluetoothFakeAdapter(const std::string& adapter_name,
                               v8::Local<v8::Function> callback);
  void SetBluetoothManualChooser(bool enable);
  void SetCanOpenWindows();
  void SetCloseRemainingWindowsWhenComplete(gin::Arguments* args);
  void SetColorProfile(const std::string& name,
                       v8::Local<v8::Function> callback);
  void SetCustomPolicyDelegate(gin::Arguments* args);
  void SetCustomTextOutput(const std::string& output);
  void SetDatabaseQuota(int quota);
  void SetDisallowedSubresourcePathSuffixes(
      const std::vector<std::string>& suffixes,
      bool block_subresources);
  void SetDomainRelaxationForbiddenForURLScheme(bool forbidden,
                                                const std::string& scheme);
  void SetDumpConsoleMessages(bool value);
  void SetDumpJavaScriptDialogs(bool value);
  void SetEffectiveConnectionType(const std::string& connection_type);
  void SetMockSpellCheckerEnabled(bool enabled);
  void SetImagesAllowed(bool allowed);
  void SetIsolatedWorldInfo(int world_id,
                            v8::Local<v8::Value> security_origin,
                            v8::Local<v8::Value> content_security_policy);
  void SetJavaScriptCanAccessClipboard(bool can_access);
  void SetMockScreenOrientation(const std::string& orientation);
  void SetPOSIXLocale(const std::string& locale);
  void SetPageVisibility(const std::string& new_visibility);
  void SetPermission(const std::string& name,
                     const std::string& value,
                     const std::string& origin,
                     const std::string& embedding_origin);
  void SetPluginsAllowed(bool allowed);
  void SetPluginsEnabled(bool enabled);
  void SetPointerLockWillFailSynchronously();
  void SetPointerLockWillRespondAsynchronously();
  void SetPopupBlockingEnabled(bool block_popups);
  void SetPrinting();
  void SetPrintingForFrame(const std::string& frame_name);
  void SetScriptsAllowed(bool allowed);
  void SetShouldGeneratePixelResults(bool);
  void SetShouldStayOnPageAfterHandlingBeforeUnload(bool value);
  void SetSpellCheckResolvedCallback(v8::Local<v8::Function> callback);
  void SetStorageAllowed(bool allowed);
  void SetTabKeyCyclesThroughElements(bool tab_key_cycles_through_elements);
  void SetTextDirection(const std::string& direction_name);
  void SetTextSubpixelPositioning(bool value);
  void SetViewSourceForFrame(const std::string& name, bool enabled);
  void SetWillSendRequestClearHeader(const std::string& header);
  void SetWindowIsKey(bool value);
  void NavigateSecondaryWindow(const std::string& url);
  void InspectSecondaryWindow();
  void SimulateWebNotificationClick(gin::Arguments* args);
  void SimulateWebNotificationClose(const std::string& title, bool by_user);
  void SimulateWebContentIndexDelete(const std::string& id);
  void UseUnfortunateSynchronousResizeMode();
  void WaitForPolicyDelegate();
  void WaitUntilDone();
  void WaitUntilExternalURLLoad();
  void DisableAutoResizeMode(int new_width, int new_height);
  void EnableAutoResizeMode(int min_width,
                            int min_height,
                            int max_width,
                            int max_height);
  v8::Local<v8::Value> EvaluateScriptInIsolatedWorldAndReturnValue(
      int world_id,
      const std::string& script);
  bool FindString(const std::string& search_text,
                  const std::vector<std::string>& options_array);
  bool HasCustomPageSizeStyle(int page_index);
  bool IsChooserShown();

  bool IsCommandEnabled(const std::string& command);
  std::string PathToLocalResource(const std::string& path);
  std::string PlatformName();
  std::string SelectionAsMarkup();
  std::string TooltipText();

  int WebHistoryItemCount();
  int WindowCount();

  base::WeakPtr<TestRunner> runner_;
  base::WeakPtr<TestRunnerForSpecificView> view_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestRunnerBindings);
};

gin::WrapperInfo TestRunnerBindings::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
void TestRunnerBindings::Install(
    base::WeakPtr<TestRunner> test_runner,
    base::WeakPtr<TestRunnerForSpecificView> view_test_runner,
    blink::WebLocalFrame* frame,
    bool is_wpt_test,
    bool is_frame_part_of_main_test_window) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  TestRunnerBindings* wrapped =
      new TestRunnerBindings(test_runner, view_test_runner);
  gin::Handle<TestRunnerBindings> bindings =
      gin::CreateHandle(isolate, wrapped);
  if (bindings.IsEmpty())
    return;
  v8::Local<v8::Object> global = context->Global();
  v8::Local<v8::Value> v8_bindings = bindings.ToV8();

  global->Set(context, gin::StringToV8(isolate, "testRunner"), v8_bindings)
      .Check();

  // Inject some JavaScript to the top-level frame of a reftest in the
  // web-platform-tests suite to have the same reftest screenshot timing as
  // upstream WPT:
  //
  // 1. For normal reftest, we would like to take screenshots after web fonts
  //    are loaded, i.e. replicate the behavior of this injected script:
  //    https://github.com/web-platform-tests/wpt/blob/master/tools/wptrunner/wptrunner/executors/reftest-wait_webdriver.js
  // 2. For reftests with a 'reftest-wait' class on the root element, reference
  //    comparison is delayed until that class attribute is removed. To support
  //    this feature, we use a mutation observer.
  //    https://web-platform-tests.org/writing-tests/reftests.html#controlling-when-comparison-occurs
  //
  // Note that this method may be called multiple times on a frame, so we put
  // the code behind a flag. The flag is safe to be installed on testRunner
  // because WPT reftests never access this object.
  if (is_wpt_test && is_frame_part_of_main_test_window && !frame->Parent() &&
      !frame->Opener()) {
    frame->ExecuteScript(blink::WebString(
        R"(if (!window.testRunner._wpt_reftest_setup) {
          window.testRunner._wpt_reftest_setup = true;

          window.addEventListener('load', function() {
            if (window.assert_equals) // In case of a testharness test.
              return;
            window.testRunner.waitUntilDone();
            const target = document.documentElement;
            if (target != null && target.classList.contains('reftest-wait')) {
              const observer = new MutationObserver(function(mutations) {
                mutations.forEach(function(mutation) {
                  if (!target.classList.contains('reftest-wait')) {
                    window.testRunner.notifyDone();
                  }
                });
              });
              const config = {attributes: true};
              observer.observe(target, config);
            } else {
              document.fonts.ready.then(() => window.testRunner.notifyDone());
            }
          });
        })"));
  }
}

TestRunnerBindings::TestRunnerBindings(
    base::WeakPtr<TestRunner> runner,
    base::WeakPtr<TestRunnerForSpecificView> view_runner)
    : runner_(runner), view_runner_(view_runner) {}

TestRunnerBindings::~TestRunnerBindings() {}

gin::ObjectTemplateBuilder TestRunnerBindings::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<TestRunnerBindings>::GetObjectTemplateBuilder(isolate)
      .SetMethod("abortModal", &TestRunnerBindings::NotImplemented)
      .SetMethod("addDisallowedURL", &TestRunnerBindings::NotImplemented)
      .SetMethod("addOriginAccessAllowListEntry",
                 &TestRunnerBindings::AddOriginAccessAllowListEntry)
      .SetMethod("addWebPageOverlay", &TestRunnerBindings::AddWebPageOverlay)
      .SetMethod("capturePixelsAsyncThen",
                 &TestRunnerBindings::CapturePixelsAsyncThen)
      .SetMethod("clearAllDatabases", &TestRunnerBindings::ClearAllDatabases)
      .SetMethod("clearBackForwardList", &TestRunnerBindings::NotImplemented)
      .SetMethod("clearPrinting", &TestRunnerBindings::ClearPrinting)
      .SetMethod("copyImageAtAndCapturePixelsAsyncThen",
                 &TestRunnerBindings::CopyImageAtAndCapturePixelsAsyncThen)
      .SetMethod("didAcquirePointerLock",
                 &TestRunnerBindings::DidAcquirePointerLock)
      .SetMethod("didLosePointerLock", &TestRunnerBindings::DidLosePointerLock)
      .SetMethod("didNotAcquirePointerLock",
                 &TestRunnerBindings::DidNotAcquirePointerLock)
      .SetMethod("disableAutoResizeMode",
                 &TestRunnerBindings::DisableAutoResizeMode)
      .SetMethod("disableMockScreenOrientation",
                 &TestRunnerBindings::DisableMockScreenOrientation)
      .SetMethod("setDisallowedSubresourcePathSuffixes",
                 &TestRunnerBindings::SetDisallowedSubresourcePathSuffixes)
      .SetMethod("dispatchBeforeInstallPromptEvent",
                 &TestRunnerBindings::DispatchBeforeInstallPromptEvent)
      .SetMethod("dumpAsMarkup", &TestRunnerBindings::DumpAsMarkup)
      .SetMethod("dumpAsText", &TestRunnerBindings::DumpAsText)
      .SetMethod("dumpAsTextWithPixelResults",
                 &TestRunnerBindings::DumpAsTextWithPixelResults)
      .SetMethod("dumpAsLayout", &TestRunnerBindings::DumpAsLayout)
      .SetMethod("dumpAsLayoutWithPixelResults",
                 &TestRunnerBindings::DumpAsLayoutWithPixelResults)
      .SetMethod("dumpBackForwardList",
                 &TestRunnerBindings::DumpBackForwardList)
      .SetMethod("dumpChildFrames", &TestRunnerBindings::DumpChildFrames)
      .SetMethod("dumpCreateView", &TestRunnerBindings::DumpCreateView)
      .SetMethod("dumpDatabaseCallbacks", &TestRunnerBindings::NotImplemented)
      .SetMethod("dumpDragImage", &TestRunnerBindings::DumpDragImage)
      .SetMethod("dumpEditingCallbacks",
                 &TestRunnerBindings::DumpEditingCallbacks)
      .SetMethod("dumpFrameLoadCallbacks",
                 &TestRunnerBindings::DumpFrameLoadCallbacks)
      .SetMethod("dumpIconChanges", &TestRunnerBindings::DumpIconChanges)
      .SetMethod("dumpNavigationPolicy",
                 &TestRunnerBindings::DumpNavigationPolicy)
      .SetMethod("dumpPermissionClientCallbacks",
                 &TestRunnerBindings::DumpPermissionClientCallbacks)
      .SetMethod("dumpPingLoaderCallbacks",
                 &TestRunnerBindings::DumpPingLoaderCallbacks)
      .SetMethod("dumpSelectionRect", &TestRunnerBindings::DumpSelectionRect)
      .SetMethod("dumpSpellCheckCallbacks",
                 &TestRunnerBindings::DumpSpellCheckCallbacks)
      .SetMethod("dumpTitleChanges", &TestRunnerBindings::DumpTitleChanges)
      .SetMethod("dumpUserGestureInFrameLoadCallbacks",
                 &TestRunnerBindings::DumpUserGestureInFrameLoadCallbacks)
      .SetMethod("enableAutoResizeMode",
                 &TestRunnerBindings::EnableAutoResizeMode)
      .SetMethod("enableUseZoomForDSF",
                 &TestRunnerBindings::EnableUseZoomForDSF)
      .SetMethod("evaluateScriptInIsolatedWorld",
                 &TestRunnerBindings::EvaluateScriptInIsolatedWorld)
      .SetMethod(
          "evaluateScriptInIsolatedWorldAndReturnValue",
          &TestRunnerBindings::EvaluateScriptInIsolatedWorldAndReturnValue)
      .SetMethod("execCommand", &TestRunnerBindings::ExecCommand)
      .SetMethod("findString", &TestRunnerBindings::FindString)
      .SetMethod("forceNextDrawingBufferCreationToFail",
                 &TestRunnerBindings::ForceNextDrawingBufferCreationToFail)
      .SetMethod("forceNextWebGLContextCreationToFail",
                 &TestRunnerBindings::ForceNextWebGLContextCreationToFail)
      .SetMethod("forceRedSelectionColors",
                 &TestRunnerBindings::ForceRedSelectionColors)

      // The Bluetooth functions are specified at
      // https://webbluetoothcg.github.io/web-bluetooth/tests/.
      .SetMethod("getBluetoothManualChooserEvents",
                 &TestRunnerBindings::GetBluetoothManualChooserEvents)
      .SetMethod("getManifestThen", &TestRunnerBindings::GetManifestThen)
      .SetMethod("hasCustomPageSizeStyle",
                 &TestRunnerBindings::HasCustomPageSizeStyle)
      .SetMethod("insertStyleSheet", &TestRunnerBindings::InsertStyleSheet)
      .SetMethod("isChooserShown", &TestRunnerBindings::IsChooserShown)
      .SetMethod("isCommandEnabled", &TestRunnerBindings::IsCommandEnabled)
      .SetMethod("keepWebHistory", &TestRunnerBindings::NotImplemented)
      .SetMethod("updateAllLifecyclePhasesAndComposite",
                 &TestRunnerBindings::UpdateAllLifecyclePhasesAndComposite)
      .SetMethod("updateAllLifecyclePhasesAndCompositeThen",
                 &TestRunnerBindings::UpdateAllLifecyclePhasesAndCompositeThen)
      .SetMethod("setAnimationRequiresRaster",
                 &TestRunnerBindings::SetAnimationRequiresRaster)
      .SetMethod("logToStderr", &TestRunnerBindings::LogToStderr)
      .SetMethod("notifyDone", &TestRunnerBindings::NotifyDone)
      .SetMethod("overridePreference", &TestRunnerBindings::OverridePreference)
      .SetMethod("pathToLocalResource",
                 &TestRunnerBindings::PathToLocalResource)
      .SetProperty("platformName", &TestRunnerBindings::PlatformName)
      .SetMethod("queueBackNavigation",
                 &TestRunnerBindings::QueueBackNavigation)
      .SetMethod("queueForwardNavigation",
                 &TestRunnerBindings::QueueForwardNavigation)
      .SetMethod("queueLoad", &TestRunnerBindings::QueueLoad)
      .SetMethod("queueLoadingScript", &TestRunnerBindings::QueueLoadingScript)
      .SetMethod("queueNonLoadingScript",
                 &TestRunnerBindings::QueueNonLoadingScript)
      .SetMethod("queueReload", &TestRunnerBindings::QueueReload)
      .SetMethod("removeSpellCheckResolvedCallback",
                 &TestRunnerBindings::RemoveSpellCheckResolvedCallback)
      .SetMethod("removeWebPageOverlay",
                 &TestRunnerBindings::RemoveWebPageOverlay)
      .SetMethod("resetTestHelperControllers",
                 &TestRunnerBindings::ResetTestHelperControllers)
      .SetMethod("resolveBeforeInstallPromptPromise",
                 &TestRunnerBindings::ResolveBeforeInstallPromptPromise)
      .SetMethod("runIdleTasks", &TestRunnerBindings::RunIdleTasks)
      .SetMethod("selectionAsMarkup", &TestRunnerBindings::SelectionAsMarkup)

      // The Bluetooth functions are specified at
      // https://webbluetoothcg.github.io/web-bluetooth/tests/.
      .SetMethod("sendBluetoothManualChooserEvent",
                 &TestRunnerBindings::SendBluetoothManualChooserEvent)
      .SetMethod("setAcceptLanguages", &TestRunnerBindings::SetAcceptLanguages)
      .SetMethod("setAllowFileAccessFromFileURLs",
                 &TestRunnerBindings::SetAllowFileAccessFromFileURLs)
      .SetMethod("setAllowRunningOfInsecureContent",
                 &TestRunnerBindings::SetAllowRunningOfInsecureContent)
      .SetMethod("setAutoplayAllowed", &TestRunnerBindings::SetAutoplayAllowed)
      .SetMethod("setBlockThirdPartyCookies",
                 &TestRunnerBindings::SetBlockThirdPartyCookies)
      .SetMethod("setAudioData", &TestRunnerBindings::SetAudioData)
      .SetMethod("setBackingScaleFactor",
                 &TestRunnerBindings::SetBackingScaleFactor)
      // The Bluetooth functions are specified at
      // https://webbluetoothcg.github.io/web-bluetooth/tests/.
      .SetMethod("setBluetoothFakeAdapter",
                 &TestRunnerBindings::SetBluetoothFakeAdapter)
      .SetMethod("setBluetoothManualChooser",
                 &TestRunnerBindings::SetBluetoothManualChooser)
      .SetMethod("setCallCloseOnWebViews", &TestRunnerBindings::NotImplemented)
      .SetMethod("setCanOpenWindows", &TestRunnerBindings::SetCanOpenWindows)
      .SetMethod("setCloseRemainingWindowsWhenComplete",
                 &TestRunnerBindings::SetCloseRemainingWindowsWhenComplete)
      .SetMethod("setColorProfile", &TestRunnerBindings::SetColorProfile)
      .SetMethod("setCustomPolicyDelegate",
                 &TestRunnerBindings::SetCustomPolicyDelegate)
      .SetMethod("setCustomTextOutput",
                 &TestRunnerBindings::SetCustomTextOutput)
      .SetMethod("setDatabaseQuota", &TestRunnerBindings::SetDatabaseQuota)
      .SetMethod("setDomainRelaxationForbiddenForURLScheme",
                 &TestRunnerBindings::SetDomainRelaxationForbiddenForURLScheme)
      .SetMethod("setDumpConsoleMessages",
                 &TestRunnerBindings::SetDumpConsoleMessages)
      .SetMethod("setDumpJavaScriptDialogs",
                 &TestRunnerBindings::SetDumpJavaScriptDialogs)
      .SetMethod("setEffectiveConnectionType",
                 &TestRunnerBindings::SetEffectiveConnectionType)
      .SetMethod("setHighlightAds", &TestRunnerBindings::SetHighlightAds)
      .SetMethod("setMockSpellCheckerEnabled",
                 &TestRunnerBindings::SetMockSpellCheckerEnabled)
      .SetMethod("setIconDatabaseEnabled", &TestRunnerBindings::NotImplemented)
      .SetMethod("setImagesAllowed", &TestRunnerBindings::SetImagesAllowed)
      .SetMethod("setIsolatedWorldInfo",
                 &TestRunnerBindings::SetIsolatedWorldInfo)
      .SetMethod("setJavaScriptCanAccessClipboard",
                 &TestRunnerBindings::SetJavaScriptCanAccessClipboard)
      .SetMethod("setMainFrameIsFirstResponder",
                 &TestRunnerBindings::NotImplemented)
      .SetMethod("setMockScreenOrientation",
                 &TestRunnerBindings::SetMockScreenOrientation)
      .SetMethod("setPOSIXLocale", &TestRunnerBindings::SetPOSIXLocale)
      .SetMethod("setPageVisibility", &TestRunnerBindings::SetPageVisibility)
      .SetMethod("setPermission", &TestRunnerBindings::SetPermission)
      .SetMethod("setPluginsAllowed", &TestRunnerBindings::SetPluginsAllowed)
      .SetMethod("setPluginsEnabled", &TestRunnerBindings::SetPluginsEnabled)
      .SetMethod("setPointerLockWillFailSynchronously",
                 &TestRunnerBindings::SetPointerLockWillFailSynchronously)
      .SetMethod("setPointerLockWillRespondAsynchronously",
                 &TestRunnerBindings::SetPointerLockWillRespondAsynchronously)
      .SetMethod("setPopupBlockingEnabled",
                 &TestRunnerBindings::SetPopupBlockingEnabled)
      .SetMethod("setPrinting", &TestRunnerBindings::SetPrinting)
      .SetMethod("setPrintingForFrame",
                 &TestRunnerBindings::SetPrintingForFrame)
      .SetMethod("setScriptsAllowed", &TestRunnerBindings::SetScriptsAllowed)
      .SetMethod("setScrollbarPolicy", &TestRunnerBindings::NotImplemented)
      .SetMethod("setShouldGeneratePixelResults",
                 &TestRunnerBindings::SetShouldGeneratePixelResults)
      .SetMethod(
          "setShouldStayOnPageAfterHandlingBeforeUnload",
          &TestRunnerBindings::SetShouldStayOnPageAfterHandlingBeforeUnload)
      .SetMethod("setSpellCheckResolvedCallback",
                 &TestRunnerBindings::SetSpellCheckResolvedCallback)
      .SetMethod("setStorageAllowed", &TestRunnerBindings::SetStorageAllowed)
      .SetMethod("setTabKeyCyclesThroughElements",
                 &TestRunnerBindings::SetTabKeyCyclesThroughElements)
      .SetMethod("setTextDirection", &TestRunnerBindings::SetTextDirection)
      .SetMethod("setTextSubpixelPositioning",
                 &TestRunnerBindings::SetTextSubpixelPositioning)
      .SetMethod("setUseDashboardCompatibilityMode",
                 &TestRunnerBindings::NotImplemented)
      .SetMethod("setViewSourceForFrame",
                 &TestRunnerBindings::SetViewSourceForFrame)
      .SetMethod("setWillSendRequestClearHeader",
                 &TestRunnerBindings::SetWillSendRequestClearHeader)
      .SetMethod("setWindowIsKey", &TestRunnerBindings::SetWindowIsKey)
      .SetMethod("navigateSecondaryWindow",
                 &TestRunnerBindings::NavigateSecondaryWindow)
      .SetMethod("inspectSecondaryWindow",
                 &TestRunnerBindings::InspectSecondaryWindow)
      .SetMethod("simulateWebNotificationClick",
                 &TestRunnerBindings::SimulateWebNotificationClick)
      .SetMethod("simulateWebNotificationClose",
                 &TestRunnerBindings::SimulateWebNotificationClose)
      .SetMethod("simulateWebContentIndexDelete",
                 &TestRunnerBindings::SimulateWebContentIndexDelete)
      .SetProperty("tooltipText", &TestRunnerBindings::TooltipText)
      .SetMethod("useUnfortunateSynchronousResizeMode",
                 &TestRunnerBindings::UseUnfortunateSynchronousResizeMode)
      .SetMethod("waitForPolicyDelegate",
                 &TestRunnerBindings::WaitForPolicyDelegate)
      .SetMethod("waitUntilDone", &TestRunnerBindings::WaitUntilDone)
      .SetMethod("waitUntilExternalURLLoad",
                 &TestRunnerBindings::WaitUntilExternalURLLoad)

      // webHistoryItemCount is used by tests in web_tests\http\tests\history
      .SetProperty("webHistoryItemCount",
                   &TestRunnerBindings::WebHistoryItemCount)
      .SetMethod("windowCount", &TestRunnerBindings::WindowCount);
}

void TestRunnerBindings::LogToStderr(const std::string& output) {
  TRACE_EVENT1("shell", "TestRunner::LogToStderr", "output", output);
  LOG(ERROR) << output;
}

void TestRunnerBindings::NotifyDone() {
  if (runner_)
    runner_->NotifyDone();
}

void TestRunnerBindings::WaitUntilDone() {
  if (runner_)
    runner_->WaitUntilDone();
}

void TestRunnerBindings::QueueBackNavigation(int how_far_back) {
  if (runner_)
    runner_->QueueBackNavigation(how_far_back);
}

void TestRunnerBindings::QueueForwardNavigation(int how_far_forward) {
  if (runner_)
    runner_->QueueForwardNavigation(how_far_forward);
}

void TestRunnerBindings::QueueReload() {
  if (runner_)
    runner_->QueueReload();
}

void TestRunnerBindings::QueueLoadingScript(const std::string& script) {
  if (runner_)
    runner_->QueueLoadingScript(script);
}

void TestRunnerBindings::QueueNonLoadingScript(const std::string& script) {
  if (runner_)
    runner_->QueueNonLoadingScript(script);
}

void TestRunnerBindings::QueueLoad(gin::Arguments* args) {
  if (runner_) {
    std::string url;
    std::string target;
    args->GetNext(&url);
    args->GetNext(&target);
    runner_->QueueLoad(url, target);
  }
}

void TestRunnerBindings::SetCustomPolicyDelegate(gin::Arguments* args) {
  if (runner_)
    runner_->SetCustomPolicyDelegate(args);
}

void TestRunnerBindings::WaitForPolicyDelegate() {
  if (runner_)
    runner_->WaitForPolicyDelegate();
}

int TestRunnerBindings::WindowCount() {
  if (runner_)
    return runner_->WindowCount();
  return 0;
}

void TestRunnerBindings::SetCloseRemainingWindowsWhenComplete(
    gin::Arguments* args) {
  if (!runner_)
    return;

  // In the original implementation, nothing happens if the argument is
  // ommitted.
  bool close_remaining_windows = false;
  if (args->GetNext(&close_remaining_windows))
    runner_->SetCloseRemainingWindowsWhenComplete(close_remaining_windows);
}

void TestRunnerBindings::ResetTestHelperControllers() {
  if (runner_)
    runner_->ResetTestHelperControllers();
}

void TestRunnerBindings::SetTabKeyCyclesThroughElements(
    bool tab_key_cycles_through_elements) {
  if (view_runner_)
    view_runner_->SetTabKeyCyclesThroughElements(
        tab_key_cycles_through_elements);
}

void TestRunnerBindings::ExecCommand(gin::Arguments* args) {
  if (view_runner_)
    view_runner_->ExecCommand(args);
}

bool TestRunnerBindings::IsCommandEnabled(const std::string& command) {
  if (view_runner_)
    return view_runner_->IsCommandEnabled(command);
  return false;
}

void TestRunnerBindings::SetDomainRelaxationForbiddenForURLScheme(
    bool forbidden,
    const std::string& scheme) {
  if (view_runner_)
    view_runner_->SetDomainRelaxationForbiddenForURLScheme(forbidden, scheme);
}

void TestRunnerBindings::SetDumpConsoleMessages(bool enabled) {
  if (runner_)
    runner_->SetDumpConsoleMessages(enabled);
}

void TestRunnerBindings::SetDumpJavaScriptDialogs(bool enabled) {
  if (runner_)
    runner_->SetDumpJavaScriptDialogs(enabled);
}

void TestRunnerBindings::SetEffectiveConnectionType(
    const std::string& connection_type) {
  blink::WebEffectiveConnectionType web_type =
      blink::WebEffectiveConnectionType::kTypeUnknown;
  if (connection_type == "TypeUnknown")
    web_type = blink::WebEffectiveConnectionType::kTypeUnknown;
  else if (connection_type == "TypeOffline")
    web_type = blink::WebEffectiveConnectionType::kTypeOffline;
  else if (connection_type == "TypeSlow2G")
    web_type = blink::WebEffectiveConnectionType::kTypeSlow2G;
  else if (connection_type == "Type2G")
    web_type = blink::WebEffectiveConnectionType::kType2G;
  else if (connection_type == "Type3G")
    web_type = blink::WebEffectiveConnectionType::kType3G;
  else if (connection_type == "Type4G")
    web_type = blink::WebEffectiveConnectionType::kType4G;
  else
    NOTREACHED();

  if (runner_)
    runner_->SetEffectiveConnectionType(web_type);
}

void TestRunnerBindings::SetMockSpellCheckerEnabled(bool enabled) {
  if (runner_)
    runner_->SetMockSpellCheckerEnabled(enabled);
}

void TestRunnerBindings::SetSpellCheckResolvedCallback(
    v8::Local<v8::Function> callback) {
  if (runner_)
    runner_->spellcheck_->SetSpellCheckResolvedCallback(callback);
}

void TestRunnerBindings::RemoveSpellCheckResolvedCallback() {
  if (runner_)
    runner_->spellcheck_->RemoveSpellCheckResolvedCallback();
}

v8::Local<v8::Value>
TestRunnerBindings::EvaluateScriptInIsolatedWorldAndReturnValue(
    int world_id,
    const std::string& script) {
  if (!view_runner_ || world_id <= 0 || world_id >= (1 << 29))
    return v8::Local<v8::Value>();
  return view_runner_->EvaluateScriptInIsolatedWorldAndReturnValue(world_id,
                                                                   script);
}

void TestRunnerBindings::EvaluateScriptInIsolatedWorld(
    int world_id,
    const std::string& script) {
  if (view_runner_ && world_id > 0 && world_id < (1 << 29))
    view_runner_->EvaluateScriptInIsolatedWorld(world_id, script);
}

void TestRunnerBindings::SetIsolatedWorldInfo(
    int world_id,
    v8::Local<v8::Value> security_origin,
    v8::Local<v8::Value> content_security_policy) {
  if (view_runner_) {
    view_runner_->SetIsolatedWorldInfo(world_id, security_origin,
                                       content_security_policy);
  }
}

void TestRunnerBindings::AddOriginAccessAllowListEntry(
    const std::string& source_origin,
    const std::string& destination_protocol,
    const std::string& destination_host,
    bool allow_destination_subdomains) {
  if (runner_) {
    // Non-standard schemes should be added to the scheme registeries to use
    // for the origin access whitelisting.
    GURL url(source_origin);
    DCHECK(url.is_valid());
    DCHECK(url.has_scheme());
    DCHECK(url.has_host());

    runner_->AddOriginAccessAllowListEntry(source_origin, destination_protocol,
                                           destination_host,
                                           allow_destination_subdomains);
  }
}

bool TestRunnerBindings::HasCustomPageSizeStyle(int page_index) {
  if (view_runner_)
    return view_runner_->HasCustomPageSizeStyle(page_index);
  return false;
}

void TestRunnerBindings::ForceRedSelectionColors() {
  if (view_runner_)
    view_runner_->ForceRedSelectionColors();
}

void TestRunnerBindings::InsertStyleSheet(const std::string& source_code) {
  if (runner_)
    runner_->InsertStyleSheet(source_code);
}

bool TestRunnerBindings::FindString(
    const std::string& search_text,
    const std::vector<std::string>& options_array) {
  if (view_runner_)
    return view_runner_->FindString(search_text, options_array);
  return false;
}

std::string TestRunnerBindings::SelectionAsMarkup() {
  if (view_runner_)
    return view_runner_->SelectionAsMarkup();
  return std::string();
}

void TestRunnerBindings::SetTextSubpixelPositioning(bool value) {
  if (runner_)
    runner_->SetTextSubpixelPositioning(value);
}

void TestRunnerBindings::SetPageVisibility(const std::string& new_visibility) {
  if (view_runner_)
    view_runner_->SetPageVisibility(new_visibility);
}

void TestRunnerBindings::SetTextDirection(const std::string& direction_name) {
  if (view_runner_)
    view_runner_->SetTextDirection(direction_name);
}

void TestRunnerBindings::UseUnfortunateSynchronousResizeMode() {
  if (runner_)
    runner_->UseUnfortunateSynchronousResizeMode();
}

void TestRunnerBindings::EnableAutoResizeMode(int min_width,
                                              int min_height,
                                              int max_width,
                                              int max_height) {
  if (runner_)
    runner_->EnableAutoResizeMode(min_width, min_height, max_width, max_height);
}

void TestRunnerBindings::DisableAutoResizeMode(int new_width, int new_height) {
  if (runner_)
    return runner_->DisableAutoResizeMode(new_width, new_height);
}

void TestRunnerBindings::SetMockScreenOrientation(
    const std::string& orientation) {
  if (!runner_)
    return;

  runner_->SetMockScreenOrientation(orientation);
}

void TestRunnerBindings::DisableMockScreenOrientation() {
  if (runner_)
    runner_->DisableMockScreenOrientation();
}

void TestRunnerBindings::SetDisallowedSubresourcePathSuffixes(
    const std::vector<std::string>& suffixes,
    bool block_subresources) {
  if (runner_)
    runner_->SetDisallowedSubresourcePathSuffixes(suffixes, block_subresources);
}

void TestRunnerBindings::DidAcquirePointerLock() {
  if (view_runner_)
    view_runner_->DidAcquirePointerLock();
}

void TestRunnerBindings::DidNotAcquirePointerLock() {
  if (view_runner_)
    view_runner_->DidNotAcquirePointerLock();
}

void TestRunnerBindings::DidLosePointerLock() {
  if (view_runner_)
    view_runner_->DidLosePointerLock();
}

void TestRunnerBindings::SetPointerLockWillFailSynchronously() {
  if (view_runner_)
    view_runner_->SetPointerLockWillFailSynchronously();
}

void TestRunnerBindings::SetPointerLockWillRespondAsynchronously() {
  if (view_runner_)
    view_runner_->SetPointerLockWillRespondAsynchronously();
}

void TestRunnerBindings::SetPopupBlockingEnabled(bool block_popups) {
  if (runner_)
    runner_->SetPopupBlockingEnabled(block_popups);
}

void TestRunnerBindings::SetJavaScriptCanAccessClipboard(bool can_access) {
  if (runner_)
    runner_->SetJavaScriptCanAccessClipboard(can_access);
}

void TestRunnerBindings::SetAllowFileAccessFromFileURLs(bool allow) {
  if (runner_)
    runner_->SetAllowFileAccessFromFileURLs(allow);
}

void TestRunnerBindings::OverridePreference(gin::Arguments* args) {
  if (runner_)
    runner_->OverridePreference(args);
}

void TestRunnerBindings::SetAcceptLanguages(
    const std::string& accept_languages) {
  if (!runner_)
    return;

  runner_->SetAcceptLanguages(accept_languages);
}

void TestRunnerBindings::SetPluginsEnabled(bool enabled) {
  if (runner_)
    runner_->SetPluginsEnabled(enabled);
}

void TestRunnerBindings::DumpEditingCallbacks() {
  if (runner_)
    runner_->DumpEditingCallbacks();
}

void TestRunnerBindings::DumpAsMarkup() {
  if (runner_)
    runner_->DumpAsMarkup();
}

void TestRunnerBindings::DumpAsText() {
  if (runner_)
    runner_->DumpAsText();
}

void TestRunnerBindings::DumpAsTextWithPixelResults() {
  if (runner_)
    runner_->DumpAsTextWithPixelResults();
}

void TestRunnerBindings::DumpAsLayout() {
  if (runner_)
    runner_->DumpAsLayout();
}

void TestRunnerBindings::DumpAsLayoutWithPixelResults() {
  if (runner_)
    runner_->DumpAsLayoutWithPixelResults();
}

void TestRunnerBindings::DumpChildFrames() {
  if (runner_)
    runner_->DumpChildFrames();
}

void TestRunnerBindings::DumpIconChanges() {
  if (runner_)
    runner_->DumpIconChanges();
}

void TestRunnerBindings::SetAudioData(const gin::ArrayBufferView& view) {
  if (runner_)
    runner_->SetAudioData(view);
}

void TestRunnerBindings::DumpFrameLoadCallbacks() {
  if (runner_)
    runner_->DumpFrameLoadCallbacks();
}

void TestRunnerBindings::DumpPingLoaderCallbacks() {
  if (runner_)
    runner_->DumpPingLoaderCallbacks();
}

void TestRunnerBindings::DumpUserGestureInFrameLoadCallbacks() {
  if (runner_)
    runner_->DumpUserGestureInFrameLoadCallbacks();
}

void TestRunnerBindings::DumpTitleChanges() {
  if (runner_)
    runner_->DumpTitleChanges();
}

void TestRunnerBindings::DumpCreateView() {
  if (runner_)
    runner_->DumpCreateView();
}

void TestRunnerBindings::SetCanOpenWindows() {
  if (runner_)
    runner_->SetCanOpenWindows();
}

void TestRunnerBindings::SetImagesAllowed(bool allowed) {
  if (runner_)
    runner_->SetImagesAllowed(allowed);
}

void TestRunnerBindings::SetScriptsAllowed(bool allowed) {
  if (runner_)
    runner_->SetScriptsAllowed(allowed);
}

void TestRunnerBindings::SetStorageAllowed(bool allowed) {
  if (runner_)
    runner_->SetStorageAllowed(allowed);
}

void TestRunnerBindings::SetPluginsAllowed(bool allowed) {
  if (runner_)
    runner_->SetPluginsAllowed(allowed);
}

void TestRunnerBindings::SetAllowRunningOfInsecureContent(bool allowed) {
  if (runner_)
    runner_->SetAllowRunningOfInsecureContent(allowed);
}

void TestRunnerBindings::SetAutoplayAllowed(bool allowed) {
  if (runner_)
    runner_->SetAutoplayAllowed(allowed);
}

void TestRunnerBindings::DumpPermissionClientCallbacks() {
  if (runner_)
    runner_->DumpPermissionClientCallbacks();
}

void TestRunnerBindings::DumpSpellCheckCallbacks() {
  if (runner_)
    runner_->DumpSpellCheckCallbacks();
}

void TestRunnerBindings::DumpBackForwardList() {
  if (runner_)
    runner_->DumpBackForwardList();
}

void TestRunnerBindings::DumpSelectionRect() {
  if (runner_)
    runner_->DumpSelectionRect();
}

void TestRunnerBindings::SetPrinting() {
  if (runner_)
    runner_->SetPrinting();
}

void TestRunnerBindings::SetPrintingForFrame(const std::string& frame_name) {
  if (runner_)
    runner_->SetPrintingForFrame(frame_name);
}

void TestRunnerBindings::ClearPrinting() {
  if (runner_)
    runner_->ClearPrinting();
}

void TestRunnerBindings::SetShouldGeneratePixelResults(bool value) {
  if (runner_)
    runner_->SetShouldGeneratePixelResults(value);
}

void TestRunnerBindings::SetShouldStayOnPageAfterHandlingBeforeUnload(
    bool value) {
  if (runner_)
    runner_->SetShouldStayOnPageAfterHandlingBeforeUnload(value);
}

void TestRunnerBindings::SetWillSendRequestClearHeader(
    const std::string& header) {
  if (runner_)
    runner_->SetWillSendRequestClearHeader(header);
}

void TestRunnerBindings::WaitUntilExternalURLLoad() {
  if (runner_)
    runner_->WaitUntilExternalURLLoad();
}

void TestRunnerBindings::DumpDragImage() {
  if (runner_)
    runner_->DumpDragImage();
}

void TestRunnerBindings::DumpNavigationPolicy() {
  if (runner_)
    runner_->DumpNavigationPolicy();
}

void TestRunnerBindings::NavigateSecondaryWindow(const std::string& url) {
  if (runner_)
    runner_->NavigateSecondaryWindow(GURL(url));
}

void TestRunnerBindings::InspectSecondaryWindow() {
  if (runner_)
    runner_->InspectSecondaryWindow();
}

bool TestRunnerBindings::IsChooserShown() {
  if (runner_)
    return runner_->IsChooserShown();
  return false;
}

void TestRunnerBindings::ClearAllDatabases() {
  if (runner_)
    runner_->ClearAllDatabases();
}

void TestRunnerBindings::SetDatabaseQuota(int quota) {
  if (runner_)
    runner_->SetDatabaseQuota(quota);
}

void TestRunnerBindings::SetBlockThirdPartyCookies(bool block) {
  if (runner_)
    runner_->SetBlockThirdPartyCookies(block);
}

void TestRunnerBindings::SetWindowIsKey(bool value) {
  if (view_runner_)
    view_runner_->SetWindowIsKey(value);
}

std::string TestRunnerBindings::PathToLocalResource(const std::string& path) {
  if (runner_)
    return runner_->PathToLocalResource(path);
  return std::string();
}

void TestRunnerBindings::SetBackingScaleFactor(
    double value,
    v8::Local<v8::Function> callback) {
  // Limit backing scale factor to something low - 15x. Without
  // this limit, arbitrarily large values can be used, which can lead to
  // crashes and other problems. Examples of problems: gfx::Size::GetCheckedArea
  // crashes with a size which overflows int; GLES2DecoderImpl::TexStorageImpl
  // fails with "dimensions out of range"; GL ERROR :GL_OUT_OF_MEMORY.
  // See https://crbug.com/899482 or https://crbug.com/900271
  double limited_value = fmin(15, value);
  if (view_runner_)
    view_runner_->SetBackingScaleFactor(limited_value, callback);
}

void TestRunnerBindings::EnableUseZoomForDSF(v8::Local<v8::Function> callback) {
  if (view_runner_)
    view_runner_->EnableUseZoomForDSF(callback);
}

void TestRunnerBindings::SetColorProfile(const std::string& name,
                                         v8::Local<v8::Function> callback) {
  if (view_runner_)
    view_runner_->SetColorProfile(name, callback);
}

void TestRunnerBindings::SetBluetoothFakeAdapter(
    const std::string& adapter_name,
    v8::Local<v8::Function> callback) {
  if (view_runner_)
    view_runner_->SetBluetoothFakeAdapter(adapter_name, callback);
}

void TestRunnerBindings::SetBluetoothManualChooser(bool enable) {
  if (view_runner_)
    view_runner_->SetBluetoothManualChooser(enable);
}

void TestRunnerBindings::GetBluetoothManualChooserEvents(
    v8::Local<v8::Function> callback) {
  if (view_runner_)
    return view_runner_->GetBluetoothManualChooserEvents(callback);
}

void TestRunnerBindings::SendBluetoothManualChooserEvent(
    const std::string& event,
    const std::string& argument) {
  if (view_runner_)
    view_runner_->SendBluetoothManualChooserEvent(event, argument);
}

void TestRunnerBindings::SetPOSIXLocale(const std::string& locale) {
  if (runner_)
    runner_->SetPOSIXLocale(locale);
}

void TestRunnerBindings::SimulateWebNotificationClick(gin::Arguments* args) {
  DCHECK_GE(args->Length(), 1);
  if (!runner_)
    return;

  std::string title;
  base::Optional<int> action_index;
  base::Optional<base::string16> reply;

  if (!args->GetNext(&title)) {
    args->ThrowError();
    return;
  }

  // Optional |action_index| argument.
  if (args->Length() >= 2) {
    int action_index_int;
    if (!args->GetNext(&action_index_int)) {
      args->ThrowError();
      return;
    }

    action_index = action_index_int;
  }

  // Optional |reply| argument.
  if (args->Length() >= 3) {
    std::string reply_string;
    if (!args->GetNext(&reply_string)) {
      args->ThrowError();
      return;
    }

    reply = base::UTF8ToUTF16(reply_string);
  }

  runner_->SimulateWebNotificationClick(title, action_index, reply);
}

void TestRunnerBindings::SimulateWebNotificationClose(const std::string& title,
                                                      bool by_user) {
  if (!runner_)
    return;
  runner_->SimulateWebNotificationClose(title, by_user);
}

void TestRunnerBindings::SimulateWebContentIndexDelete(const std::string& id) {
  if (!runner_)
    return;
  runner_->SimulateWebContentIndexDelete(id);
}

void TestRunnerBindings::SetHighlightAds() {
  if (view_runner_)
    view_runner_->SetHighlightAds(true);
}

void TestRunnerBindings::AddWebPageOverlay() {
  if (view_runner_)
    view_runner_->AddWebPageOverlay();
}

void TestRunnerBindings::RemoveWebPageOverlay() {
  if (view_runner_)
    view_runner_->RemoveWebPageOverlay();
}

void TestRunnerBindings::UpdateAllLifecyclePhasesAndComposite() {
  if (view_runner_)
    view_runner_->UpdateAllLifecyclePhasesAndComposite();
}

void TestRunnerBindings::UpdateAllLifecyclePhasesAndCompositeThen(
    v8::Local<v8::Function> callback) {
  if (view_runner_)
    view_runner_->UpdateAllLifecyclePhasesAndCompositeThen(callback);
}

void TestRunnerBindings::SetAnimationRequiresRaster(bool do_raster) {
  if (!runner_)
    return;
  runner_->SetAnimationRequiresRaster(do_raster);
}

void TestRunnerBindings::GetManifestThen(v8::Local<v8::Function> callback) {
  if (view_runner_)
    view_runner_->GetManifestThen(callback);
}

void TestRunnerBindings::CapturePixelsAsyncThen(
    v8::Local<v8::Function> callback) {
  if (view_runner_)
    view_runner_->CapturePixelsAsyncThen(callback);
}

void TestRunnerBindings::CopyImageAtAndCapturePixelsAsyncThen(
    int x,
    int y,
    v8::Local<v8::Function> callback) {
  if (view_runner_)
    view_runner_->CopyImageAtAndCapturePixelsAsyncThen(x, y, callback);
}

void TestRunnerBindings::SetCustomTextOutput(const std::string& output) {
  if (runner_)
    runner_->SetCustomTextOutput(output);
}

void TestRunnerBindings::SetViewSourceForFrame(const std::string& name,
                                               bool enabled) {
  if (view_runner_)
    view_runner_->SetViewSourceForFrame(name, enabled);
}

void TestRunnerBindings::SetPermission(const std::string& name,
                                       const std::string& value,
                                       const std::string& origin,
                                       const std::string& embedding_origin) {
  if (!runner_)
    return;

  return runner_->SetPermission(name, value, GURL(origin),
                                GURL(embedding_origin));
}

void TestRunnerBindings::DispatchBeforeInstallPromptEvent(
    const std::vector<std::string>& event_platforms,
    v8::Local<v8::Function> callback) {
  if (!view_runner_)
    return;

  return view_runner_->DispatchBeforeInstallPromptEvent(event_platforms,
                                                        callback);
}

void TestRunnerBindings::ResolveBeforeInstallPromptPromise(
    const std::string& platform) {
  if (!runner_)
    return;

  runner_->ResolveBeforeInstallPromptPromise(platform);
}

void TestRunnerBindings::RunIdleTasks(v8::Local<v8::Function> callback) {
  if (!view_runner_)
    return;
  view_runner_->RunIdleTasks(callback);
}

std::string TestRunnerBindings::PlatformName() {
  if (runner_)
    return runner_->platform_name_;
  return std::string();
}

std::string TestRunnerBindings::TooltipText() {
  if (runner_)
    return runner_->tooltip_text_;
  return std::string();
}

int TestRunnerBindings::WebHistoryItemCount() {
  if (runner_)
    return runner_->web_history_item_count_;
  return false;
}

void TestRunnerBindings::ForceNextWebGLContextCreationToFail() {
  if (view_runner_)
    view_runner_->ForceNextWebGLContextCreationToFail();
}

void TestRunnerBindings::ForceNextDrawingBufferCreationToFail() {
  if (view_runner_)
    view_runner_->ForceNextDrawingBufferCreationToFail();
}

void TestRunnerBindings::NotImplemented(const gin::Arguments& args) {}

TestRunner::WorkQueue::WorkQueue(TestRunner* controller)
    : controller_(controller) {}

TestRunner::WorkQueue::~WorkQueue() {
  Reset();
}

void TestRunner::WorkQueue::ProcessWorkSoon() {
  // We delay processing queued work to avoid recursion problems, and to avoid
  // running tasks in the middle of a navigation call stack, where blink and
  // content may have inconsistent states halfway through being updated.
  controller_->delegate_->PostTask(base::BindOnce(
      &TestRunner::WorkQueue::ProcessWork, weak_factory_.GetWeakPtr()));
}

void TestRunner::WorkQueue::Reset() {
  frozen_ = false;
  finished_loading_ = false;
  while (!queue_.empty()) {
    delete queue_.front();
    queue_.pop_front();
  }
}

void TestRunner::WorkQueue::AddWork(WorkItem* work) {
  if (frozen_) {
    delete work;
    return;
  }
  queue_.push_back(work);
}

void TestRunner::WorkQueue::ProcessWork() {
  if (!controller_->main_view_)
    return;

  while (!queue_.empty()) {
    finished_loading_ = false;  // Watch for loading finishing inside Run().
    bool started_load =
        queue_.front()->Run(controller_->delegate_, controller_->main_view_);
    delete queue_.front();
    queue_.pop_front();

    if (started_load) {
      // If a load started, and didn't complete inside of Run(), then mark
      // the load as running.
      if (!finished_loading_)
        controller_->running_load_ = true;

      // Quit doing work once a load is in progress.
      //
      // TODO(danakj): We could avoid the post-task of ProcessWork() by not
      // early-outting here if |finished_loading_|. Since load finished we could
      // keep running work. And in RemoveLoadingFrame() instead of calling
      // ProcessWorkSoon() unconditionally, only call it if we're not already
      // inside ProcessWork().
      return;
    }
  }

  // If there was no navigation stated, there may be no more tasks in the
  // system. We can safely finish the test here as we're not in the middle
  // of a navigation call stack, and ProcessWork() was a posted task.
  controller_->FinishTestIfReady();
}

TestRunner::TestRunner(TestInterfaces* interfaces)
    : work_queue_(this),
      test_interfaces_(interfaces),
      mock_content_settings_client_(std::make_unique<MockContentSettingsClient>(
          &web_test_runtime_flags_)),
      spellcheck_(std::make_unique<SpellCheckClient>(this)) {}

TestRunner::~TestRunner() = default;

void TestRunner::Install(
    blink::WebLocalFrame* frame,
    base::WeakPtr<TestRunnerForSpecificView> view_test_runner) {
  // In WPT, only reftests generate pixel results.
  TestRunnerBindings::Install(weak_factory_.GetWeakPtr(), view_test_runner,
                              frame, is_web_platform_tests_mode(),
                              IsFramePartOfMainTestWindow(frame));
  mock_screen_orientation_client_.OverrideAssociatedInterfaceProviderForFrame(
      frame);
}

void TestRunner::SetDelegate(WebTestDelegate* delegate) {
  delegate_ = delegate;
  mock_content_settings_client_->SetDelegate(delegate);
  spellcheck_->SetDelegate(delegate);
}

void TestRunner::SetMainView(blink::WebView* web_view) {
  main_view_ = web_view;
  if (disable_v8_cache_)
    SetV8CacheDisabled(true);
}

void TestRunner::Reset() {
  is_web_platform_tests_mode_ = false;
  loading_frames_.clear();
  web_test_runtime_flags_.Reset();
  mock_screen_orientation_client_.ResetData();
  mock_content_settings_client_->ResetClientHintsPersistencyData();
  drag_image_.reset();

  blink::WebSecurityPolicy::ClearOriginAccessList();
#if defined(OS_LINUX) || defined(OS_FUCHSIA)
  blink::WebFontRenderStyle::SetSubpixelPositioning(false);
#endif

  if (delegate_) {
    // Reset the default quota for each origin.
    delegate_->SetDatabaseQuota(kDefaultDatabaseQuota);
    delegate_->SetDeviceColorSpace("reset");
    delegate_->SetDeviceScaleFactor(GetDefaultDeviceScaleFactor());
    delegate_->SetBlockThirdPartyCookies(false);
    delegate_->SetLocale("");
    delegate_->UseUnfortunateSynchronousResizeMode(false);
    delegate_->ResetAutoResizeMode();
    delegate_->DeleteAllCookies();
    delegate_->SetBluetoothManualChooser(false);
    delegate_->ResetPermissions();
  }

  dump_as_audio_ = false;
  dump_back_forward_list_ = false;
  test_repaint_ = false;
  sweep_horizontally_ = false;
  animation_requires_raster_ = false;
  // Starts as true for the initial load which does not come from the
  // WorkQueue.
  running_load_ = true;
  did_notify_done_ = false;

  http_headers_to_clear_.clear();

  platform_name_ = "chromium";
  tooltip_text_ = std::string();
  web_history_item_count_ = 0;

  weak_factory_.InvalidateWeakPtrs();
  work_queue_.Reset();

  if (close_remaining_windows_ && delegate_)
    delegate_->CloseRemainingWindows();
  else
    close_remaining_windows_ = true;

  spellcheck_->Reset();
}

void TestRunner::SetTestIsRunning(bool running) {
  test_is_running_ = running;
}

bool TestRunner::ShouldDumpSelectionRect() const {
  return web_test_runtime_flags_.dump_selection_rect();
}

bool TestRunner::ShouldDumpEditingCallbacks() const {
  return web_test_runtime_flags_.dump_editting_callbacks();
}

void TestRunner::SetShouldDumpAsText(bool value) {
  web_test_runtime_flags_.set_dump_as_text(value);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::SetShouldDumpAsMarkup(bool value) {
  web_test_runtime_flags_.set_dump_as_markup(value);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::SetShouldDumpAsLayout(bool value) {
  web_test_runtime_flags_.set_dump_as_layout(value);
  OnWebTestRuntimeFlagsChanged();
}

bool TestRunner::ShouldDumpAsCustomText() const {
  return web_test_runtime_flags_.has_custom_text_output();
}

std::string TestRunner::CustomDumpText() const {
  return web_test_runtime_flags_.custom_text_output();
}

void TestRunner::SetCustomTextOutput(const std::string& text) {
  web_test_runtime_flags_.set_custom_text_output(text);
  web_test_runtime_flags_.set_has_custom_text_output(true);
  OnWebTestRuntimeFlagsChanged();
}

bool TestRunner::ShouldGeneratePixelResults() {
  CheckResponseMimeType();
  return web_test_runtime_flags_.generate_pixel_results();
}

bool TestRunner::ShouldStayOnPageAfterHandlingBeforeUnload() const {
  return web_test_runtime_flags_.stay_on_page_after_handling_before_unload();
}

void TestRunner::SetShouldGeneratePixelResults(bool value) {
  web_test_runtime_flags_.set_generate_pixel_results(value);
  OnWebTestRuntimeFlagsChanged();
}

bool TestRunner::ShouldDumpAsAudio() const {
  return dump_as_audio_;
}

void TestRunner::GetAudioData(std::vector<unsigned char>* buffer_view) const {
  *buffer_view = audio_data_;
}

bool TestRunner::IsRecursiveLayoutDumpRequested() {
  CheckResponseMimeType();
  return web_test_runtime_flags_.dump_child_frames();
}

std::string TestRunner::DumpLayout(blink::WebLocalFrame* frame) {
  CheckResponseMimeType();
  return ::test_runner::DumpLayout(frame, web_test_runtime_flags_);
}

bool TestRunner::CanDumpPixelsFromRenderer() const {
  return web_test_runtime_flags_.dump_drag_image() ||
         web_test_runtime_flags_.is_printing();
}

void TestRunner::DumpPixelsAsync(
    content::RenderView* render_view,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  auto* view_proxy = static_cast<WebViewTestProxy*>(render_view);
  DCHECK(view_proxy->GetWebView()->MainFrame());
  DCHECK(CanDumpPixelsFromRenderer());

  if (web_test_runtime_flags_.dump_drag_image()) {
    if (!drag_image_.isNull()) {
      std::move(callback).Run(drag_image_);
    } else {
      // This means the test called dumpDragImage but did not initiate a drag.
      // Return a blank image so that the test fails.
      SkBitmap bitmap;
      bitmap.allocN32Pixels(1, 1);
      bitmap.eraseColor(0);
      std::move(callback).Run(bitmap);
    }
    return;
  }

  blink::WebLocalFrame* frame =
      view_proxy->GetWebView()->MainFrame()->ToWebLocalFrame();
  blink::WebLocalFrame* target_frame = frame;
  std::string frame_name = web_test_runtime_flags_.printing_frame();
  if (!frame_name.empty()) {
    blink::WebFrame* frame_to_print =
        frame->FindFrameByName(blink::WebString::FromUTF8(frame_name));
    if (frame_to_print && frame_to_print->IsWebLocalFrame())
      target_frame = frame_to_print->ToWebLocalFrame();
  }
  test_runner::PrintFrameAsync(target_frame, std::move(callback));
}

void TestRunner::ReplicateWebTestRuntimeFlagsChanges(
    const base::DictionaryValue& changed_values) {
  if (test_is_running_) {
    web_test_runtime_flags_.tracked_dictionary().ApplyUntrackedChanges(
        changed_values);

    bool allowed = web_test_runtime_flags_.plugins_allowed();
    for (WebViewTestProxy* window : test_interfaces_->GetWindowList())
      window->webview()->GetSettings()->SetPluginsEnabled(allowed);
  }
}

bool TestRunner::HasCustomTextDump(std::string* custom_text_dump) const {
  if (ShouldDumpAsCustomText()) {
    *custom_text_dump = CustomDumpText();
    return true;
  }

  return false;
}

bool TestRunner::ShouldDumpFrameLoadCallbacks() const {
  return test_is_running_ &&
         web_test_runtime_flags_.dump_frame_load_callbacks();
}

void TestRunner::SetShouldDumpFrameLoadCallbacks(bool value) {
  web_test_runtime_flags_.set_dump_frame_load_callbacks(value);
  OnWebTestRuntimeFlagsChanged();
}

bool TestRunner::ShouldDumpPingLoaderCallbacks() const {
  return test_is_running_ &&
         web_test_runtime_flags_.dump_ping_loader_callbacks();
}

void TestRunner::SetShouldEnableViewSource(bool value) {
  // TODO(lukasza): This flag should be 1) replicated across OOPIFs and
  // 2) applied to all views, not just the main window view.

  // Path-based test config is trigerred by BlinkTestRunner, when |main_view_|
  // is guaranteed to exist at this point.
  DCHECK(main_view_);

  CHECK(main_view_->MainFrame()->IsWebLocalFrame())
      << "This function requires that the main frame is a local frame.";
  main_view_->MainFrame()->ToWebLocalFrame()->EnableViewSourceMode(value);
}

bool TestRunner::ShouldDumpUserGestureInFrameLoadCallbacks() const {
  return test_is_running_ &&
         web_test_runtime_flags_.dump_user_gesture_in_frame_load_callbacks();
}

bool TestRunner::ShouldDumpTitleChanges() const {
  return web_test_runtime_flags_.dump_title_changes();
}

bool TestRunner::ShouldDumpIconChanges() const {
  return web_test_runtime_flags_.dump_icon_changes();
}

bool TestRunner::ShouldDumpCreateView() const {
  return web_test_runtime_flags_.dump_create_view();
}

bool TestRunner::CanOpenWindows() const {
  return web_test_runtime_flags_.can_open_windows();
}

blink::WebContentSettingsClient* TestRunner::GetWebContentSettings() const {
  return mock_content_settings_client_.get();
}

blink::WebTextCheckClient* TestRunner::GetWebTextCheckClient() const {
  return spellcheck_.get();
}

bool TestRunner::ShouldDumpSpellCheckCallbacks() const {
  return web_test_runtime_flags_.dump_spell_check_callbacks();
}

bool TestRunner::ShouldDumpBackForwardList() const {
  return dump_back_forward_list_;
}

bool TestRunner::ShouldWaitUntilExternalURLLoad() const {
  return web_test_runtime_flags_.wait_until_external_url_load();
}

const std::set<std::string>* TestRunner::HttpHeadersToClear() const {
  return &http_headers_to_clear_;
}

bool TestRunner::IsFramePartOfMainTestWindow(blink::WebFrame* frame) const {
  return test_is_running_ && frame->Top()->View() == main_view_;
}

void TestRunner::AddLoadingFrame(blink::WebFrame* frame) {
  if (!IsFramePartOfMainTestWindow(frame))
    return;

  if (loading_frames_.empty()) {
    // Don't do anything if another renderer process is already tracking the
    // loading frames.
    if (web_test_runtime_flags_.have_loading_frame())
      return;
    web_test_runtime_flags_.set_have_loading_frame(true);
    OnWebTestRuntimeFlagsChanged();
  }

  loading_frames_.push_back(frame);
}

void TestRunner::RemoveLoadingFrame(blink::WebFrame* frame) {
  if (!IsFramePartOfMainTestWindow(frame))
    return;

  if (!base::Contains(loading_frames_, frame))
    return;

  DCHECK(web_test_runtime_flags_.have_loading_frame());

  base::Erase(loading_frames_, frame);
  if (!loading_frames_.empty())
    return;

  running_load_ = false;
  web_test_runtime_flags_.set_have_loading_frame(false);
  OnWebTestRuntimeFlagsChanged();

  web_history_item_count_ = delegate_->NavigationEntryCount();

  // No more new work after the first complete load.
  work_queue_.set_frozen(true);
  // Inform the work queue that any load it started is done, in case it is still
  // inside ProcessWork().
  work_queue_.set_finished_loading();

  // The test chooses between running queued tasks or waiting for NotifyDone()
  // but not both.
  if (!web_test_runtime_flags_.wait_until_done())
    work_queue_.ProcessWorkSoon();
}

void TestRunner::FinishTestIfReady() {
  if (!test_is_running_)
    return;
  // The test only ends due to no queued tasks when not waiting for
  // NotifyDone() from the test. The test chooses between these two modes.
  if (web_test_runtime_flags_.wait_until_done())
    return;
  // If the test is running a loading task, we wait for that.
  if (running_load_)
    return;

  // The test may cause loading to occur in ways other than through the
  // WorkQueue, and we wait for them before finishing the test.
  if (!loading_frames_.empty())
    return;

  // If there are tasks in the queue still, we must wait for them before
  // finishing the test.
  if (!work_queue_.is_empty())
    return;

  // When there are no more frames loading, and the test hasn't asked to wait
  // for NotifyDone(), then we normally conclude the test. However if this
  // TestRunner is attached to a swapped out frame tree - that is the main
  // frame is in another frame tree - then finishing here would be premature
  // for the main frame where the test is running. If |did_notify_done_| is true
  // then we *were* waiting for NotifyDone() and it has already happened, so we
  // want to proceed as if the NotifyDone() is happening now.
  //
  // Ideally, the main frame would wait for loading frames in its frame tree
  // as well as any secondary renderers, but it does not know about secondary
  // renderers. So in this case the test should finish when frames finish
  // loading in the primary renderer, and we don't finish the test from a
  // secondary renderer unless it is asked for explicitly via NotifyDone.
  if (!main_view_->MainFrame()->IsWebLocalFrame() && !did_notify_done_)
    return;

  // No tasks left to run, all frames are done loading from previous tasks, and
  // we're not waiting for NotifyDone(), so the test is done.
  delegate_->TestFinished();
}

blink::WebFrame* TestRunner::MainFrame() const {
  return main_view_->MainFrame();
}

void TestRunner::PolicyDelegateDone() {
  DCHECK(web_test_runtime_flags_.wait_until_done());
  delegate_->TestFinished();
  web_test_runtime_flags_.set_wait_until_done(false);
  OnWebTestRuntimeFlagsChanged();
}

bool TestRunner::PolicyDelegateEnabled() const {
  return web_test_runtime_flags_.policy_delegate_enabled();
}

bool TestRunner::PolicyDelegateIsPermissive() const {
  return web_test_runtime_flags_.policy_delegate_is_permissive();
}

bool TestRunner::PolicyDelegateShouldNotifyDone() const {
  return web_test_runtime_flags_.policy_delegate_should_notify_done();
}

void TestRunner::SetToolTipText(const blink::WebString& text) {
  tooltip_text_ = text.Utf8();
}

void TestRunner::SetDragImage(const SkBitmap& drag_image) {
  if (web_test_runtime_flags_.dump_drag_image()) {
    if (drag_image_.isNull())
      drag_image_ = drag_image;
  }
}

bool TestRunner::ShouldDumpNavigationPolicy() const {
  return web_test_runtime_flags_.dump_navigation_policy();
}

void TestRunner::SetV8CacheDisabled(bool disabled) {
  if (!main_view_) {
    disable_v8_cache_ = disabled;
    return;
  }
  main_view_->GetSettings()->SetV8CacheOptions(
      disabled ? blink::WebSettings::V8CacheOptions::kNone
               : blink::WebSettings::V8CacheOptions::kDefault);
}

void TestRunner::NavigateSecondaryWindow(const GURL& url) {
  delegate_->NavigateSecondaryWindow(url);
}

void TestRunner::InspectSecondaryWindow() {
  delegate_->InspectSecondaryWindow();
}

class WorkItemBackForward : public TestRunner::WorkItem {
 public:
  explicit WorkItemBackForward(int distance) : distance_(distance) {}

  bool Run(WebTestDelegate* delegate, blink::WebView*) override {
    delegate->GoToOffset(distance_);
    return true;  // FIXME: Did it really start a navigation?
  }

 private:
  int distance_;
};

void TestRunner::WaitUntilDone() {
  web_test_runtime_flags_.set_wait_until_done(true);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::QueueBackNavigation(int how_far_back) {
  work_queue_.AddWork(new WorkItemBackForward(-how_far_back));
}

void TestRunner::QueueForwardNavigation(int how_far_forward) {
  work_queue_.AddWork(new WorkItemBackForward(how_far_forward));
}

class WorkItemReload : public TestRunner::WorkItem {
 public:
  bool Run(WebTestDelegate* delegate, blink::WebView*) override {
    delegate->Reload();
    return true;
  }
};

void TestRunner::QueueReload() {
  work_queue_.AddWork(new WorkItemReload());
}

class WorkItemLoadingScript : public TestRunner::WorkItem {
 public:
  explicit WorkItemLoadingScript(const std::string& script) : script_(script) {}

  bool Run(WebTestDelegate*, blink::WebView* web_view) override {
    blink::WebFrame* main_frame = web_view->MainFrame();
    if (!main_frame->IsWebLocalFrame()) {
      CHECK(false) << "This function cannot be called if the main frame is not "
                      "a local frame.";
      return false;
    }
    main_frame->ToWebLocalFrame()->ExecuteScript(
        blink::WebScriptSource(blink::WebString::FromUTF8(script_)));
    return true;  // FIXME: Did it really start a navigation?
  }

 private:
  std::string script_;
};

void TestRunner::QueueLoadingScript(const std::string& script) {
  work_queue_.AddWork(new WorkItemLoadingScript(script));
}

class WorkItemNonLoadingScript : public TestRunner::WorkItem {
 public:
  explicit WorkItemNonLoadingScript(const std::string& script)
      : script_(script) {}

  bool Run(WebTestDelegate*, blink::WebView* web_view) override {
    blink::WebFrame* main_frame = web_view->MainFrame();
    if (!main_frame->IsWebLocalFrame()) {
      CHECK(false) << "This function cannot be called if the main frame is not "
                      "a local frame.";
      return false;
    }
    main_frame->ToWebLocalFrame()->ExecuteScript(
        blink::WebScriptSource(blink::WebString::FromUTF8(script_)));
    return false;
  }

 private:
  std::string script_;
};

void TestRunner::QueueNonLoadingScript(const std::string& script) {
  work_queue_.AddWork(new WorkItemNonLoadingScript(script));
}

class WorkItemLoad : public TestRunner::WorkItem {
 public:
  WorkItemLoad(const blink::WebURL& url, const std::string& target)
      : url_(url), target_(target) {}

  bool Run(WebTestDelegate* delegate, blink::WebView*) override {
    delegate->LoadURLForFrame(url_, target_);
    return true;  // FIXME: Did it really start a navigation?
  }

 private:
  blink::WebURL url_;
  std::string target_;
};

void TestRunner::QueueLoad(const std::string& url, const std::string& target) {
  if (!main_view_)
    return;

  // TODO(lukasza): testRunner.queueLoad(...) should work even if the main frame
  // is remote (ideally testRunner.queueLoad would bind to and execute in the
  // context of a specific local frame - resolving relative urls should be done
  // on relative to the calling frame's url).
  CHECK(main_view_->MainFrame()->IsWebLocalFrame())
      << "This function cannot be called if the main frame is not "
         "a local frame.";

  // FIXME: Implement blink::WebURL::resolve() and avoid GURL.
  GURL current_url =
      main_view_->MainFrame()->ToWebLocalFrame()->GetDocument().Url();
  GURL full_url = current_url.Resolve(url);
  work_queue_.AddWork(new WorkItemLoad(full_url, target));
}

void TestRunner::SetCustomPolicyDelegate(gin::Arguments* args) {
  bool value;
  args->GetNext(&value);
  web_test_runtime_flags_.set_policy_delegate_enabled(value);

  if (!args->PeekNext().IsEmpty() && args->PeekNext()->IsBoolean()) {
    args->GetNext(&value);
    web_test_runtime_flags_.set_policy_delegate_is_permissive(value);
  }

  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::WaitForPolicyDelegate() {
  web_test_runtime_flags_.set_policy_delegate_enabled(true);
  web_test_runtime_flags_.set_policy_delegate_should_notify_done(true);
  web_test_runtime_flags_.set_wait_until_done(true);
  OnWebTestRuntimeFlagsChanged();
}

int TestRunner::WindowCount() {
  return test_interfaces_->GetWindowList().size();
}

void TestRunner::SetCloseRemainingWindowsWhenComplete(
    bool close_remaining_windows) {
  close_remaining_windows_ = close_remaining_windows;
}

void TestRunner::ResetTestHelperControllers() {
  test_interfaces_->ResetTestHelperControllers();
}

void TestRunner::AddOriginAccessAllowListEntry(
    const std::string& source_origin,
    const std::string& destination_protocol,
    const std::string& destination_host,
    bool allow_destination_subdomains) {
  blink::WebURL url((GURL(source_origin)));
  if (!url.IsValid())
    return;

  blink::WebSecurityPolicy::AddOriginAccessAllowListEntry(
      url, blink::WebString::FromUTF8(destination_protocol),
      blink::WebString::FromUTF8(destination_host), /*destination_port=*/0,
      allow_destination_subdomains
          ? network::mojom::CorsDomainMatchMode::kAllowSubdomains
          : network::mojom::CorsDomainMatchMode::kDisallowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
}

void TestRunner::SetTextSubpixelPositioning(bool value) {
#if defined(OS_LINUX) || defined(OS_FUCHSIA)
  // Since FontConfig doesn't provide a variable to control subpixel
  // positioning, we'll fall back to setting it globally for all fonts.
  blink::WebFontRenderStyle::SetSubpixelPositioning(value);
#endif
}

void TestRunner::UseUnfortunateSynchronousResizeMode() {
  delegate_->UseUnfortunateSynchronousResizeMode(true);
}

void TestRunner::EnableAutoResizeMode(int min_width,
                                      int min_height,
                                      int max_width,
                                      int max_height) {
  if (max_width <= 0 || max_height <= 0)
    return;
  blink::WebSize min_size(min_width, min_height);
  blink::WebSize max_size(max_width, max_height);
  delegate_->EnableAutoResizeMode(min_size, max_size);
}

void TestRunner::DisableAutoResizeMode(int new_width, int new_height) {
  if (new_width <= 0 || new_height <= 0)
    return;
  blink::WebSize new_size(new_width, new_height);
  delegate_->DisableAutoResizeMode(new_size);
}

MockScreenOrientationClient* TestRunner::GetMockScreenOrientationClient() {
  return &mock_screen_orientation_client_;
}

void TestRunner::SetMockScreenOrientation(const std::string& orientation_str) {
  blink::WebScreenOrientationType orientation;

  if (orientation_str == "portrait-primary") {
    orientation = blink::kWebScreenOrientationPortraitPrimary;
  } else if (orientation_str == "portrait-secondary") {
    orientation = blink::kWebScreenOrientationPortraitSecondary;
  } else if (orientation_str == "landscape-primary") {
    orientation = blink::kWebScreenOrientationLandscapePrimary;
  } else {
    DCHECK_EQ("landscape-secondary", orientation_str);
    orientation = blink::kWebScreenOrientationLandscapeSecondary;
  }

  for (WebViewTestProxy* window : test_interfaces_->GetWindowList()) {
    blink::WebFrame* main_frame = window->webview()->MainFrame();
    // TODO(lukasza): Need to make this work for remote frames.
    if (main_frame->IsWebLocalFrame()) {
      mock_screen_orientation_client_.UpdateDeviceOrientation(
          main_frame->ToWebLocalFrame(), orientation);
    }
  }
}

void TestRunner::DisableMockScreenOrientation() {
  mock_screen_orientation_client_.SetDisabled(true);
}

void TestRunner::SetPopupBlockingEnabled(bool block_popups) {
  delegate_->SetPopupBlockingEnabled(block_popups);
}

void TestRunner::SetJavaScriptCanAccessClipboard(bool can_access) {
  delegate_->Preferences()->java_script_can_access_clipboard = can_access;
  delegate_->ApplyPreferences();
}

void TestRunner::SetAllowFileAccessFromFileURLs(bool allow) {
  delegate_->Preferences()->allow_file_access_from_file_urls = allow;
  delegate_->ApplyPreferences();
}

void TestRunner::OverridePreference(gin::Arguments* args) {
  if (args->Length() != 2) {
    args->ThrowTypeError("overridePreference expects 2 arguments");
    return;
  }

  std::string key;
  if (!args->GetNext(&key)) {
    args->ThrowError();
    return;
  }

  TestPreferences* prefs = delegate_->Preferences();
  if (key == "WebKitDefaultFontSize") {
    ConvertAndSet(args, &prefs->default_font_size);
  } else if (key == "WebKitMinimumFontSize") {
    ConvertAndSet(args, &prefs->minimum_font_size);
  } else if (key == "WebKitDefaultTextEncodingName") {
    ConvertAndSet(args, &prefs->default_text_encoding_name);
  } else if (key == "WebKitJavaScriptEnabled") {
    ConvertAndSet(args, &prefs->java_script_enabled);
  } else if (key == "WebKitSupportsMultipleWindows") {
    ConvertAndSet(args, &prefs->supports_multiple_windows);
  } else if (key == "WebKitDisplayImagesKey") {
    ConvertAndSet(args, &prefs->loads_images_automatically);
  } else if (key == "WebKitPluginsEnabled") {
    ConvertAndSet(args, &prefs->plugins_enabled);
  } else if (key == "WebKitTabToLinksPreferenceKey") {
    ConvertAndSet(args, &prefs->tabs_to_links);
  } else if (key == "WebKitCSSGridLayoutEnabled") {
    ConvertAndSet(args, &prefs->experimental_css_grid_layout_enabled);
  } else if (key == "WebKitHyperlinkAuditingEnabled") {
    ConvertAndSet(args, &prefs->hyperlink_auditing_enabled);
  } else if (key == "WebKitEnableCaretBrowsing") {
    ConvertAndSet(args, &prefs->caret_browsing_enabled);
  } else if (key == "WebKitAllowRunningInsecureContent") {
    ConvertAndSet(args, &prefs->allow_running_of_insecure_content);
  } else if (key == "WebKitDisableReadingFromCanvas") {
    ConvertAndSet(args, &prefs->disable_reading_from_canvas);
  } else if (key == "WebKitStrictMixedContentChecking") {
    ConvertAndSet(args, &prefs->strict_mixed_content_checking);
  } else if (key == "WebKitStrictPowerfulFeatureRestrictions") {
    ConvertAndSet(args, &prefs->strict_powerful_feature_restrictions);
  } else if (key == "WebKitShouldRespectImageOrientation") {
    ConvertAndSet(args, &prefs->should_respect_image_orientation);
  } else if (key == "WebKitWebSecurityEnabled") {
    ConvertAndSet(args, &prefs->web_security_enabled);
  } else if (key == "WebKitSpatialNavigationEnabled") {
    ConvertAndSet(args, &prefs->spatial_navigation_enabled);
  } else {
    args->ThrowTypeError("Invalid name for preference: " + key);
  }
  delegate_->ApplyPreferences();
}

std::string TestRunner::GetAcceptLanguages() const {
  return web_test_runtime_flags_.accept_languages();
}

void TestRunner::SetAcceptLanguages(const std::string& accept_languages) {
  if (accept_languages == GetAcceptLanguages())
    return;

  web_test_runtime_flags_.set_accept_languages(accept_languages);
  OnWebTestRuntimeFlagsChanged();

  for (WebViewTestProxy* window : test_interfaces_->GetWindowList())
    window->webview()->AcceptLanguagesChanged();
}

void TestRunner::SetPluginsEnabled(bool enabled) {
  delegate_->Preferences()->plugins_enabled = enabled;
  delegate_->ApplyPreferences();
}

void TestRunner::DumpEditingCallbacks() {
  web_test_runtime_flags_.set_dump_editting_callbacks(true);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::DumpAsMarkup() {
  web_test_runtime_flags_.set_dump_as_markup(true);
  web_test_runtime_flags_.set_generate_pixel_results(false);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::DumpAsText() {
  web_test_runtime_flags_.set_dump_as_text(true);
  web_test_runtime_flags_.set_generate_pixel_results(false);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::DumpAsTextWithPixelResults() {
  web_test_runtime_flags_.set_dump_as_text(true);
  web_test_runtime_flags_.set_generate_pixel_results(true);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::DumpAsLayout() {
  web_test_runtime_flags_.set_dump_as_layout(true);
  web_test_runtime_flags_.set_generate_pixel_results(false);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::DumpAsLayoutWithPixelResults() {
  web_test_runtime_flags_.set_dump_as_layout(true);
  web_test_runtime_flags_.set_generate_pixel_results(true);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::DumpChildFrames() {
  web_test_runtime_flags_.set_dump_child_frames(true);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::DumpIconChanges() {
  web_test_runtime_flags_.set_dump_icon_changes(true);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::SetAudioData(const gin::ArrayBufferView& view) {
  unsigned char* bytes = static_cast<unsigned char*>(view.bytes());
  audio_data_.resize(view.num_bytes());
  std::copy(bytes, bytes + view.num_bytes(), audio_data_.begin());
  dump_as_audio_ = true;
}

void TestRunner::DumpFrameLoadCallbacks() {
  web_test_runtime_flags_.set_dump_frame_load_callbacks(true);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::DumpPingLoaderCallbacks() {
  web_test_runtime_flags_.set_dump_ping_loader_callbacks(true);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::DumpUserGestureInFrameLoadCallbacks() {
  web_test_runtime_flags_.set_dump_user_gesture_in_frame_load_callbacks(true);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::DumpTitleChanges() {
  web_test_runtime_flags_.set_dump_title_changes(true);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::DumpCreateView() {
  web_test_runtime_flags_.set_dump_create_view(true);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::SetCanOpenWindows() {
  web_test_runtime_flags_.set_can_open_windows(true);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::SetImagesAllowed(bool allowed) {
  web_test_runtime_flags_.set_images_allowed(allowed);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::SetScriptsAllowed(bool allowed) {
  web_test_runtime_flags_.set_scripts_allowed(allowed);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::SetStorageAllowed(bool allowed) {
  web_test_runtime_flags_.set_storage_allowed(allowed);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::SetPluginsAllowed(bool allowed) {
  web_test_runtime_flags_.set_plugins_allowed(allowed);

  for (WebViewTestProxy* window : test_interfaces_->GetWindowList())
    window->webview()->GetSettings()->SetPluginsEnabled(allowed);

  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::SetAllowRunningOfInsecureContent(bool allowed) {
  web_test_runtime_flags_.set_running_insecure_content_allowed(allowed);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::SetAutoplayAllowed(bool allowed) {
  web_test_runtime_flags_.set_autoplay_allowed(allowed);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::DumpPermissionClientCallbacks() {
  web_test_runtime_flags_.set_dump_web_content_settings_client_callbacks(true);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::SetDisallowedSubresourcePathSuffixes(
    const std::vector<std::string>& suffixes,
    bool block_subresources) {
  DCHECK(main_view_);
  if (!main_view_->MainFrame()->IsWebLocalFrame())
    return;
  main_view_->MainFrame()
      ->ToWebLocalFrame()
      ->GetDocumentLoader()
      ->SetSubresourceFilter(
          new MockWebDocumentSubresourceFilter(suffixes, block_subresources));
}

void TestRunner::DumpSpellCheckCallbacks() {
  web_test_runtime_flags_.set_dump_spell_check_callbacks(true);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::DumpBackForwardList() {
  dump_back_forward_list_ = true;
}

void TestRunner::DumpSelectionRect() {
  web_test_runtime_flags_.set_dump_selection_rect(true);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::SetPrinting() {
  SetPrintingForFrame("");
}

void TestRunner::SetPrintingForFrame(const std::string& frame_name) {
  web_test_runtime_flags_.set_printing_frame(frame_name);
  web_test_runtime_flags_.set_is_printing(true);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::ClearPrinting() {
  web_test_runtime_flags_.set_is_printing(false);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::SetShouldStayOnPageAfterHandlingBeforeUnload(bool value) {
  web_test_runtime_flags_.set_stay_on_page_after_handling_before_unload(value);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::SetWillSendRequestClearHeader(const std::string& header) {
  if (!header.empty())
    http_headers_to_clear_.insert(header);
}

void TestRunner::WaitUntilExternalURLLoad() {
  web_test_runtime_flags_.set_wait_until_external_url_load(true);
  web_test_runtime_flags_.set_wait_until_done(true);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::DumpDragImage() {
  web_test_runtime_flags_.set_dump_drag_image(true);
  DumpAsTextWithPixelResults();
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::DumpNavigationPolicy() {
  web_test_runtime_flags_.set_dump_navigation_policy(true);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::SetDumpConsoleMessages(bool value) {
  web_test_runtime_flags_.set_dump_console_messages(value);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::SetDumpJavaScriptDialogs(bool value) {
  web_test_runtime_flags_.set_dump_javascript_dialogs(value);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::SetEffectiveConnectionType(
    blink::WebEffectiveConnectionType connection_type) {
  effective_connection_type_ = connection_type;
}

void TestRunner::SetMockSpellCheckerEnabled(bool enabled) {
  spellcheck_->SetEnabled(enabled);
}

bool TestRunner::ShouldDumpConsoleMessages() const {
  return web_test_runtime_flags_.dump_console_messages();
}

bool TestRunner::ShouldDumpJavaScriptDialogs() const {
  return web_test_runtime_flags_.dump_javascript_dialogs();
}

bool TestRunner::IsChooserShown() {
  return 0 < chooser_count_;
}

void TestRunner::ClearAllDatabases() {
  delegate_->ClearAllDatabases();
}

void TestRunner::SetDatabaseQuota(int quota) {
  delegate_->SetDatabaseQuota(quota);
}

void TestRunner::SetBlockThirdPartyCookies(bool block) {
  delegate_->SetBlockThirdPartyCookies(block);
}

void TestRunner::SetFocus(blink::WebView* web_view, bool focus) {
  if (focus) {
    if (previously_focused_view_ != web_view) {
      delegate_->SetFocus(previously_focused_view_, false);
      delegate_->SetFocus(web_view, true);
      previously_focused_view_ = web_view;
    }
  } else {
    if (previously_focused_view_ == web_view) {
      delegate_->SetFocus(web_view, false);
      previously_focused_view_ = nullptr;
    }
  }
}

std::string TestRunner::PathToLocalResource(const std::string& path) {
  return delegate_->PathToLocalResource(path);
}

void TestRunner::SetPermission(const std::string& name,
                               const std::string& value,
                               const GURL& origin,
                               const GURL& embedding_origin) {
  delegate_->SetPermission(name, value, origin, embedding_origin);
}

void TestRunner::ResolveBeforeInstallPromptPromise(
    const std::string& platform) {
  delegate_->ResolveBeforeInstallPromptPromise(platform);
}

void TestRunner::SetPOSIXLocale(const std::string& locale) {
  delegate_->SetLocale(locale);
}

void TestRunner::SimulateWebNotificationClick(
    const std::string& title,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply) {
  delegate_->SimulateWebNotificationClick(title, action_index, reply);
}

void TestRunner::SimulateWebNotificationClose(const std::string& title,
                                              bool by_user) {
  delegate_->SimulateWebNotificationClose(title, by_user);
}

void TestRunner::SimulateWebContentIndexDelete(const std::string& id) {
  delegate_->SimulateWebContentIndexDelete(id);
}

void TestRunner::SetAnimationRequiresRaster(bool do_raster) {
  animation_requires_raster_ = do_raster;
}

void TestRunner::OnWebTestRuntimeFlagsChanged() {
  if (web_test_runtime_flags_.tracked_dictionary().changed_values().empty())
    return;
  if (!test_is_running_)
    return;

  delegate_->OnWebTestRuntimeFlagsChanged(
      web_test_runtime_flags_.tracked_dictionary().changed_values());
  web_test_runtime_flags_.tracked_dictionary().ResetChangeTracking();
}

void TestRunner::CheckResponseMimeType() {
  // Text output: the test page can request different types of output which we
  // handle here.

  if (web_test_runtime_flags_.dump_as_text())
    return;

  if (!main_view_)
    return;

  if (!main_view_->MainFrame()->IsWebLocalFrame())
    return;

  blink::WebDocumentLoader* document_loader =
      main_view_->MainFrame()->ToWebLocalFrame()->GetDocumentLoader();
  if (!document_loader)
    return;

  std::string mimeType = document_loader->GetResponse().MimeType().Utf8();
  if (mimeType != "text/plain")
    return;

  web_test_runtime_flags_.set_dump_as_text(true);
  web_test_runtime_flags_.set_generate_pixel_results(false);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::NotifyDone() {
  if (web_test_runtime_flags_.wait_until_done() && loading_frames_.empty() &&
      work_queue_.is_empty())
    delegate_->TestFinished();
  web_test_runtime_flags_.set_wait_until_done(false);
  did_notify_done_ = true;
  OnWebTestRuntimeFlagsChanged();
}

}  // namespace test_runner
