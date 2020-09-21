// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/test_runner.h"

#include <stddef.h>

#include <algorithm>
#include <clocale>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/renderer/render_thread_impl.h"
#include "content/web_test/common/web_test_constants.h"
#include "content/web_test/common/web_test_string_util.h"
#include "content/web_test/renderer/app_banner_service.h"
#include "content/web_test/renderer/blink_test_helpers.h"
#include "content/web_test/renderer/fake_subresource_filter.h"
#include "content/web_test/renderer/pixel_dump.h"
#include "content/web_test/renderer/spell_check_client.h"
#include "content/web_test/renderer/test_preferences.h"
#include "content/web_test/renderer/web_frame_test_proxy.h"
#include "content/web_test/renderer/web_view_test_proxy.h"
#include "content/web_test/renderer/web_widget_test_proxy.h"
#include "gin/arguments.h"
#include "gin/array_buffer.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "mojo/public/mojom/base/text_direction.mojom-forward.h"
#include "services/network/public/mojom/cors.mojom.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/app_banner/app_banner.mojom.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_isolated_world_ids.h"
#include "third_party/blink/public/platform/web_isolated_world_info.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_array_buffer.h"
#include "third_party/blink/public/web/web_array_buffer_converter.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_manifest_manager.h"
#include "third_party/blink/public/web/web_render_theme.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"
#include "third_party/blink/public/web/web_testing_support.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/test/icc_profiles.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_FUCHSIA)
#include "third_party/blink/public/platform/web_font_render_style.h"
#endif

namespace content {

namespace {

// A V8 callback with bound arguments, and the ability to pass additional
// arguments at time of calling Run().
using BoundV8Callback =
    base::OnceCallback<void(const std::vector<v8::Local<v8::Value>>&)>;
// Returns an empty set of args for running the BoundV8Callback.
std::vector<v8::Local<v8::Value>> NoV8Args() {
  return {};
}

// Returns 3 arguments, width, height, and an array of pixel values. Takes a
// v8::Context::Scope just to prove one exists in the caller.
std::vector<v8::Local<v8::Value>> ConvertBitmapToV8(
    const v8::Context::Scope& context_scope,
    const SkBitmap& bitmap) {
  v8::Isolate* isolate = blink::MainThreadIsolate();

  std::vector<v8::Local<v8::Value>> args;
  // Note that the bitmap size can be 0 if there's no pixels.
  args.push_back(v8::Number::New(isolate, bitmap.info().width()));
  args.push_back(v8::Number::New(isolate, bitmap.info().height()));
  if (bitmap.isNull()) {
    // The 3rd argument will be undefined (an empty argument is not valid and
    // would crash).
    return args;
  }

  // Always produce pixels in RGBA order, regardless of the platform default.
  SkImageInfo info = bitmap.info().makeColorType(kRGBA_8888_SkColorType);
  size_t row_bytes = info.minRowBytes();

  blink::WebArrayBuffer buffer =
      blink::WebArrayBuffer::Create(info.computeByteSize(row_bytes), 1);
  bool read = bitmap.readPixels(info, buffer.Data(), row_bytes, 0, 0);
  CHECK(read);

  args.push_back(blink::WebArrayBufferConverter::ToV8Value(
      &buffer, isolate->GetCurrentContext()->Global(), isolate));
  return args;
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

  *set_param = web_test_string_util::V8StringToWebString(
      args->isolate(), result.ToLocalChecked());
}

}  // namespace

class TestRunnerBindings : public gin::Wrappable<TestRunnerBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static void Install(TestRunner* test_runner,
                      WebFrameTestProxy* frame,
                      SpellCheckClient* spell_check,
                      bool is_wpt_reftest,
                      bool is_main_test_window);

  // Wraps the V8 function in a base::OnceCallback that binds in the given V8
  // arguments. The callback will do nothing when Run() if the
  // TestRunnerBindings has been destroyed, so it is safe to PostTask(). At the
  // time of Run(), further arguments can be passed to the V8 function.
  BoundV8Callback WrapV8Callback(
      v8::Local<v8::Function> v8_callback,
      std::vector<v8::Local<v8::Value>> args_to_bind = {});
  // Same as WrapV8Callback but Run() takes no arguments, so only bound
  // arguments can be passed to the V8 function.
  base::OnceClosure WrapV8Closure(
      v8::Local<v8::Function> v8_callback,
      std::vector<v8::Local<v8::Value>> args_to_bind = {});
  // Calls WrapV8Callback() and then posts the resulting callback to the frame's
  // task runner.
  void PostV8Callback(v8::Local<v8::Function> v8_callback,
                      std::vector<v8::Local<v8::Value>> args = {});

  blink::WebLocalFrame* GetWebFrame() { return frame_->GetWebFrame(); }

 private:
  // Watches for the RenderFrame that the TestRunnerBindings is attached to
  // being destroyed.
  class TestRunnerBindingsRenderFrameObserver : public RenderFrameObserver {
   public:
    TestRunnerBindingsRenderFrameObserver(TestRunnerBindings* bindings,
                                          RenderFrame* frame)
        : RenderFrameObserver(frame), bindings_(bindings) {}

    // RenderFrameObserver implementation.
    void OnDestruct() override { bindings_->OnFrameDestroyed(); }

   private:
    TestRunnerBindings* const bindings_;
  };

  explicit TestRunnerBindings(TestRunner* test_runner,
                              WebFrameTestProxy* frame,
                              SpellCheckClient* spell_check);
  ~TestRunnerBindings() override;

  // gin::Wrappable overrides.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  void AddOriginAccessAllowListEntry(const std::string& source_origin,
                                     const std::string& destination_protocol,
                                     const std::string& destination_host,
                                     bool allow_destination_subdomains);
  void AddWebPageOverlay();
  void SetHighlightAds();
  void CapturePrintingPixelsThen(v8::Local<v8::Function> callback);
  void CheckForLeakedWindows();
  void ClearAllDatabases();
  void ClearTrustTokenState(v8::Local<v8::Function> callback);
  void CopyImageThen(int x, int y, v8::Local<v8::Function> callback);
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
  void DumpTitleChanges();
  void DumpUserGestureInFrameLoadCallbacks();
  void EvaluateScriptInIsolatedWorld(int world_id, const std::string& script);
  void ExecCommand(gin::Arguments* args);
  void TriggerTestInspectorIssue(gin::Arguments* args);
  void FocusDevtoolsSecondaryWindow();
  void ForceNextDrawingBufferCreationToFail();
  void ForceNextWebGLContextCreationToFail();
  void GetBluetoothManualChooserEvents(v8::Local<v8::Function> callback);
  void GetManifestThen(v8::Local<v8::Function> callback);
  base::FilePath::StringType GetWritableDirectory();
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
  void ResolveBeforeInstallPromptPromise(const std::string& platform);
  void RunIdleTasks(v8::Local<v8::Function> callback);
  void SendBluetoothManualChooserEvent(const std::string& event,
                                       const std::string& argument);
  void SetAcceptLanguages(const std::string& accept_languages);
  void SetAllowFileAccessFromFileURLs(bool allow);
  void SetAllowRunningOfInsecureContent(bool allowed);
  void SetBlockThirdPartyCookies(bool block);
  void SetAudioData(const gin::ArrayBufferView& view);
  void SetBackingScaleFactor(double value, v8::Local<v8::Function> callback);
  void SetBluetoothFakeAdapter(const std::string& adapter_name,
                               v8::Local<v8::Function> callback);
  void SetBluetoothManualChooser(bool enable);
  void SetCanOpenWindows();
  void SetColorProfile(const std::string& name,
                       v8::Local<v8::Function> callback);
  void SetCustomPolicyDelegate(gin::Arguments* args);
  void SetCustomTextOutput(const std::string& output);
  void SetDatabaseQuota(int quota);
  void SetDisallowedSubresourcePathSuffixes(std::vector<std::string> suffixes,
                                            bool block_subresources);
  void SetDomainRelaxationForbiddenForURLScheme(bool forbidden,
                                                const std::string& scheme);
  void SetDumpConsoleMessages(bool value);
  void SetDumpJavaScriptDialogs(bool value);
  void SetEffectiveConnectionType(const std::string& connection_type);
  void SetFilePathForMockFileDialog(const base::FilePath::StringType& path);
  void SetMockSpellCheckerEnabled(bool enabled);
  void SetImagesAllowed(bool allowed);
  void SetIsolatedWorldInfo(int world_id,
                            v8::Local<v8::Value> security_origin,
                            v8::Local<v8::Value> content_security_policy);
  void SetJavaScriptCanAccessClipboard(bool can_access);
  void SetMockScreenOrientation(const std::string& orientation);
  void SetPOSIXLocale(const std::string& locale);
  void SetMainWindowHidden(bool hidden);
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
  void SetTrustTokenKeyCommitments(const std::string& raw_commitments,
                                   v8::Local<v8::Function> callback);
  void SetWillSendRequestClearHeader(const std::string& header);
  void SetWillSendRequestClearReferrer();
  void SimulateBrowserWindowFocus(bool value);
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

  bool IsCommandEnabled(const std::string& command);
  std::string PathToLocalResource(const std::string& path);
  std::string PlatformName();
  std::string SelectionAsMarkup();
  void TextZoomIn();
  void TextZoomOut();
  void ZoomPageIn();
  void ZoomPageOut();
  void SetPageZoomFactor(double factor);
  std::string TooltipText();

  int WebHistoryItemCount();
  int WindowCount();

  void InvokeV8Callback(v8::UniquePersistent<v8::Function> callback,
                        std::vector<v8::UniquePersistent<v8::Value>> bound_args,
                        const std::vector<v8::Local<v8::Value>>& runtime_args);

  // Hears about the RenderFrame in |frame_| being destroyed. The
  // TestRunningBindings should not do anything thereafter.
  void OnFrameDestroyed() {
    invalid_ = true;
    weak_ptr_factory_.InvalidateWeakPtrs();
  }

  // Observer for the |frame_| the TestRunningBindings is bound to.
  TestRunnerBindingsRenderFrameObserver frame_observer_;

  // Becomes true when the underlying frame is destroyed. Then the class should
  // stop doing anything.
  bool invalid_ = false;

  TestRunner* runner_;
  WebFrameTestProxy* const frame_;
  SpellCheckClient* const spell_check_;
  TestPreferences prefs_;
  std::unique_ptr<AppBannerService> app_banner_service_;

  base::WeakPtrFactory<TestRunnerBindings> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TestRunnerBindings);
};

gin::WrapperInfo TestRunnerBindings::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
void TestRunnerBindings::Install(TestRunner* test_runner,
                                 WebFrameTestProxy* frame,
                                 SpellCheckClient* spell_check,
                                 bool is_wpt_test,
                                 bool is_main_test_window) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  blink::WebLocalFrame* web_frame = frame->GetWebFrame();
  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
  CHECK(!context.IsEmpty());

  v8::Context::Scope context_scope(context);

  TestRunnerBindings* wrapped =
      new TestRunnerBindings(test_runner, frame, spell_check);
  gin::Handle<TestRunnerBindings> bindings =
      gin::CreateHandle(isolate, wrapped);
  CHECK(!bindings.IsEmpty());
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
  // 2. For reftests with a 'reftest-wait' or crash tests with a 'test-wait'
  //    class on the root element, reference comparison is delayed (and a
  //    TestRendered event emitted in its place) until that class attribute is
  //    removed. To support this feature, we use a mutation observer.
  //    https://web-platform-tests.org/writing-tests/reftests.html#controlling-when-comparison-occurs
  //    https://web-platform-tests.org/writing-tests/crashtest.html
  //
  // Note that this method may be called multiple times on a frame, so we put
  // the code behind a flag. The flag is safe to be installed on testRunner
  // because WPT reftests never access this object.
  if (is_wpt_test && is_main_test_window && !web_frame->Parent() &&
      !web_frame->Opener()) {
    web_frame->ExecuteScript(blink::WebString(
        R"(if (!window.testRunner._wpt_reftest_setup) {
          window.testRunner._wpt_reftest_setup = true;

          window.addEventListener('load', function() {
            if (window.assert_equals) // In case of a testharness test.
              return;
            window.testRunner.waitUntilDone();
            const target = document.documentElement;
            if (target != null &&
                (target.classList.contains('reftest-wait') ||
                 target.classList.contains('test-wait'))) {
              const observer = new MutationObserver(function(mutations) {
                mutations.forEach(function(mutation) {
                  if (!target.classList.contains('reftest-wait') &&
                      !target.classList.contains('test-wait')) {
                    window.testRunner.notifyDone();
                  }
                });
              });
              const config = {attributes: true};
              observer.observe(target, config);

              var event = new Event('TestRendered', {bubbles: true});
              target.dispatchEvent(event);
            } else {
              document.fonts.ready.then(() => window.testRunner.notifyDone());
            }
          });
        })"));
  }
}

TestRunnerBindings::TestRunnerBindings(TestRunner* runner,
                                       WebFrameTestProxy* frame,
                                       SpellCheckClient* spell_check)
    : frame_observer_(this, frame),
      runner_(runner),
      frame_(frame),
      spell_check_(spell_check) {}

TestRunnerBindings::~TestRunnerBindings() = default;

gin::ObjectTemplateBuilder TestRunnerBindings::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<TestRunnerBindings>::GetObjectTemplateBuilder(isolate)
      .SetMethod("abortModal", &TestRunnerBindings::NotImplemented)
      .SetMethod("addDisallowedURL", &TestRunnerBindings::NotImplemented)
      .SetMethod("addOriginAccessAllowListEntry",
                 &TestRunnerBindings::AddOriginAccessAllowListEntry)
      // Permits the adding of only one opaque overlay. May only be called from
      // inside the main frame.
      .SetMethod("addWebPageOverlay", &TestRunnerBindings::AddWebPageOverlay)
      .SetMethod("capturePrintingPixelsThen",
                 &TestRunnerBindings::CapturePrintingPixelsThen)
      // If the test will be closing its windows explicitly, and wants to look
      // for leaks due to those windows closing incorrectly, it can specify this
      // to avoid having them closed at the end of the test before the leak
      // checker.
      .SetMethod("checkForLeakedWindows",
                 &TestRunnerBindings::CheckForLeakedWindows)
      // Clears WebSQL databases.
      .SetMethod("clearAllDatabases", &TestRunnerBindings::ClearAllDatabases)
      .SetMethod("clearBackForwardList", &TestRunnerBindings::NotImplemented)
      // Clears persistent Trust Tokens state in the browser. See
      // https://github.com/wicg/trust-token-api.
      .SetMethod("clearTrustTokenState",
                 &TestRunnerBindings::ClearTrustTokenState)
      .SetMethod("copyImageThen", &TestRunnerBindings::CopyImageThen)
      .SetMethod("disableAutoResizeMode",
                 &TestRunnerBindings::DisableAutoResizeMode)
      .SetMethod("disableMockScreenOrientation",
                 &TestRunnerBindings::DisableMockScreenOrientation)
      // Sets up a mock DocumentSubresourceFilter to disallow subsequent
      // subresource loads within the current document with the given path
      // |suffixes|. The filter is created and injected even if |suffixes| is
      // empty. If |suffixes| contains the empty string, all subresource loads
      // will be disallowed. If |block_subresources| is false, matching
      // resources will not be blocked but instead marked as matching a
      // disallowed resource.
      .SetMethod("setDisallowedSubresourcePathSuffixes",
                 &TestRunnerBindings::SetDisallowedSubresourcePathSuffixes)
      // Causes the beforeinstallprompt event to be sent to the renderer.
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
      .SetMethod("dumpTitleChanges", &TestRunnerBindings::DumpTitleChanges)
      .SetMethod("dumpUserGestureInFrameLoadCallbacks",
                 &TestRunnerBindings::DumpUserGestureInFrameLoadCallbacks)
      .SetMethod("enableAutoResizeMode",
                 &TestRunnerBindings::EnableAutoResizeMode)
      .SetMethod("evaluateScriptInIsolatedWorld",
                 &TestRunnerBindings::EvaluateScriptInIsolatedWorld)
      .SetMethod(
          "evaluateScriptInIsolatedWorldAndReturnValue",
          &TestRunnerBindings::EvaluateScriptInIsolatedWorldAndReturnValue)
      // Executes an internal command (superset of document.execCommand()
      // commands) on the frame's document.
      .SetMethod("execCommand", &TestRunnerBindings::ExecCommand)
      // Trigger an inspector issue for the frame.
      .SetMethod("triggerTestInspectorIssue",
                 &TestRunnerBindings::TriggerTestInspectorIssue)
      .SetMethod("findString", &TestRunnerBindings::FindString)
      // Moves focus and active state to the secondary devtools window, which
      // exists only in devtools JS tests.
      .SetMethod("focusDevtoolsSecondaryWindow",
                 &TestRunnerBindings::FocusDevtoolsSecondaryWindow)
      // Sets a flag causing the next call to WebGLRenderingContext::Create() to
      // fail.
      .SetMethod("forceNextDrawingBufferCreationToFail",
                 &TestRunnerBindings::ForceNextDrawingBufferCreationToFail)
      // Sets a flag causing the next call to DrawingBuffer::Create() to fail.
      .SetMethod("forceNextWebGLContextCreationToFail",
                 &TestRunnerBindings::ForceNextWebGLContextCreationToFail)

      // The Bluetooth functions are specified at
      // https://webbluetoothcg.github.io/web-bluetooth/tests/.
      //
      // Returns the events recorded since the last call to this function.
      .SetMethod("getBluetoothManualChooserEvents",
                 &TestRunnerBindings::GetBluetoothManualChooserEvents)
      .SetMethod("getManifestThen", &TestRunnerBindings::GetManifestThen)
      // Returns the absolute path to a directory this test can write data in.
      // This returns the path to a fresh empty directory every time this method
      // is called. Additionally when this method is called any previously
      // created directories will be deleted.
      .SetMethod("getWritableDirectory",
                 &TestRunnerBindings::GetWritableDirectory)
      .SetMethod("insertStyleSheet", &TestRunnerBindings::InsertStyleSheet)
      // Checks if an internal editing command is currently available for the
      // frame's document.
      .SetMethod("isCommandEnabled", &TestRunnerBindings::IsCommandEnabled)
      .SetMethod("keepWebHistory", &TestRunnerBindings::NotImplemented)
      .SetMethod("updateAllLifecyclePhasesAndComposite",
                 &TestRunnerBindings::UpdateAllLifecyclePhasesAndComposite)
      // Note, the reply callback is executed synchronously. Wrap in
      // setTimeout() to run asynchronously.
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
      // Removes an overlay added by addWebPageOverlay(). May only be called
      // from inside the main frame.
      .SetMethod("removeWebPageOverlay",
                 &TestRunnerBindings::RemoveWebPageOverlay)
      .SetMethod("resolveBeforeInstallPromptPromise",
                 &TestRunnerBindings::ResolveBeforeInstallPromptPromise)
      // Immediately run all pending idle tasks, including all pending
      // requestIdleCallback calls.  Invoke the callback when all
      // idle tasks are complete.
      .SetMethod("runIdleTasks", &TestRunnerBindings::RunIdleTasks)
      .SetMethod("selectionAsMarkup", &TestRunnerBindings::SelectionAsMarkup)

      // The Bluetooth functions are specified at
      // https://webbluetoothcg.github.io/web-bluetooth/tests/.

      // Calls the BluetoothChooser::EventHandler with the arguments here. Valid
      // event strings are:
      //  * "cancel" - simulates the user canceling the chooser.
      //  * "select" - simulates the user selecting a device whose device ID is
      //               in the 2nd parameter.
      .SetMethod("sendBluetoothManualChooserEvent",
                 &TestRunnerBindings::SendBluetoothManualChooserEvent)
      .SetMethod("setAcceptLanguages", &TestRunnerBindings::SetAcceptLanguages)
      .SetMethod("setAllowFileAccessFromFileURLs",
                 &TestRunnerBindings::SetAllowFileAccessFromFileURLs)
      .SetMethod("setAllowRunningOfInsecureContent",
                 &TestRunnerBindings::SetAllowRunningOfInsecureContent)
      // Controls whether all cookies should be accepted or writing cookies in a
      // third-party context is blocked:
      // - Allows all cookies when |block| is false
      // - Blocks only third-party cookies when |block| is true
      .SetMethod("setBlockThirdPartyCookies",
                 &TestRunnerBindings::SetBlockThirdPartyCookies)
      .SetMethod("setAudioData", &TestRunnerBindings::SetAudioData)
      .SetMethod("setBackingScaleFactor",
                 &TestRunnerBindings::SetBackingScaleFactor)
      // Set the bluetooth adapter while running a web test.
      .SetMethod("setBluetoothFakeAdapter",
                 &TestRunnerBindings::SetBluetoothFakeAdapter)
      // If |enable| is true, makes the Bluetooth chooser record its input and
      // wait for instructions from the test program on how to proceed.
      // Otherwise falls back to the browser's default chooser.
      .SetMethod("setBluetoothManualChooser",
                 &TestRunnerBindings::SetBluetoothManualChooser)
      .SetMethod("setCallCloseOnWebViews", &TestRunnerBindings::NotImplemented)
      .SetMethod("setCanOpenWindows", &TestRunnerBindings::SetCanOpenWindows)
      .SetMethod("setColorProfile", &TestRunnerBindings::SetColorProfile)
      .SetMethod("setCustomPolicyDelegate",
                 &TestRunnerBindings::SetCustomPolicyDelegate)
      .SetMethod("setCustomTextOutput",
                 &TestRunnerBindings::SetCustomTextOutput)
      // Setting quota to kDefaultDatabaseQuota will reset it to the default
      // value.
      .SetMethod("setDatabaseQuota", &TestRunnerBindings::SetDatabaseQuota)
      .SetMethod("setDomainRelaxationForbiddenForURLScheme",
                 &TestRunnerBindings::SetDomainRelaxationForbiddenForURLScheme)
      .SetMethod("setDumpConsoleMessages",
                 &TestRunnerBindings::SetDumpConsoleMessages)
      .SetMethod("setDumpJavaScriptDialogs",
                 &TestRunnerBindings::SetDumpJavaScriptDialogs)
      .SetMethod("setEffectiveConnectionType",
                 &TestRunnerBindings::SetEffectiveConnectionType)
      // Sets the path that should be returned when the test shows a file
      // dialog.
      .SetMethod("setFilePathForMockFileDialog",
                 &TestRunnerBindings::SetFilePathForMockFileDialog)
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
      // Calls setlocale(LC_ALL, ...) for a specified locale.
      .SetMethod("setPOSIXLocale", &TestRunnerBindings::SetPOSIXLocale)
      // Hide or show the main window. Watch for the |document.visibilityState|
      // to change in order to wait for the side effects of calling this.
      .SetMethod("setMainWindowHidden",
                 &TestRunnerBindings::SetMainWindowHidden)
      // Sets the permission's |name| to |value| for a given {origin, embedder}
      // tuple. Sends a message to the WebTestPermissionManager in order for it
      // to update its database.
      .SetMethod("setPermission", &TestRunnerBindings::SetPermission)
      .SetMethod("setPluginsAllowed", &TestRunnerBindings::SetPluginsAllowed)
      .SetMethod("setPluginsEnabled", &TestRunnerBindings::SetPluginsEnabled)
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
      // Method that controls whether pressing Tab key cycles through page
      // elements or inserts a '\t' char in text area
      .SetMethod("setTabKeyCyclesThroughElements",
                 &TestRunnerBindings::SetTabKeyCyclesThroughElements)
      // Changes the direction of text for the frame's focused element.
      .SetMethod("setTextDirection", &TestRunnerBindings::SetTextDirection)
      .SetMethod("setTextSubpixelPositioning",
                 &TestRunnerBindings::SetTextSubpixelPositioning)
      // Sets the network service-global Trust Tokens key commitments.
      // Takes a |raw_commitments| string that should be JSON-encoded according
      // to the format expected by NetworkService::SetTrustTokenKeyCommitments.
      .SetMethod("setTrustTokenKeyCommitments",
                 &TestRunnerBindings::SetTrustTokenKeyCommitments)
      .SetMethod("setUseDashboardCompatibilityMode",
                 &TestRunnerBindings::NotImplemented)
      .SetMethod("setWillSendRequestClearHeader",
                 &TestRunnerBindings::SetWillSendRequestClearHeader)
      .SetMethod("setWillSendRequestClearReferrer",
                 &TestRunnerBindings::SetWillSendRequestClearReferrer)
      .SetMethod("setWindowFocus",
                 &TestRunnerBindings::SimulateBrowserWindowFocus)
      // Simulates a click on a Web Notification.
      .SetMethod("simulateWebNotificationClick",
                 &TestRunnerBindings::SimulateWebNotificationClick)
      // Simulates closing a Web Notification.
      .SetMethod("simulateWebNotificationClose",
                 &TestRunnerBindings::SimulateWebNotificationClose)
      // Simulates a user deleting a content index entry.
      .SetMethod("simulateWebContentIndexDelete",
                 &TestRunnerBindings::SimulateWebContentIndexDelete)
      .SetMethod("textZoomIn", &TestRunnerBindings::TextZoomIn)
      .SetMethod("textZoomOut", &TestRunnerBindings::TextZoomOut)
      .SetMethod("zoomPageIn", &TestRunnerBindings::ZoomPageIn)
      .SetMethod("zoomPageOut", &TestRunnerBindings::ZoomPageOut)
      .SetMethod("setPageZoomFactor", &TestRunnerBindings::SetPageZoomFactor)
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

BoundV8Callback TestRunnerBindings::WrapV8Callback(
    v8::Local<v8::Function> v8_callback,
    std::vector<v8::Local<v8::Value>> args_to_bind) {
  auto persistent_callback = v8::UniquePersistent<v8::Function>(
      blink::MainThreadIsolate(), std::move(v8_callback));

  std::vector<v8::UniquePersistent<v8::Value>> persistent_args;
  persistent_args.reserve(args_to_bind.size());
  for (auto& arg : args_to_bind)
    persistent_args.emplace_back(blink::MainThreadIsolate(), std::move(arg));

  return base::BindOnce(
      &TestRunnerBindings::InvokeV8Callback, weak_ptr_factory_.GetWeakPtr(),
      std::move(persistent_callback), std::move(persistent_args));
}

base::OnceClosure TestRunnerBindings::WrapV8Closure(
    v8::Local<v8::Function> v8_callback,
    std::vector<v8::Local<v8::Value>> args_to_bind) {
  return base::BindOnce(
      WrapV8Callback(std::move(v8_callback), std::move(args_to_bind)),
      NoV8Args());
}

void TestRunnerBindings::PostV8Callback(
    v8::Local<v8::Function> v8_callback,
    std::vector<v8::Local<v8::Value>> args) {
  const auto& task_runner =
      GetWebFrame()->GetTaskRunner(blink::TaskType::kInternalTest);
  task_runner->PostTask(FROM_HERE,
                        WrapV8Closure(std::move(v8_callback), std::move(args)));
}

void TestRunnerBindings::InvokeV8Callback(
    v8::UniquePersistent<v8::Function> callback,
    std::vector<v8::UniquePersistent<v8::Value>> bound_args,
    const std::vector<v8::Local<v8::Value>>& runtime_args) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = GetWebFrame()->MainWorldScriptContext();
  CHECK(!context.IsEmpty());
  v8::Context::Scope context_scope(context);

  std::vector<v8::Local<v8::Value>> local_args;
  for (auto& arg : bound_args)
    local_args.push_back(v8::Local<v8::Value>::New(isolate, std::move(arg)));
  for (const auto& arg : runtime_args)
    local_args.push_back(arg);

  GetWebFrame()->CallFunctionEvenIfScriptDisabled(
      v8::Local<v8::Function>::New(isolate, std::move(callback)),
      context->Global(), local_args.size(), local_args.data());
}

void TestRunnerBindings::LogToStderr(const std::string& output) {
  if (invalid_)
    return;
  TRACE_EVENT1("shell", "TestRunner::LogToStderr", "output", output);
  LOG(ERROR) << output;
}

void TestRunnerBindings::NotifyDone() {
  if (invalid_)
    return;
  runner_->NotifyDone();
}

void TestRunnerBindings::WaitUntilDone() {
  if (invalid_)
    return;
  runner_->WaitUntilDone();
}

void TestRunnerBindings::QueueBackNavigation(int how_far_back) {
  if (invalid_)
    return;
  runner_->QueueBackNavigation(how_far_back);
}

void TestRunnerBindings::QueueForwardNavigation(int how_far_forward) {
  if (invalid_)
    return;
  runner_->QueueForwardNavigation(how_far_forward);
}

void TestRunnerBindings::QueueReload() {
  if (invalid_)
    return;
  runner_->QueueReload();
}

void TestRunnerBindings::QueueLoadingScript(const std::string& script) {
  if (invalid_)
    return;
  runner_->QueueLoadingScript(script, weak_ptr_factory_.GetWeakPtr());
}

void TestRunnerBindings::QueueNonLoadingScript(const std::string& script) {
  if (invalid_)
    return;
  runner_->QueueNonLoadingScript(script, weak_ptr_factory_.GetWeakPtr());
}

void TestRunnerBindings::QueueLoad(gin::Arguments* args) {
  if (invalid_)
    return;
  std::string url;
  std::string target;
  args->GetNext(&url);
  args->GetNext(&target);
  runner_->QueueLoad(GURL(GetWebFrame()->GetDocument().Url()), url, target);
}

void TestRunnerBindings::SetCustomPolicyDelegate(gin::Arguments* args) {
  if (invalid_)
    return;
  runner_->SetCustomPolicyDelegate(args);
}

void TestRunnerBindings::WaitForPolicyDelegate() {
  if (invalid_)
    return;
  runner_->WaitForPolicyDelegate();
}

int TestRunnerBindings::WindowCount() {
  if (invalid_)
    return 0;
  return runner_->InProcessWindowCount();
}

void TestRunnerBindings::SetTabKeyCyclesThroughElements(
    bool tab_key_cycles_through_elements) {
  if (invalid_)
    return;
  blink::WebView* web_view = GetWebFrame()->View();
  web_view->SetTabKeyCyclesThroughElements(tab_key_cycles_through_elements);
}

void TestRunnerBindings::ExecCommand(gin::Arguments* args) {
  if (invalid_)
    return;

  std::string command;
  args->GetNext(&command);

  std::string value;
  if (args->Length() >= 3) {
    // Ignore the second parameter (which is userInterface)
    // since this command emulates a manual action.
    args->Skip();
    args->GetNext(&value);
  }

  // Note: webkit's version does not return the boolean, so neither do we.
  GetWebFrame()->ExecuteCommand(blink::WebString::FromUTF8(command),
                                blink::WebString::FromUTF8(value));
}

void TestRunnerBindings::TriggerTestInspectorIssue(gin::Arguments* args) {
  if (invalid_)
    return;
  GetWebFrame()->AddInspectorIssue(
      blink::mojom::InspectorIssueCode::kSameSiteCookieIssue);
}

bool TestRunnerBindings::IsCommandEnabled(const std::string& command) {
  if (invalid_)
    return false;
  return GetWebFrame()->IsCommandEnabled(blink::WebString::FromUTF8(command));
}

void TestRunnerBindings::SetDomainRelaxationForbiddenForURLScheme(
    bool forbidden,
    const std::string& scheme) {
  if (invalid_)
    return;
  blink::SetDomainRelaxationForbiddenForTest(
      forbidden, blink::WebString::FromUTF8(scheme));
}

void TestRunnerBindings::SetDumpConsoleMessages(bool enabled) {
  if (invalid_)
    return;
  runner_->SetDumpConsoleMessages(enabled);
}

void TestRunnerBindings::SetDumpJavaScriptDialogs(bool enabled) {
  if (invalid_)
    return;
  runner_->SetDumpJavaScriptDialogs(enabled);
}

void TestRunnerBindings::SetEffectiveConnectionType(
    const std::string& connection_type) {
  if (invalid_)
    return;

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

base::FilePath::StringType TestRunnerBindings::GetWritableDirectory() {
  if (invalid_)
    return {};
  base::FilePath result;
  runner_->GetWebTestControlHostRemote()->GetWritableDirectory(&result);
  return result.value();
}

void TestRunnerBindings::SetFilePathForMockFileDialog(
    const base::FilePath::StringType& path) {
  if (invalid_)
    return;
  runner_->GetWebTestControlHostRemote()->SetFilePathForMockFileDialog(
      base::FilePath(path));
}

void TestRunnerBindings::SetMockSpellCheckerEnabled(bool enabled) {
  if (invalid_)
    return;
  spell_check_->SetEnabled(enabled);
}

void TestRunnerBindings::SetSpellCheckResolvedCallback(
    v8::Local<v8::Function> callback) {
  if (invalid_)
    return;
  spell_check_->SetSpellCheckResolvedCallback(callback);
}

void TestRunnerBindings::RemoveSpellCheckResolvedCallback() {
  if (invalid_)
    return;
  spell_check_->RemoveSpellCheckResolvedCallback();
}

v8::Local<v8::Value>
TestRunnerBindings::EvaluateScriptInIsolatedWorldAndReturnValue(
    int world_id,
    const std::string& script) {
  if (invalid_ || world_id <= 0 || world_id >= (1 << 29))
    return {};

  blink::WebScriptSource source = blink::WebString::FromUTF8(script);
  return GetWebFrame()->ExecuteScriptInIsolatedWorldAndReturnValue(world_id,
                                                                   source);
}

void TestRunnerBindings::EvaluateScriptInIsolatedWorld(
    int world_id,
    const std::string& script) {
  if (invalid_ || world_id <= 0 || world_id >= (1 << 29))
    return;

  blink::WebScriptSource source = blink::WebString::FromUTF8(script);
  GetWebFrame()->ExecuteScriptInIsolatedWorld(world_id, source);
}

void TestRunnerBindings::SetIsolatedWorldInfo(
    int world_id,
    v8::Local<v8::Value> security_origin,
    v8::Local<v8::Value> content_security_policy) {
  if (invalid_)
    return;

  if (world_id <= content::ISOLATED_WORLD_ID_GLOBAL ||
      world_id >= blink::IsolatedWorldId::kEmbedderWorldIdLimit) {
    return;
  }

  if (!security_origin->IsString() && !security_origin->IsNull())
    return;

  if (!content_security_policy->IsString() &&
      !content_security_policy->IsNull()) {
    return;
  }

  // If |content_security_policy| is specified, |security_origin| must also be
  // specified.
  if (content_security_policy->IsString() && security_origin->IsNull())
    return;

  blink::WebIsolatedWorldInfo info;
  if (security_origin->IsString()) {
    info.security_origin = blink::WebSecurityOrigin::CreateFromString(
        web_test_string_util::V8StringToWebString(
            blink::MainThreadIsolate(), security_origin.As<v8::String>()));
  }

  if (content_security_policy->IsString()) {
    info.content_security_policy = web_test_string_util::V8StringToWebString(
        blink::MainThreadIsolate(), content_security_policy.As<v8::String>());
  }

  // Clear the document->isolated world CSP mapping.
  GetWebFrame()->ClearIsolatedWorldCSPForTesting(world_id);

  blink::SetIsolatedWorldInfo(world_id, info);
}

void TestRunnerBindings::AddOriginAccessAllowListEntry(
    const std::string& source_origin,
    const std::string& destination_protocol,
    const std::string& destination_host,
    bool allow_destination_subdomains) {
  if (invalid_)
    return;

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

void TestRunnerBindings::InsertStyleSheet(const std::string& source_code) {
  if (invalid_)
    return;
  GetWebFrame()->GetDocument().InsertStyleSheet(
      blink::WebString::FromUTF8(source_code));
}

bool TestRunnerBindings::FindString(
    const std::string& search_text,
    const std::vector<std::string>& options_array) {
  if (invalid_)
    return false;

  bool match_case = true;
  bool forward = true;
  bool new_session = false;
  bool wrap_around = false;
  bool async = false;
  for (const auto& option : options_array) {
    if (option == "CaseInsensitive")
      match_case = false;
    else if (option == "Backwards")
      forward = false;
    else if (option == "StartInSelection")
      new_session = true;
    else if (option == "WrapAround")
      wrap_around = true;
    else if (option == "Async")
      async = true;
  }

  const bool find_result = GetWebFrame()->FindForTesting(
      0, blink::WebString::FromUTF8(search_text), match_case, forward,
      new_session, false /* force */, wrap_around, async);
  return find_result;
}

std::string TestRunnerBindings::SelectionAsMarkup() {
  if (invalid_)
    return {};
  return GetWebFrame()->SelectionAsMarkup().Utf8();
}

void TestRunnerBindings::SetTextSubpixelPositioning(bool value) {
  if (invalid_)
    return;
  runner_->SetTextSubpixelPositioning(value);
}

void TestRunnerBindings::SetTrustTokenKeyCommitments(
    const std::string& raw_commitments,
    v8::Local<v8::Function> v8_callback) {
  if (invalid_)
    return;

  runner_->GetWebTestControlHostRemote()->SetTrustTokenKeyCommitments(
      raw_commitments, WrapV8Closure(std::move(v8_callback)));
}

void TestRunnerBindings::SetMainWindowHidden(bool hidden) {
  if (invalid_)
    return;
  runner_->GetWebTestControlHostRemote()->SetMainWindowHidden(hidden);
}

void TestRunnerBindings::SetTextDirection(const std::string& direction_name) {
  if (invalid_)
    return;

  // Map a direction name to a base::i18n::TextDirection value.
  base::i18n::TextDirection direction;
  if (direction_name == "auto")
    direction = base::i18n::TextDirection::UNKNOWN_DIRECTION;
  else if (direction_name == "rtl")
    direction = base::i18n::TextDirection::RIGHT_TO_LEFT;
  else if (direction_name == "ltr")
    direction = base::i18n::TextDirection::LEFT_TO_RIGHT;
  else
    return;

  GetWebFrame()->SetTextDirectionForTesting(direction);
}

void TestRunnerBindings::UseUnfortunateSynchronousResizeMode() {
  if (invalid_)
    return;
  runner_->UseUnfortunateSynchronousResizeMode();
}

void TestRunnerBindings::EnableAutoResizeMode(int min_width,
                                              int min_height,
                                              int max_width,
                                              int max_height) {
  if (invalid_)
    return;
  // Early out instead of CHECK() to avoid poking the fuzzer bear.
  if (!frame_->IsMainFrame())
    return;
  if (max_width <= 0 || max_height <= 0)
    return;

  blink::WebView* web_view = GetWebFrame()->View();

  gfx::Size min_size(min_width, min_height);
  gfx::Size max_size(max_width, max_height);
  web_view->EnableAutoResizeForTesting(min_size, max_size);
}

void TestRunnerBindings::DisableAutoResizeMode(int new_width, int new_height) {
  if (invalid_)
    return;
  // Early out instead of CHECK() to avoid poking the fuzzer bear.
  if (!frame_->IsMainFrame())
    return;
  if (new_width <= 0 || new_height <= 0)
    return;

  RenderWidget* widget = frame_->GetLocalRootRenderWidget();

  gfx::Size new_size(new_width, new_height);
  blink::WebView* web_view = GetWebFrame()->View();
  web_view->DisableAutoResizeForTesting(new_size);

  gfx::Rect window_rect(widget->GetWebWidget()->WindowRect().origin(),
                        new_size);
  web_view->SetWindowRectSynchronouslyForTesting(window_rect);
}

void TestRunnerBindings::SetMockScreenOrientation(
    const std::string& orientation) {
  if (invalid_)
    return;
  runner_->SetMockScreenOrientation(frame_->GetWebViewTestProxy(), orientation);
}

void TestRunnerBindings::DisableMockScreenOrientation() {
  if (invalid_)
    return;
  runner_->DisableMockScreenOrientation(frame_->GetWebViewTestProxy());
}

void TestRunnerBindings::SetDisallowedSubresourcePathSuffixes(
    std::vector<std::string> suffixes,
    bool block_subresources) {
  if (invalid_)
    return;
  GetWebFrame()->GetDocumentLoader()->SetSubresourceFilter(
      new FakeSubresourceFilter(std::move(suffixes), block_subresources));
}

void TestRunnerBindings::SetPopupBlockingEnabled(bool block_popups) {
  if (invalid_)
    return;
  runner_->GetWebTestControlHostRemote()->SetPopupBlockingEnabled(block_popups);
}

void TestRunnerBindings::SetJavaScriptCanAccessClipboard(bool can_access) {
  if (invalid_)
    return;

  // WebPreferences aren't propagated between frame tree fragments, so only
  // allow this in the main frame.
  // Early out instead of CHECK() to avoid poking the fuzzer bear.
  if (!frame_->IsMainFrame())
    return;

  prefs_.java_script_can_access_clipboard = can_access;
  runner_->OnTestPreferencesChanged(prefs_, frame_);
}

void TestRunnerBindings::SetAllowFileAccessFromFileURLs(bool allow) {
  if (invalid_)
    return;

  // WebPreferences aren't propagated between frame tree fragments, so only
  // allow this in the main frame.
  // Early out instead of CHECK() to avoid poking the fuzzer bear.
  if (!frame_->IsMainFrame())
    return;

  prefs_.allow_file_access_from_file_urls = allow;
  runner_->OnTestPreferencesChanged(prefs_, frame_);
}

void TestRunnerBindings::OverridePreference(gin::Arguments* args) {
  if (invalid_)
    return;

  if (args->Length() != 2) {
    args->ThrowTypeError("overridePreference expects 2 arguments");
    return;
  }

  std::string key;
  if (!args->GetNext(&key)) {
    args->ThrowError();
    return;
  }

  if (key == "WebKitDefaultFontSize") {
    ConvertAndSet(args, &prefs_.default_font_size);
  } else if (key == "WebKitMinimumFontSize") {
    ConvertAndSet(args, &prefs_.minimum_font_size);
  } else if (key == "WebKitDefaultTextEncodingName") {
    ConvertAndSet(args, &prefs_.default_text_encoding_name);
  } else if (key == "WebKitJavaScriptEnabled") {
    ConvertAndSet(args, &prefs_.java_script_enabled);
  } else if (key == "WebKitSupportsMultipleWindows") {
    ConvertAndSet(args, &prefs_.supports_multiple_windows);
  } else if (key == "WebKitDisplayImagesKey") {
    ConvertAndSet(args, &prefs_.loads_images_automatically);
  } else if (key == "WebKitPluginsEnabled") {
    ConvertAndSet(args, &prefs_.plugins_enabled);
  } else if (key == "WebKitTabToLinksPreferenceKey") {
    ConvertAndSet(args, &prefs_.tabs_to_links);
  } else if (key == "WebKitCSSGridLayoutEnabled") {
    ConvertAndSet(args, &prefs_.experimental_css_grid_layout_enabled);
  } else if (key == "WebKitHyperlinkAuditingEnabled") {
    ConvertAndSet(args, &prefs_.hyperlink_auditing_enabled);
  } else if (key == "WebKitEnableCaretBrowsing") {
    ConvertAndSet(args, &prefs_.caret_browsing_enabled);
  } else if (key == "WebKitAllowRunningInsecureContent") {
    ConvertAndSet(args, &prefs_.allow_running_of_insecure_content);
  } else if (key == "WebKitDisableReadingFromCanvas") {
    ConvertAndSet(args, &prefs_.disable_reading_from_canvas);
  } else if (key == "WebKitStrictMixedContentChecking") {
    ConvertAndSet(args, &prefs_.strict_mixed_content_checking);
  } else if (key == "WebKitStrictPowerfulFeatureRestrictions") {
    ConvertAndSet(args, &prefs_.strict_powerful_feature_restrictions);
  } else if (key == "WebKitShouldRespectImageOrientation") {
    ConvertAndSet(args, &prefs_.should_respect_image_orientation);
  } else if (key == "WebKitWebSecurityEnabled") {
    ConvertAndSet(args, &prefs_.web_security_enabled);
  } else if (key == "WebKitSpatialNavigationEnabled") {
    ConvertAndSet(args, &prefs_.spatial_navigation_enabled);
  } else {
    args->ThrowTypeError("Invalid name for preference: " + key);
  }

  runner_->OnTestPreferencesChanged(prefs_, frame_);
}

void TestRunnerBindings::SetAcceptLanguages(
    const std::string& accept_languages) {
  if (invalid_)
    return;
  runner_->SetAcceptLanguages(accept_languages);
}

void TestRunnerBindings::SetPluginsEnabled(bool enabled) {
  if (invalid_)
    return;

  // WebPreferences aren't propagated between frame tree fragments, so only
  // allow this in the main frame.
  // Early out instead of CHECK() to avoid poking the fuzzer bear.
  if (!frame_->IsMainFrame())
    return;

  prefs_.plugins_enabled = enabled;
  runner_->OnTestPreferencesChanged(prefs_, frame_);
}

void TestRunnerBindings::DumpEditingCallbacks() {
  if (invalid_)
    return;
  runner_->DumpEditingCallbacks();
}

void TestRunnerBindings::DumpAsMarkup() {
  if (invalid_)
    return;
  runner_->DumpAsMarkup();
}

void TestRunnerBindings::DumpAsText() {
  if (invalid_)
    return;
  runner_->DumpAsText();
}

void TestRunnerBindings::DumpAsTextWithPixelResults() {
  if (invalid_)
    return;
  runner_->DumpAsTextWithPixelResults();
}

void TestRunnerBindings::DumpAsLayout() {
  if (invalid_)
    return;
  runner_->DumpAsLayout();
}

void TestRunnerBindings::DumpAsLayoutWithPixelResults() {
  if (invalid_)
    return;
  runner_->DumpAsLayoutWithPixelResults();
}

void TestRunnerBindings::DumpChildFrames() {
  if (invalid_)
    return;
  runner_->DumpChildFrames();
}

void TestRunnerBindings::DumpIconChanges() {
  if (invalid_)
    return;
  runner_->DumpIconChanges();
}

void TestRunnerBindings::SetAudioData(const gin::ArrayBufferView& view) {
  if (invalid_)
    return;
  runner_->SetAudioData(view);
}

void TestRunnerBindings::DumpFrameLoadCallbacks() {
  if (invalid_)
    return;
  runner_->DumpFrameLoadCallbacks();
}

void TestRunnerBindings::DumpPingLoaderCallbacks() {
  if (invalid_)
    return;
  runner_->DumpPingLoaderCallbacks();
}

void TestRunnerBindings::DumpUserGestureInFrameLoadCallbacks() {
  if (invalid_)
    return;
  runner_->DumpUserGestureInFrameLoadCallbacks();
}

void TestRunnerBindings::DumpTitleChanges() {
  if (invalid_)
    return;
  runner_->DumpTitleChanges();
}

void TestRunnerBindings::DumpCreateView() {
  if (invalid_)
    return;
  runner_->DumpCreateView();
}

void TestRunnerBindings::SetCanOpenWindows() {
  if (invalid_)
    return;
  runner_->SetCanOpenWindows();
}

void TestRunnerBindings::SetImagesAllowed(bool allowed) {
  if (invalid_)
    return;
  runner_->SetImagesAllowed(allowed);
}

void TestRunnerBindings::SetScriptsAllowed(bool allowed) {
  if (invalid_)
    return;
  runner_->SetScriptsAllowed(allowed);
}

void TestRunnerBindings::SetStorageAllowed(bool allowed) {
  if (invalid_)
    return;
  runner_->SetStorageAllowed(allowed);
}

void TestRunnerBindings::SetPluginsAllowed(bool allowed) {
  if (invalid_)
    return;
  // This only modifies the local process, and is used to verify behaviour based
  // on settings, but does not test propagation of settings across renderers.
  blink::WebView* web_view = GetWebFrame()->View();
  web_view->GetSettings()->SetPluginsEnabled(allowed);
}

void TestRunnerBindings::SetAllowRunningOfInsecureContent(bool allowed) {
  if (invalid_)
    return;
  runner_->SetAllowRunningOfInsecureContent(allowed);
}

void TestRunnerBindings::DumpPermissionClientCallbacks() {
  if (invalid_)
    return;
  runner_->DumpPermissionClientCallbacks();
}

void TestRunnerBindings::DumpBackForwardList() {
  if (invalid_)
    return;
  runner_->DumpBackForwardList();
}

void TestRunnerBindings::DumpSelectionRect() {
  if (invalid_)
    return;
  runner_->DumpSelectionRect();
}

void TestRunnerBindings::SetPrinting() {
  if (invalid_)
    return;
  runner_->SetPrinting();
}

void TestRunnerBindings::SetPrintingForFrame(const std::string& frame_name) {
  if (invalid_)
    return;
  runner_->SetPrintingForFrame(frame_name);
}

void TestRunnerBindings::ClearTrustTokenState(
    v8::Local<v8::Function> v8_callback) {
  if (invalid_)
    return;
  runner_->GetWebTestControlHostRemote()->ClearTrustTokenState(
      WrapV8Closure(std::move(v8_callback)));
}

void TestRunnerBindings::SetShouldGeneratePixelResults(bool value) {
  if (invalid_)
    return;
  runner_->SetShouldGeneratePixelResults(value);
}

void TestRunnerBindings::SetShouldStayOnPageAfterHandlingBeforeUnload(
    bool value) {
  if (invalid_)
    return;
  runner_->SetShouldStayOnPageAfterHandlingBeforeUnload(value);
}

void TestRunnerBindings::SetWillSendRequestClearHeader(
    const std::string& header) {
  if (invalid_)
    return;
  runner_->SetWillSendRequestClearHeader(header);
}

void TestRunnerBindings::SetWillSendRequestClearReferrer() {
  if (invalid_)
    return;
  runner_->SetWillSendRequestClearReferrer();
}

void TestRunnerBindings::WaitUntilExternalURLLoad() {
  if (invalid_)
    return;
  runner_->WaitUntilExternalURLLoad();
}

void TestRunnerBindings::DumpDragImage() {
  if (invalid_)
    return;
  runner_->DumpDragImage();
}

void TestRunnerBindings::DumpNavigationPolicy() {
  if (invalid_)
    return;
  runner_->DumpNavigationPolicy();
}

void TestRunnerBindings::ClearAllDatabases() {
  if (invalid_)
    return;
  runner_->GetWebTestControlHostRemote()->ClearAllDatabases();
}

void TestRunnerBindings::SetDatabaseQuota(int quota) {
  if (invalid_)
    return;
  runner_->GetWebTestControlHostRemote()->SetDatabaseQuota(quota);
}

void TestRunnerBindings::SetBlockThirdPartyCookies(bool block) {
  if (invalid_)
    return;
  runner_->GetWebTestControlHostRemote()->BlockThirdPartyCookies(block);
}

void TestRunnerBindings::SimulateBrowserWindowFocus(bool value) {
  if (invalid_)
    return;
  // This simulates the browser focusing or unfocusing the window,
  // but does so only for this renderer process. Other frame tree
  // fragments in other processes do not hear about the change. To
  // do so the focus change would need to go through window.focus()
  // and then watch for the focus event or do a round trip to the
  // browser.
  // TODO(danakj): This does not appear to do the same thing as the
  // browser does, because actually moving focus causes different test
  // results in tests such as editing/selection/4975120.html with the
  // inner frame not getting its caret back.
  // Early out instead of CHECK() to avoid poking the fuzzer bear.
  if (!frame_->IsMainFrame())
    return;
  runner_->FocusWindow(frame_, value);
}

std::string TestRunnerBindings::PathToLocalResource(const std::string& path) {
  if (invalid_)
    return {};
  return RewriteFileURLToLocalResource(path).GetString().Utf8();
}

void TestRunnerBindings::SetBackingScaleFactor(
    double value,
    v8::Local<v8::Function> v8_callback) {
  if (invalid_)
    return;

  // Limit backing scale factor to something low - 15x. Without
  // this limit, arbitrarily large values can be used, which can lead to
  // crashes and other problems. Examples of problems:
  // gfx::Size::GetCheckedArea crashes with a size which overflows int;
  // GLES2DecoderImpl::TexStorageImpl fails with "dimensions out of range"; GL
  // ERROR :GL_OUT_OF_MEMORY. See https://crbug.com/899482 or
  // https://crbug.com/900271
  double limited_value = fmin(15, value);

  frame_->GetLocalRootWebFrameWidget()->SetDeviceScaleFactorForTesting(
      limited_value);

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);

  WrapV8Callback(std::move(v8_callback))
      .Run({
          // TODO(oshima): remove this callback argument when all platforms are
          // migrated to use-zoom-for-dsf by default.
          v8::Boolean::New(isolate, IsUseZoomForDSFEnabled()),
      });
}

void TestRunnerBindings::SetColorProfile(const std::string& name,
                                         v8::Local<v8::Function> v8_callback) {
  if (invalid_)
    return;

  gfx::ColorSpace color_space;
  if (name == "genericRGB") {
    color_space = gfx::ICCProfileForTestingGenericRGB().GetColorSpace();
  } else if (name == "sRGB") {
    color_space = gfx::ColorSpace::CreateSRGB();
  } else if (name == "colorSpin") {
    color_space = gfx::ICCProfileForTestingColorSpin().GetColorSpace();
  } else if (name == "adobeRGB") {
    color_space = gfx::ICCProfileForTestingAdobeRGB().GetColorSpace();
  }
  GetWebFrame()->View()->SetDeviceColorSpaceForTesting(color_space);

  WrapV8Closure(std::move(v8_callback)).Run();
}

void TestRunnerBindings::SetBluetoothFakeAdapter(
    const std::string& adapter_name,
    v8::Local<v8::Function> v8_callback) {
  if (invalid_)
    return;
  runner_->GetBluetoothFakeAdapterSetter().Set(
      adapter_name, WrapV8Closure(std::move(v8_callback)));
}

void TestRunnerBindings::SetBluetoothManualChooser(bool enable) {
  if (invalid_)
    return;
  runner_->GetWebTestControlHostRemote()->SetBluetoothManualChooser(enable);
}

static void GetBluetoothManualChooserEventsReply(
    base::WeakPtr<TestRunnerBindings> test_runner,
    blink::WebLocalFrame* frame,
    BoundV8Callback callback,
    const std::vector<std::string>& events) {
  if (!test_runner)  // This guards the validity of the |frame|.
    return;

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);

  // gin::TryConvertToV8() requires a v8::Context.
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  CHECK(!context.IsEmpty());
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Value> arg;
  bool converted = gin::TryConvertToV8(isolate, events, &arg);
  CHECK(converted);

  std::move(callback).Run({
      arg,
  });
}

void TestRunnerBindings::GetBluetoothManualChooserEvents(
    v8::Local<v8::Function> callback) {
  if (invalid_)
    return;
  runner_->GetWebTestControlHostRemote()->GetBluetoothManualChooserEvents(
      base::BindOnce(&GetBluetoothManualChooserEventsReply,
                     weak_ptr_factory_.GetWeakPtr(), GetWebFrame(),
                     WrapV8Callback(std::move(callback))));
}

void TestRunnerBindings::SendBluetoothManualChooserEvent(
    const std::string& event,
    const std::string& argument) {
  if (invalid_)
    return;
  runner_->GetWebTestControlHostRemote()->SendBluetoothManualChooserEvent(
      event, argument);
}

void TestRunnerBindings::SetPOSIXLocale(const std::string& locale) {
  if (invalid_)
    return;
  setlocale(LC_ALL, locale.c_str());
  // Number to string conversions require C locale, regardless of what
  // all the other subsystems are set to.
  setlocale(LC_NUMERIC, "C");
}

void TestRunnerBindings::SimulateWebNotificationClick(gin::Arguments* args) {
  if (invalid_)
    return;

  DCHECK_GE(args->Length(), 1);

  std::string title;
  int action_index = std::numeric_limits<int32_t>::min();
  base::Optional<base::string16> reply;

  if (!args->GetNext(&title)) {
    args->ThrowError();
    return;
  }

  // Optional |action_index| argument.
  if (args->Length() >= 2) {
    if (!args->GetNext(&action_index)) {
      args->ThrowError();
      return;
    }
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

  runner_->GetWebTestControlHostRemote()->SimulateWebNotificationClick(
      title, action_index, reply);
}

void TestRunnerBindings::SimulateWebNotificationClose(const std::string& title,
                                                      bool by_user) {
  if (invalid_)
    return;
  runner_->GetWebTestControlHostRemote()->SimulateWebNotificationClose(title,
                                                                       by_user);
}

void TestRunnerBindings::SimulateWebContentIndexDelete(const std::string& id) {
  if (invalid_)
    return;
  runner_->GetWebTestControlHostRemote()->SimulateWebContentIndexDelete(id);
}

void TestRunnerBindings::SetHighlightAds() {
  if (invalid_)
    return;
  blink::WebView* web_view = GetWebFrame()->View();
  web_view->GetSettings()->SetHighlightAds(true);
}

void TestRunnerBindings::AddWebPageOverlay() {
  if (invalid_)
    return;
  // Early out instead of CHECK() to avoid poking the fuzzer bear.
  if (!frame_->IsMainFrame())
    return;
  frame_->GetLocalRootWebFrameWidget()->SetMainFrameOverlayColor(SK_ColorCYAN);
}

void TestRunnerBindings::RemoveWebPageOverlay() {
  if (invalid_)
    return;
  // Early out instead of CHECK() to avoid poking the fuzzer bear.
  if (!frame_->IsMainFrame())
    return;
  frame_->GetLocalRootWebFrameWidget()->SetMainFrameOverlayColor(
      SK_ColorTRANSPARENT);
}

void TestRunnerBindings::UpdateAllLifecyclePhasesAndComposite() {
  if (invalid_)
    return;
  frame_->GetLocalRootRenderWidget()->RequestPresentation(base::DoNothing());
}

static void UpdateAllLifecyclePhasesAndCompositeThenReply(
    base::OnceClosure callback,
    const gfx::PresentationFeedback& feedback) {
  std::move(callback).Run();
}

void TestRunnerBindings::UpdateAllLifecyclePhasesAndCompositeThen(
    v8::Local<v8::Function> v8_callback) {
  if (invalid_)
    return;
  frame_->GetLocalRootRenderWidget()->RequestPresentation(
      base::BindOnce(&UpdateAllLifecyclePhasesAndCompositeThenReply,
                     WrapV8Closure(std::move(v8_callback))));
}

void TestRunnerBindings::SetAnimationRequiresRaster(bool do_raster) {
  if (invalid_)
    return;
  runner_->SetAnimationRequiresRaster(do_raster);
}

static void GetManifestReply(BoundV8Callback callback,
                             const blink::WebURL& manifest_url,
                             const blink::Manifest& manifest) {
  std::move(callback).Run(NoV8Args());
}

void TestRunnerBindings::GetManifestThen(v8::Local<v8::Function> v8_callback) {
  if (invalid_)
    return;
  blink::WebManifestManager::RequestManifestForTesting(
      GetWebFrame(),
      base::BindOnce(GetManifestReply, WrapV8Callback(std::move(v8_callback))));
}

void TestRunnerBindings::CapturePrintingPixelsThen(
    v8::Local<v8::Function> v8_callback) {
  if (invalid_)
    return;
  SkBitmap bitmap = PrintFrameToBitmap(GetWebFrame());

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);

  // ConvertBitmapToV8() requires a v8::Context.
  v8::Local<v8::Context> context = GetWebFrame()->MainWorldScriptContext();
  CHECK(!context.IsEmpty());
  v8::Context::Scope context_scope(context);

  WrapV8Callback(std::move(v8_callback))
      .Run({
          ConvertBitmapToV8(context_scope, bitmap),
      });
}

void TestRunnerBindings::CheckForLeakedWindows() {
  if (invalid_)
    return;
  runner_->GetWebTestControlHostRemote()->CheckForLeakedWindows();
}

void TestRunnerBindings::CopyImageThen(int x,
                                       int y,
                                       v8::Local<v8::Function> v8_callback) {
  mojo::Remote<blink::mojom::ClipboardHost> remote_clipboard;
  frame_->GetBrowserInterfaceBroker()->GetInterface(
      remote_clipboard.BindNewPipeAndPassReceiver());

  uint64_t sequence_number_before = 0;
  remote_clipboard->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste,
                                      &sequence_number_before);
  GetWebFrame()->CopyImageAtForTesting(gfx::Point(x, y));
  uint64_t sequence_number_after = 0;
  while (sequence_number_before == sequence_number_after) {
    remote_clipboard->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste,
                                        &sequence_number_after);
  }

  SkBitmap bitmap;
  remote_clipboard->ReadImage(ui::ClipboardBuffer::kCopyPaste, &bitmap);

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = GetWebFrame()->MainWorldScriptContext();
  CHECK(!context.IsEmpty());
  v8::Context::Scope context_scope(context);

  WrapV8Callback(std::move(v8_callback))
      .Run(ConvertBitmapToV8(context_scope, std::move(bitmap)));
}

void TestRunnerBindings::SetCustomTextOutput(const std::string& output) {
  if (invalid_)
    return;
  runner_->SetCustomTextOutput(output);
}

void TestRunnerBindings::SetPermission(const std::string& name,
                                       const std::string& value,
                                       const std::string& origin,
                                       const std::string& embedding_origin) {
  if (invalid_)
    return;
  runner_->GetWebTestControlHostRemote()->SetPermission(
      name, blink::ToPermissionStatus(value), GURL(origin),
      GURL(embedding_origin));
}

static void DispatchBeforeInstallPromptEventReply(BoundV8Callback callback,
                                                  bool cancelled) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  std::move(callback).Run({
      v8::Boolean::New(isolate, cancelled),
  });
}

void TestRunnerBindings::DispatchBeforeInstallPromptEvent(
    const std::vector<std::string>& event_platforms,
    v8::Local<v8::Function> v8_callback) {
  if (invalid_)
    return;
  app_banner_service_ = std::make_unique<AppBannerService>();
  frame_->BindLocalInterface(blink::mojom::AppBannerController::Name_,
                             app_banner_service_->controller()
                                 .BindNewPipeAndPassReceiver()
                                 .PassPipe());

  app_banner_service_->SendBannerPromptRequest(
      event_platforms, base::BindOnce(&DispatchBeforeInstallPromptEventReply,
                                      WrapV8Callback(std::move(v8_callback))));
}

void TestRunnerBindings::ResolveBeforeInstallPromptPromise(
    const std::string& platform) {
  if (invalid_)
    return;
  if (app_banner_service_) {
    app_banner_service_->ResolvePromise(platform);
    app_banner_service_.reset();
  }
}

void TestRunnerBindings::RunIdleTasks(v8::Local<v8::Function> v8_callback) {
  if (invalid_)
    return;
  blink::scheduler::WebThreadScheduler* scheduler =
      content::RenderThreadImpl::current()->GetWebMainThreadScheduler();
  blink::scheduler::RunIdleTasksForTesting(
      scheduler, WrapV8Closure(std::move(v8_callback)));
}

std::string TestRunnerBindings::PlatformName() {
  if (invalid_)
    return {};
  return runner_->platform_name_;
}

void TestRunnerBindings::TextZoomIn() {
  if (invalid_)
    return;

  // This may only be run from the main frame, as the user modifies this at the
  // top level.
  // Early out instead of CHECK() to avoid poking the fuzzer bear.
  if (!frame_->IsMainFrame())
    return;

  // TODO(danakj): This should be an async call through the browser process, but
  // note this is an AndroidWebView feature which is not part of the content (or
  // content_shell) APIs.
  blink::WebFrameWidget* widget = frame_->GetLocalRootWebFrameWidget();
  widget->SetTextZoomFactor(widget->TextZoomFactor() * 1.2f);
}

void TestRunnerBindings::TextZoomOut() {
  if (invalid_)
    return;

  // This may only be run from the main frame, as the user modifies this at the
  // top level.
  // Early out instead of CHECK() to avoid poking the fuzzer bear.
  if (!frame_->IsMainFrame())
    return;

  // TODO(danakj): This should be an async call through the browser process, but
  // note this is an AndroidWebView feature which is not part of the content (or
  // content_shell) APIs.
  blink::WebFrameWidget* widget = frame_->GetLocalRootWebFrameWidget();
  widget->SetTextZoomFactor(widget->TextZoomFactor() / 1.2f);
}

void TestRunnerBindings::ZoomPageIn() {
  if (invalid_)
    return;

  // This may only be run from the main frame, as the user modifies this at the
  // top level.
  // Early out instead of CHECK() to avoid poking the fuzzer bear.
  if (!frame_->IsMainFrame())
    return;

  blink::WebView* web_view = GetWebFrame()->View();
  // TODO(danakj): This should be an async call through the browser process.
  // JS can wait for `matchMedia("screen and (min-resolution: 2dppx)").matches`
  // for the operation to complete, if it can tell which number to use in
  // min-resolution.
  frame_->GetLocalRootWebFrameWidget()->SetZoomLevelForTesting(
      web_view->ZoomLevel() + 1);
}

void TestRunnerBindings::ZoomPageOut() {
  if (invalid_)
    return;

  // This may only be run from the main frame, as the user modifies this at the
  // top level.
  // Early out instead of CHECK() to avoid poking the fuzzer bear.
  if (!frame_->IsMainFrame())
    return;

  blink::WebView* web_view = GetWebFrame()->View();
  // TODO(danakj): This should be an async call through the browser process.
  // JS can wait for `matchMedia("screen and (min-resolution: 2dppx)").matches`
  // for the operation to complete, if it can tell which number to use in
  // min-resolution.
  frame_->GetLocalRootWebFrameWidget()->SetZoomLevelForTesting(
      web_view->ZoomLevel() - 1);
}

void TestRunnerBindings::SetPageZoomFactor(double zoom_factor) {
  if (invalid_)
    return;

  // This may only be run from the main frame, as the user modifies this at the
  // top level.
  // Early out instead of CHECK() to avoid poking the fuzzer bear.
  if (!frame_->IsMainFrame())
    return;

  // TODO(danakj): This should be an async call through the browser process.
  // JS can wait for `matchMedia("screen and (min-resolution: 2dppx)").matches`
  // for the operation to complete, if it can tell which number to use in
  // min-resolution.
  frame_->GetLocalRootWebFrameWidget()->SetZoomLevelForTesting(
      blink::PageZoomFactorToZoomLevel(zoom_factor));
}

std::string TestRunnerBindings::TooltipText() {
  if (invalid_)
    return {};

  blink::WebString tooltip_text = frame_->GetLocalRootRenderWidget()
                                      ->GetWebWidget()
                                      ->GetLastToolTipTextForTesting();
  return tooltip_text.Utf8();
}

int TestRunnerBindings::WebHistoryItemCount() {
  if (invalid_)
    return 0;
  return frame_->render_view()->GetLocalSessionHistoryLengthForTesting();
}

void TestRunnerBindings::ForceNextWebGLContextCreationToFail() {
  if (invalid_)
    return;
  blink::ForceNextWebGLContextCreationToFailForTest();
}

void TestRunnerBindings::FocusDevtoolsSecondaryWindow() {
  if (invalid_)
    return;
  runner_->GetWebTestControlHostRemote()->FocusDevtoolsSecondaryWindow();
}

void TestRunnerBindings::ForceNextDrawingBufferCreationToFail() {
  if (invalid_)
    return;
  blink::ForceNextDrawingBufferCreationToFailForTest();
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
  blink::scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
      FROM_HERE, base::BindOnce(&TestRunner::WorkQueue::ProcessWork,
                                weak_factory_.GetWeakPtr()));
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
  while (!queue_.empty()) {
    finished_loading_ = false;  // Watch for loading finishing inside Run().
    bool started_load = queue_.front()->Run(controller_);
    delete queue_.front();
    queue_.pop_front();

    if (started_load) {
      // If a load started, and didn't complete inside of Run(), then mark
      // the load as running.
      if (!finished_loading_)
        controller_->frame_will_start_load_ = true;

      // Quit doing work once a load is in progress.
      //
      // TODO(danakj): We could avoid the post-task of ProcessWork() by not
      // early-outting here if |finished_loading_|. Since load finished we
      // could keep running work. And in RemoveLoadingFrame() instead of
      // calling ProcessWorkSoon() unconditionally, only call it if we're not
      // already inside ProcessWork().
      return;
    }
  }

  // If there was no navigation stated, there may be no more tasks in the
  // system. We can safely finish the test here as we're not in the middle
  // of a navigation call stack, and ProcessWork() was a posted task.
  controller_->FinishTestIfReady();
}

TestRunner::TestRunner()
    : work_queue_(this),
      test_content_settings_client_(this, &web_test_runtime_flags_) {
  // NOTE: please don't put feature specific enable flags here,
  // instead add them to runtime_enabled_features.json5.
  //
  // Stores state to be restored after each test.
  blink::WebTestingSupport::SaveRuntimeFeatures();

  Reset();
}

TestRunner::~TestRunner() = default;

void TestRunner::Install(WebFrameTestProxy* frame,
                         SpellCheckClient* spell_check) {
  bool is_main_test_window = frame->GetWebViewTestProxy()->is_main_window();
  TestRunnerBindings::Install(this, frame, spell_check,
                              IsWebPlatformTestsMode(), is_main_test_window);
  fake_screen_orientation_impl_.OverrideAssociatedInterfaceProviderForFrame(
      frame->GetWebFrame());
  gamepad_controller_.Install(frame);
  frame->GetWebViewTestProxy()
      ->GetWebView()
      ->SetScreenOrientationOverrideForTesting(
          fake_screen_orientation_impl_.CurrentOrientationType());
}

void TestRunner::Reset() {
  loading_frames_.clear();
  web_test_runtime_flags_.Reset();
  fake_screen_orientation_impl_.ResetData();
  gamepad_controller_.Reset();
  drag_image_.reset();

  blink::WebTestingSupport::ResetRuntimeFeatures();
  blink::WebCache::Clear();
  blink::WebSecurityPolicy::ClearOriginAccessList();
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_FUCHSIA)
  blink::WebFontRenderStyle::SetSubpixelPositioning(false);
#endif
  blink::ResetDomainRelaxationForTest();

  setlocale(LC_ALL, "");
  setlocale(LC_NUMERIC, "C");

  dump_as_audio_ = false;
  dump_back_forward_list_ = false;
  test_repaint_ = false;
  sweep_horizontally_ = false;
  animation_requires_raster_ = false;
  main_frame_loaded_ = false;
  frame_will_start_load_ = false;
  did_notify_done_ = false;

  http_headers_to_clear_.clear();
  clear_referrer_ = false;

  platform_name_ = "chromium";

  weak_factory_.InvalidateWeakPtrs();
  work_queue_.Reset();
}

void TestRunner::ResetWebView(WebViewTestProxy* web_view_test_proxy) {
  blink::WebView* web_view = web_view_test_proxy->GetWebView();

  web_view->SetTabKeyCyclesThroughElements(true);
  web_view->GetSettings()->SetHighlightAds(false);
  web_view->DisableAutoResizeForTesting(gfx::Size());
  web_view->SetScreenOrientationOverrideForTesting(
      fake_screen_orientation_impl_.CurrentOrientationType());
  web_view->UseSynchronousResizeModeForTesting(false);
}

void TestRunner::ResetWebWidget(WebWidgetTestProxy* web_widget_test_proxy) {
  blink::WebFrameWidget* web_widget =
      web_widget_test_proxy->GetWebFrameWidget();

  web_widget->SetDeviceScaleFactorForTesting(0);

  // These things are only modified/valid for the main frame's widget.
  if (web_widget_test_proxy->delegate()) {
    web_widget->ResetZoomLevelForTesting();

    web_widget->SetMainFrameOverlayColor(SK_ColorTRANSPARENT);
    web_widget->SetTextZoomFactor(1);
  }
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
  return web_test_runtime_flags_.generate_pixel_results();
}

TextResultType TestRunner::ShouldGenerateTextResults() {
  if (web_test_runtime_flags_.dump_as_text()) {
    return TextResultType::kText;
  } else if (web_test_runtime_flags_.dump_as_markup()) {
    DCHECK(!web_test_runtime_flags_.is_printing());
    return TextResultType::kMarkup;
  } else if (web_test_runtime_flags_.dump_as_layout()) {
    if (web_test_runtime_flags_.is_printing())
      return TextResultType::kLayoutAsPrinting;
    return TextResultType::kLayout;
  }
  return TextResultType::kEmpty;
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

const std::vector<uint8_t>& TestRunner::GetAudioData() const {
  return audio_data_;
}

bool TestRunner::IsRecursiveLayoutDumpRequested() {
  return web_test_runtime_flags_.dump_child_frames();
}

bool TestRunner::CanDumpPixelsFromRenderer() const {
  return web_test_runtime_flags_.dump_drag_image() ||
         web_test_runtime_flags_.is_printing();
}

SkBitmap TestRunner::DumpPixelsInRenderer(content::RenderView* render_view) {
  auto* view_proxy = static_cast<WebViewTestProxy*>(render_view);
  DCHECK(view_proxy->GetWebView()->MainFrame());
  DCHECK(CanDumpPixelsFromRenderer());

  if (web_test_runtime_flags_.dump_drag_image()) {
    if (!drag_image_.isNull())
      return drag_image_;

    // This means the test called dumpDragImage but did not initiate a drag.
    // Return a blank image so that the test fails.
    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    bitmap.eraseColor(0);
    return bitmap;
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
  return PrintFrameToBitmap(target_frame);
}

void TestRunner::ReplicateWebTestRuntimeFlagsChanges(
    const base::DictionaryValue& changed_values) {
  if (!test_is_running_)
    return;

  web_test_runtime_flags_.tracked_dictionary().ApplyUntrackedChanges(
      changed_values);
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

blink::WebContentSettingsClient* TestRunner::GetWebContentSettings() {
  return &test_content_settings_client_;
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

bool TestRunner::ClearReferrer() const {
  return clear_referrer_;
}

void TestRunner::AddLoadingFrame(blink::WebFrame* frame) {
  // Don't track loading the about:blank between tests
  if (!test_is_running_)
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
  frame_will_start_load_ = false;
}

void TestRunner::RemoveLoadingFrame(blink::WebFrame* frame) {
  // We don't track frames that were started between tests.
  if (!base::Contains(loading_frames_, frame))
    return;

  // We had a DCHECK checking
  // web_test_runtime_flags_.have_loading_frame() here, but that led to
  // flakiness due to inconsistent state management across renderers.
  // See https://crbug.com/1100223 for details.

  base::Erase(loading_frames_, frame);
  if (!loading_frames_.empty())
    return;

  web_test_runtime_flags_.set_have_loading_frame(false);

  // Loads in between tests should not propel us into thinking that we're now
  // inside the test. |main_frame_loaded_| set below is used to signal that the
  // test has definitely started executing.
  if (!test_is_running_)
    return;

  main_frame_loaded_ = true;
  OnWebTestRuntimeFlagsChanged();

  // No more new work after the first complete load.
  work_queue_.set_frozen(true);
  // Inform the work queue that any load it started is done, in case it is
  // still inside ProcessWork().
  work_queue_.set_finished_loading();

  // testRunner.waitUntilDone() will pause the work queue if it is being used by
  // the test, until testRunner.notifyDone() is called. However this can only be
  // done once.
  if (!web_test_runtime_flags_.wait_until_done() || did_notify_done_)
    work_queue_.ProcessWorkSoon();
}

void TestRunner::FinishTestIfReady() {
  if (!test_is_running_)
    return;

  // We don't end the test before the main frame has had a chance to load. This
  // is used to ensure the main frame has had a chance to start loading. If the
  // test calls testRunner.notifyDone() then we also know it has begun loading.
  if (!main_frame_loaded_ && !did_notify_done_)
    return;

  // While loading any frames, we do not end the test.
  // The |frame_will_start_load_| bool is used for when the work queue has
  // started a load, but it is not in |loading_frames_| yet as there is some
  // time between them. We also have to check |loading_frames_| for once the
  // loading is started, and because the test may start a load in other ways
  // besides the work queue.
  if (frame_will_start_load_ || !loading_frames_.empty())
    return;

  // If there are tasks in the queue still, we must wait for them before
  // finishing the test.
  if (!work_queue_.is_empty())
    return;

  // If waiting for testRunner.notifyDone() then we can not end the test.
  if (web_test_runtime_flags_.wait_until_done() && !did_notify_done_)
    return;

  FinishTest();
}

void TestRunner::TestFinishedFromSecondaryRenderer() {
  NotifyDone();
}

void TestRunner::ResetRendererAfterWebTest(base::OnceClosure done_callback) {
  // Instead of resetting for the next test here, delay until after the
  // navigation to about:blank, which is heard about in in
  // `DidCommitNavigationInMainFrame()`. This ensures we reset settings that are
  // set between now and the load of about:blank, and that no new changes or
  // loads can be started by the renderer.
  waiting_for_reset_navigation_to_about_blank_ = std::move(done_callback);

  // TODO(danakj): Move this navigation to the browser.
  blink::WebURLRequest request{GURL(url::kAboutBlankURL)};
  request.SetMode(network::mojom::RequestMode::kNavigate);
  request.SetRedirectMode(network::mojom::RedirectMode::kManual);
  request.SetRequestContext(blink::mojom::RequestContextType::INTERNAL);
  request.SetRequestorOrigin(blink::WebSecurityOrigin::CreateUniqueOpaque());

  WebFrameTestProxy* main_frame = FindInProcessMainWindowMainFrame();
  DCHECK(main_frame);
  main_frame->GetWebFrame()->StartNavigation(request);
}

void TestRunner::DidCommitNavigationInMainFrame(WebFrameTestProxy* main_frame) {
  // This method is just meant to catch the about:blank navigation started in
  // ResetRendererAfterWebTest().
  if (!waiting_for_reset_navigation_to_about_blank_)
    return;

  // This would mean some other navigation was already happening when the test
  // ended, the about:blank should still be coming.
  GURL url = main_frame->GetWebFrame()->GetDocumentLoader()->GetUrl();
  if (!url.IsAboutBlank())
    return;

  // Perform the reset now that the main frame is on about:blank.
  main_frame->Reset();
  Reset();

  // Ack to the browser.
  std::move(waiting_for_reset_navigation_to_about_blank_).Run();
}

void TestRunner::AddMainFrame(WebFrameTestProxy* frame) {
  main_frames_.insert(frame);
}

void TestRunner::RemoveMainFrame(WebFrameTestProxy* frame) {
  main_frames_.erase(frame);
}

void TestRunner::AddRenderView(WebViewTestProxy* view) {
  render_views_.insert(view);
}

void TestRunner::RemoveRenderView(WebViewTestProxy* view) {
  render_views_.erase(view);
}

void TestRunner::PolicyDelegateDone() {
  DCHECK(web_test_runtime_flags_.wait_until_done());
  FinishTest();
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

void TestRunner::SetDragImage(const SkBitmap& drag_image) {
  if (web_test_runtime_flags_.dump_drag_image()) {
    if (drag_image_.isNull())
      drag_image_ = drag_image;
  }
}

bool TestRunner::ShouldDumpNavigationPolicy() const {
  return web_test_runtime_flags_.dump_navigation_policy();
}

class WorkItemBackForward : public TestRunner::WorkItem {
 public:
  explicit WorkItemBackForward(int distance) : distance_(distance) {}

  bool Run(TestRunner* test_runner) override {
    test_runner->GoToOffset(distance_);
    return true;  // FIXME: Did it really start a navigation?
  }

 private:
  int distance_;
};

WebFrameTestProxy* TestRunner::FindInProcessMainWindowMainFrame() {
  for (WebFrameTestProxy* main_frame : main_frames_) {
    WebViewTestProxy* view = main_frame->GetWebViewTestProxy();
    DCHECK_EQ(view->GetMainRenderFrame(), main_frame);
    if (view->is_main_window())
      return main_frame;
  }
  return nullptr;
}

void TestRunner::WaitUntilDone() {
  web_test_runtime_flags_.set_wait_until_done(true);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::NotifyDone() {
  if (!web_test_runtime_flags_.wait_until_done())
    return;
  if (did_notify_done_)
    return;

  // Mark that the test has asked the test to end when the rest of our stopping
  // conditions are met. Then check if we can end the test.
  did_notify_done_ = true;
  FinishTestIfReady();
}

void TestRunner::QueueBackNavigation(int how_far_back) {
  work_queue_.AddWork(new WorkItemBackForward(-how_far_back));
}

void TestRunner::QueueForwardNavigation(int how_far_forward) {
  work_queue_.AddWork(new WorkItemBackForward(how_far_forward));
}

class WorkItemReload : public TestRunner::WorkItem {
 public:
  bool Run(TestRunner* test_runner) override {
    test_runner->Reload();
    return true;
  }
};

void TestRunner::QueueReload() {
  work_queue_.AddWork(new WorkItemReload());
}

class WorkItemLoadingScript : public TestRunner::WorkItem {
 public:
  explicit WorkItemLoadingScript(const std::string& script,
                                 base::WeakPtr<TestRunnerBindings> bindings)
      : script_(script), bindings_(std::move(bindings)) {}

  bool Run(TestRunner*) override {
    if (!bindings_)
      return false;
    bindings_->GetWebFrame()->ExecuteScript(
        blink::WebScriptSource(blink::WebString::FromUTF8(script_)));
    return true;  // FIXME: Did it really start a navigation?
  }

 private:
  std::string script_;
  base::WeakPtr<TestRunnerBindings> bindings_;
};

void TestRunner::QueueLoadingScript(
    const std::string& script,
    base::WeakPtr<TestRunnerBindings> bindings) {
  work_queue_.AddWork(new WorkItemLoadingScript(script, std::move(bindings)));
}

class WorkItemNonLoadingScript : public TestRunner::WorkItem {
 public:
  explicit WorkItemNonLoadingScript(const std::string& script,
                                    base::WeakPtr<TestRunnerBindings> bindings)
      : script_(script), bindings_(std::move(bindings)) {}

  bool Run(TestRunner*) override {
    if (!bindings_)
      return false;
    bindings_->GetWebFrame()->ExecuteScript(
        blink::WebScriptSource(blink::WebString::FromUTF8(script_)));
    return false;
  }

 private:
  std::string script_;
  base::WeakPtr<TestRunnerBindings> bindings_;
};

void TestRunner::QueueNonLoadingScript(
    const std::string& script,
    base::WeakPtr<TestRunnerBindings> bindings) {
  work_queue_.AddWork(
      new WorkItemNonLoadingScript(script, std::move(bindings)));
}

class WorkItemLoad : public TestRunner::WorkItem {
 public:
  WorkItemLoad(const GURL& url, const std::string& target)
      : url_(url), target_(target) {}

  bool Run(TestRunner* test_runner) override {
    test_runner->LoadURLForFrame(url_, target_);
    return true;  // FIXME: Did it really start a navigation?
  }

 private:
  GURL url_;
  std::string target_;
};

void TestRunner::QueueLoad(const GURL& current_url,
                           const std::string& relative_url,
                           const std::string& target) {
  GURL full_url = current_url.Resolve(relative_url);
  work_queue_.AddWork(new WorkItemLoad(full_url, target));
}

void TestRunner::OnTestPreferencesChanged(const TestPreferences& test_prefs,
                                          RenderFrame* frame) {
  RenderView* render_view = frame->GetRenderView();
  blink::web_pref::WebPreferences web_prefs =
      render_view->GetBlinkPreferences();

  // Turns the TestPreferences into WebPreferences.
  ExportWebTestSpecificPreferences(test_prefs, &web_prefs);

  render_view->SetBlinkPreferences(web_prefs);

  GetWebTestControlHostRemote()->OverridePreferences(web_prefs);
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

int TestRunner::InProcessWindowCount() {
  return main_frames_.size();
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
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_FUCHSIA)
  // Since FontConfig doesn't provide a variable to control subpixel
  // positioning, we'll fall back to setting it globally for all fonts.
  blink::WebFontRenderStyle::SetSubpixelPositioning(value);
#endif
}

void TestRunner::UseUnfortunateSynchronousResizeMode() {
  // Sets the resize mode on the view of each open window.
  for (WebViewTestProxy* view : render_views_) {
    view->GetWebView()->UseSynchronousResizeModeForTesting(true);
  }
}

void TestRunner::SetMockScreenOrientation(WebViewTestProxy* view_proxy,
                                          const std::string& orientation_str) {
  blink::mojom::ScreenOrientation orientation;

  if (orientation_str == "portrait-primary") {
    orientation = blink::mojom::ScreenOrientation::kPortraitPrimary;
  } else if (orientation_str == "portrait-secondary") {
    orientation = blink::mojom::ScreenOrientation::kPortraitSecondary;
  } else if (orientation_str == "landscape-primary") {
    orientation = blink::mojom::ScreenOrientation::kLandscapePrimary;
  } else {
    DCHECK_EQ("landscape-secondary", orientation_str);
    orientation = blink::mojom::ScreenOrientation::kLandscapeSecondary;
  }

  bool changed = fake_screen_orientation_impl_.UpdateDeviceOrientation(
      view_proxy, orientation);
  if (changed)
    GetWebTestControlHostRemote()->SetScreenOrientationChanged();
}

void TestRunner::DisableMockScreenOrientation(WebViewTestProxy* view_proxy) {
  fake_screen_orientation_impl_.SetDisabled(view_proxy, true);
}

std::string TestRunner::GetAcceptLanguages() const {
  return web_test_runtime_flags_.accept_languages();
}

void TestRunner::SetAcceptLanguages(const std::string& accept_languages) {
  if (accept_languages == GetAcceptLanguages())
    return;

  // TODO(danakj): IPC to WebTestControlHost, and have it change the
  // WebContentsImpl::GetMutableRendererPrefs(). Then have the browser sync that
  // to the window's RenderViews, instead of using WebTestRuntimeFlags for this.
  // Then also get rid of |render_views_|.
  web_test_runtime_flags_.set_accept_languages(accept_languages);
  OnWebTestRuntimeFlagsChanged();

  for (WebViewTestProxy* view : render_views_)
    view->GetWebView()->AcceptLanguagesChanged();
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
  uint8_t* bytes = static_cast<uint8_t*>(view.bytes());
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

void TestRunner::SetAllowRunningOfInsecureContent(bool allowed) {
  web_test_runtime_flags_.set_running_insecure_content_allowed(allowed);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::DumpPermissionClientCallbacks() {
  web_test_runtime_flags_.set_dump_web_content_settings_client_callbacks(true);
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

void TestRunner::SetShouldStayOnPageAfterHandlingBeforeUnload(bool value) {
  web_test_runtime_flags_.set_stay_on_page_after_handling_before_unload(value);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::SetWillSendRequestClearHeader(const std::string& header) {
  if (!header.empty())
    http_headers_to_clear_.insert(header);
}

void TestRunner::SetWillSendRequestClearReferrer() {
  clear_referrer_ = true;
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

void TestRunner::SetIsWebPlatformTestsMode() {
  web_test_runtime_flags_.set_is_web_platform_tests_mode(true);
  OnWebTestRuntimeFlagsChanged();
}

bool TestRunner::IsWebPlatformTestsMode() const {
  return web_test_runtime_flags_.is_web_platform_tests_mode();
}

void TestRunner::SetDumpJavaScriptDialogs(bool value) {
  web_test_runtime_flags_.set_dump_javascript_dialogs(value);
  OnWebTestRuntimeFlagsChanged();
}

void TestRunner::SetEffectiveConnectionType(
    blink::WebEffectiveConnectionType connection_type) {
  effective_connection_type_ = connection_type;
}

bool TestRunner::ShouldDumpConsoleMessages() const {
  // Once TestFinished() is entered, we don't want additional log lines to
  // be printed while we collect the renderer-side test results, so we check
  // |test_is_running_| here as well.
  return test_is_running_ && web_test_runtime_flags_.dump_console_messages();
}

void TestRunner::GoToOffset(int offset) {
  GetWebTestControlHostRemote()->GoToOffset(offset);
}

void TestRunner::Reload() {
  GetWebTestControlHostRemote()->Reload();
}

void TestRunner::LoadURLForFrame(const GURL& url,
                                 const std::string& frame_name) {
  GetWebTestControlHostRemote()->LoadURLForFrame(url, frame_name);
}

void TestRunner::PrintMessage(const std::string& message) {
  GetWebTestControlHostRemote()->PrintMessage(message);
}

void TestRunner::PrintMessageToStderr(const std::string& message) {
  GetWebTestControlHostRemote()->PrintMessageToStderr(message);
}

blink::WebString TestRunner::RegisterIsolatedFileSystem(
    const std::vector<base::FilePath>& file_paths) {
  std::string filesystem_id;
  GetWebTestControlHostRemote()->RegisterIsolatedFileSystem(file_paths,
                                                            &filesystem_id);
  return blink::WebString::FromUTF8(filesystem_id);
}

void TestRunner::FocusWindow(RenderFrame* main_frame, bool focus) {
  // Early out instead of CHECK() to avoid poking the fuzzer bear.
  if (!main_frame->IsMainFrame())
    return;

  auto* frame_proxy = static_cast<WebFrameTestProxy*>(main_frame);
  RenderWidget* widget = frame_proxy->GetLocalRootRenderWidget();

  // Web tests get multiple windows in one renderer by doing same-site
  // window.open() calls (or about:blank). They want to be able to move focus
  // between those windows synchronously in the renderer, which is what we
  // do here. We only allow it to focus main frames however, for simplicitly.

  if (!focus) {
    // This path simulates losing focus on the window, without moving it to
    // another window.
    if (widget->GetWebWidget()->HasFocus()) {
      widget->SetActive(false);
      widget->GetWebWidget()->SetFocus(false);
    }
    return;
  }

  // Find the currently focused window, and remove its focus.
  for (WebFrameTestProxy* other_main_frame : main_frames_) {
    if (other_main_frame != main_frame) {
      RenderWidget* other_widget = other_main_frame->GetLocalRootRenderWidget();
      if (other_widget->GetWebWidget()->HasFocus()) {
        other_widget->SetActive(false);
        other_widget->GetWebWidget()->SetFocus(false);
      }
    }
  }

  if (!widget->GetWebWidget()->HasFocus()) {
    widget->GetWebWidget()->SetFocus(true);
    widget->SetActive(true);
  }
}

void TestRunner::SetAnimationRequiresRaster(bool do_raster) {
  animation_requires_raster_ = do_raster;
}

void TestRunner::OnWebTestRuntimeFlagsChanged() {
  // Ignore changes that happen before we got the initial, accumulated
  // web flag changes in SetTestConfiguration().
  if (!test_is_running_)
    return;
  if (web_test_runtime_flags_.tracked_dictionary().changed_values().empty())
    return;

  GetWebTestControlHostRemote()->WebTestRuntimeFlagsChanged(
      web_test_runtime_flags_.tracked_dictionary().changed_values().Clone());

  web_test_runtime_flags_.tracked_dictionary().ResetChangeTracking();
}

void TestRunner::FinishTest() {
  WebFrameTestProxy* main_frame = FindInProcessMainWindowMainFrame();

  // When there are no more frames loading, and the test hasn't asked to wait
  // for NotifyDone(), then we normally conclude the test. However if this
  // TestRunner is attached to a swapped out frame tree - that is, the main
  // frame is in another frame tree - then finishing here would be premature
  // for the main frame where the test is running. If |did_notify_done_| is
  // true then we *were* waiting for NotifyDone() and it has already happened,
  // so we want to proceed as if the NotifyDone() is happening now.
  //
  // Ideally, the main frame would wait for loading frames in its frame tree
  // as well as any secondary renderers, but it does not know about secondary
  // renderers. So in this case the test should finish when frames finish
  // loading in the primary renderer, and we don't finish the test from a
  // secondary renderer unless it is asked for explicitly via NotifyDone.
  //
  // This will bounce through the browser to the renderer process hosting the
  // main window's main frame. There it will come back to this method, but hit
  // the other path.
  if (!main_frame) {
    if (did_notify_done_)
      GetWebTestControlHostRemote()->TestFinishedInSecondaryRenderer();
    return;
  }

  // Avoid a situation where TestFinished is called twice, because
  // of a racey test where multiple renderers call notifyDone(), or a test that
  // calls notifyDone() more than once.
  if (!test_is_running_)
    return;
  test_is_running_ = false;

  // Now we know that we're in the main frame, we should generate dump results.
  // Clean out the lifecycle if needed before capturing the web tree
  // dump and pixels from the compositor.
  auto* web_frame = main_frame->GetWebFrame();
  web_frame->FrameWidget()->UpdateAllLifecyclePhases(
      blink::DocumentUpdateReason::kTest);

  const mojom::WebTestRunTestConfiguration& test_config =
      main_frame->GetWebViewTestProxy()->test_config();

  // Initialize a new dump results object which we will populate in the calls
  // below.
  auto dump_result = mojom::WebTestRendererDumpResult::New();

  bool browser_should_dump_back_forward_list = ShouldDumpBackForwardList();
  bool browser_should_dump_pixels = false;

  if (ShouldDumpAsAudio()) {
    TRACE_EVENT0("shell", "TestRunner::CaptureLocalAudioDump");
    dump_result->audio = GetAudioData();
  } else {
    TextResultType text_result_type = ShouldGenerateTextResults();
    bool pixel_result = ShouldGeneratePixelResults();

    std::string spec = GURL(test_config.test_url).spec();
    size_t path_start = spec.rfind("web_tests/");
    if (path_start != std::string::npos)
      spec = spec.substr(path_start);

    std::string mime_type =
        web_frame->GetDocumentLoader()->GetResponse().MimeType().Utf8();

    // In a text/plain document, and in a dumpAsText/ subdirectory, we generate
    // text results no matter what the test may previously have requested.
    if (mime_type == "text/plain" ||
        spec.find("/dumpAsText/") != std::string::npos) {
      text_result_type = TextResultType::kText;
      pixel_result = false;
    }

    // If possible we grab the layout dump locally because a round trip through
    // the browser would give javascript a chance to run and change the layout.
    // We only go to the browser if we can not do it locally, because we want to
    // dump more than just the local main frame. Those tests must be written to
    // not modify layout after signalling the test is finished.
    //
    // The CustomTextDump always takes precedence if it's been specified by the
    // test.
    std::string custom_text_dump;
    if (HasCustomTextDump(&custom_text_dump)) {
      dump_result->layout = custom_text_dump + "\n";
    } else if (!IsRecursiveLayoutDumpRequested()) {
      TRACE_EVENT0("shell", "TestRunner::CaptureLocalLayoutDump");
      dump_result->layout = DumpLayoutAsString(web_frame, text_result_type);
    }

    if (pixel_result) {
      if (CanDumpPixelsFromRenderer()) {
        TRACE_EVENT0("shell", "TestRunner::CaptureLocalPixelsDump");
        SkBitmap actual =
            DumpPixelsInRenderer(main_frame->GetWebViewTestProxy());
        DCHECK_GT(actual.info().width(), 0);
        DCHECK_GT(actual.info().height(), 0);

        base::MD5Digest digest;
        base::MD5Sum(actual.getPixels(), actual.computeByteSize(), &digest);
        dump_result->actual_pixel_hash = base::MD5DigestToBase16(digest);

        if (dump_result->actual_pixel_hash != test_config.expected_pixel_hash)
          dump_result->pixels = std::move(actual);
      } else {
        browser_should_dump_pixels = true;
        if (ShouldDumpSelectionRect()) {
          TRACE_EVENT0("shell", "TestRunner::CaptureLocalSelectionRect");
          dump_result->selection_rect =
              web_frame->GetSelectionBoundsRectForTesting();
        }
      }
    }
  }

  // Informs the browser that the test is done, passing along any test results
  // that have been generated locally. The browser may collect further results
  // from this and other renderer processes before moving on to the next test.
  GetWebTestControlHostRemote()->InitiateCaptureDump(
      std::move(dump_result), browser_should_dump_back_forward_list,
      browser_should_dump_pixels);
}

mojo::AssociatedRemote<mojom::WebTestControlHost>&
TestRunner::GetWebTestControlHostRemote() {
  if (!web_test_control_host_remote_) {
    RenderThread::Get()->GetChannel()->GetRemoteAssociatedInterface(
        &web_test_control_host_remote_);
    web_test_control_host_remote_.set_disconnect_handler(
        base::BindOnce(&TestRunner::HandleWebTestControlHostDisconnected,
                       base::Unretained(this)));
  }
  return web_test_control_host_remote_;
}

void TestRunner::HandleWebTestControlHostDisconnected() {
  web_test_control_host_remote_.reset();
}

mojom::WebTestBluetoothFakeAdapterSetter&
TestRunner::GetBluetoothFakeAdapterSetter() {
  if (!bluetooth_fake_adapter_setter_) {
    RenderThread::Get()->BindHostReceiver(
        bluetooth_fake_adapter_setter_.BindNewPipeAndPassReceiver());
    bluetooth_fake_adapter_setter_.set_disconnect_handler(base::BindOnce(
        &TestRunner::HandleBluetoothFakeAdapterSetterDisconnected,
        base::Unretained(this)));
  }
  return *bluetooth_fake_adapter_setter_;
}

void TestRunner::HandleBluetoothFakeAdapterSetterDisconnected() {
  bluetooth_fake_adapter_setter_.reset();
}

}  // namespace content
