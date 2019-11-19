// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/test_runner_for_specific_view.h"

#include <stddef.h>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/renderer/compositor/layer_tree_view.h"
#include "content/shell/test_runner/layout_dump.h"
#include "content/shell/test_runner/mock_content_settings_client.h"
#include "content/shell/test_runner/mock_screen_orientation_client.h"
#include "content/shell/test_runner/pixel_dump.h"
#include "content/shell/test_runner/spell_check_client.h"
#include "content/shell/test_runner/test_common.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/test_preferences.h"
#include "content/shell/test_runner/test_runner.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "content/shell/test_runner/web_widget_test_proxy.h"
#include "gin/arguments.h"
#include "gin/array_buffer.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_isolated_world_ids.h"
#include "third_party/blink/public/platform/web_isolated_world_info.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_array_buffer.h"
#include "third_party/blink/public/web/web_array_buffer_converter.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_render_theme.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/switches.h"

namespace test_runner {

TestRunnerForSpecificView::TestRunnerForSpecificView(
    WebViewTestProxy* web_view_test_proxy)
    : web_view_test_proxy_(web_view_test_proxy) {
  Reset();
}

TestRunnerForSpecificView::~TestRunnerForSpecificView() {}

void TestRunnerForSpecificView::Install(blink::WebLocalFrame* frame) {
  web_view_test_proxy_->test_interfaces()->GetTestRunner()->Install(
      frame, weak_factory_.GetWeakPtr());
}

void TestRunnerForSpecificView::Reset() {
  pointer_locked_ = false;
  pointer_lock_planned_result_ = PointerLockWillSucceed;

  if (!web_view() || !web_view()->MainFrame())
    return;

  RemoveWebPageOverlay();
  SetTabKeyCyclesThroughElements(true);

#if !defined(OS_MACOSX) && !defined(OS_WIN)
  // (Constants copied because we can't depend on the header that defined
  // them from this file.)
  blink::SetSelectionColors(0xff1e90ff, 0xff000000, 0xffc8c8c8, 0xff323232);
#endif
  if (web_view()->MainFrame()->IsWebLocalFrame()) {
    web_view()->MainFrame()->ToWebLocalFrame()->EnableViewSourceMode(false);
    web_view()->SetTextZoomFactor(1);
    // As would the browser via IPC, set visibility on the RenderWidget then on
    // the Page.
    // TODO(danakj): This should set visibility on all RenderWidgets not just
    // the main frame.
    // TODO(danakj): This should set visible on the RenderWidget not just the
    // LayerTreeView.
    main_frame_render_widget()->layer_tree_view()->SetVisible(true);
  }
  web_view_test_proxy_->ApplyPageVisibilityState(
      content::PageVisibilityState::kVisible,
      /*initial_setting=*/true);
}

bool TestRunnerForSpecificView::RequestPointerLock() {
  switch (pointer_lock_planned_result_) {
    case PointerLockWillSucceed:
      PostTask(base::BindOnce(
          &TestRunnerForSpecificView::DidAcquirePointerLockInternal,
          weak_factory_.GetWeakPtr()));
      return true;
    case PointerLockWillRespondAsync:
      DCHECK(!pointer_locked_);
      return true;
    case PointerLockWillFailSync:
      DCHECK(!pointer_locked_);
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

void TestRunnerForSpecificView::RequestPointerUnlock() {
  PostTask(
      base::BindOnce(&TestRunnerForSpecificView::DidLosePointerLockInternal,
                     weak_factory_.GetWeakPtr()));
}

bool TestRunnerForSpecificView::isPointerLocked() {
  return pointer_locked_;
}

void TestRunnerForSpecificView::PostTask(base::OnceClosure callback) {
  delegate()->PostTask(std::move(callback));
}

void TestRunnerForSpecificView::PostV8Callback(
    const v8::Local<v8::Function>& callback) {
  PostTask(base::BindOnce(&TestRunnerForSpecificView::InvokeV8Callback,
                          weak_factory_.GetWeakPtr(),
                          v8::UniquePersistent<v8::Function>(
                              blink::MainThreadIsolate(), callback)));
}

void TestRunnerForSpecificView::PostV8CallbackWithArgs(
    v8::UniquePersistent<v8::Function> callback,
    int argc,
    v8::Local<v8::Value> argv[]) {
  std::vector<v8::UniquePersistent<v8::Value>> args;
  for (int i = 0; i < argc; i++) {
    args.push_back(
        v8::UniquePersistent<v8::Value>(blink::MainThreadIsolate(), argv[i]));
  }

  PostTask(base::BindOnce(&TestRunnerForSpecificView::InvokeV8CallbackWithArgs,
                          weak_factory_.GetWeakPtr(), std::move(callback),
                          std::move(args)));
}

void TestRunnerForSpecificView::InvokeV8Callback(
    const v8::UniquePersistent<v8::Function>& callback) {
  std::vector<v8::UniquePersistent<v8::Value>> empty_args;
  InvokeV8CallbackWithArgs(callback, std::move(empty_args));
}

void TestRunnerForSpecificView::InvokeV8CallbackWithArgs(
    const v8::UniquePersistent<v8::Function>& callback,
    const std::vector<v8::UniquePersistent<v8::Value>>& args) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);

  blink::WebLocalFrame* frame = GetLocalMainFrame();
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;
  v8::Context::Scope context_scope(context);

  std::vector<v8::Local<v8::Value>> local_args;
  for (const auto& arg : args) {
    local_args.push_back(v8::Local<v8::Value>::New(isolate, arg));
  }

  frame->CallFunctionEvenIfScriptDisabled(
      v8::Local<v8::Function>::New(isolate, callback), context->Global(),
      local_args.size(), local_args.data());
}

base::OnceClosure TestRunnerForSpecificView::CreateClosureThatPostsV8Callback(
    const v8::Local<v8::Function>& callback) {
  return base::BindOnce(
      &TestRunnerForSpecificView::PostTask, weak_factory_.GetWeakPtr(),
      base::BindOnce(&TestRunnerForSpecificView::InvokeV8Callback,
                     weak_factory_.GetWeakPtr(),
                     v8::UniquePersistent<v8::Function>(
                         blink::MainThreadIsolate(), callback)));
}

void TestRunnerForSpecificView::UpdateAllLifecyclePhasesAndComposite() {
  // Note, this is executed synchronously. Wrap in setTimeout() to run
  // asynchronously.
  main_frame_render_widget()->SynchronouslyComposite(/*raster=*/true);
}

void TestRunnerForSpecificView::UpdateAllLifecyclePhasesAndCompositeThen(
    v8::Local<v8::Function> callback) {
  // Note, this is executed synchronously. Wrap in setTimeout() to run
  // asynchronously.
  UpdateAllLifecyclePhasesAndComposite();
  InvokeV8Callback(
      v8::UniquePersistent<v8::Function>(blink::MainThreadIsolate(), callback));
}

void TestRunnerForSpecificView::CapturePixelsAsyncThen(
    v8::Local<v8::Function> callback) {
  v8::UniquePersistent<v8::Function> persistent_callback(
      blink::MainThreadIsolate(), callback);

  CHECK(web_view()->MainFrame()->IsWebLocalFrame())
      << "Web tests harness doesn't currently support running "
      << "testRuner.capturePixelsAsyncThen from an OOPIF";

  test_runner::TestInterfaces* interfaces =
      web_view_test_proxy_->test_interfaces();

  if (interfaces->GetTestRunner()->CanDumpPixelsFromRenderer()) {
    // If we're grabbing pixels from printing, we do that in the renderer, and
    // some tests actually look at the results.
    interfaces->GetTestRunner()->DumpPixelsAsync(
        web_view_test_proxy_,
        base::BindOnce(&TestRunnerForSpecificView::RunJSCallbackWithBitmap,
                       weak_factory_.GetWeakPtr(),
                       std::move(persistent_callback)));
  } else {
    // If we're running the compositor lifecycle then the pixels aren't
    // available from the renderer, and they don't matter to tests.
    // TODO(crbug.com/952399): We could stop pretending they matter and split
    // this into a separate testRunner API that won't act like its returning
    // pixels.
    main_frame_render_widget()->RequestPresentation(base::BindOnce(
        &TestRunnerForSpecificView::RunJSCallbackAfterCompositorLifecycle,
        weak_factory_.GetWeakPtr(), std::move(persistent_callback)));
  }
}

void TestRunnerForSpecificView::RunJSCallbackAfterCompositorLifecycle(
    v8::UniquePersistent<v8::Function> callback,
    const gfx::PresentationFeedback&) {
  // TODO(crbug.com/952399): We're not testing pixels on this path, remove the
  // SkBitmap plumbing entirely and rename CapturePixels* to RunLifecycle*.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(0);
  RunJSCallbackWithBitmap(std::move(callback), bitmap);
}

void TestRunnerForSpecificView::RunJSCallbackWithBitmap(
    v8::UniquePersistent<v8::Function> callback,
    const SkBitmap& snapshot) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context =
      GetLocalMainFrame()->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Value> argv[3];

  // Size can be 0 for cases where copyImageAt was called on position
  // that doesn't have an image.
  int width = snapshot.info().width();
  argv[0] = v8::Number::New(isolate, width);

  int height = snapshot.info().height();
  argv[1] = v8::Number::New(isolate, height);

  // Skia's internal byte order is platform-dependent. Always convert to RGBA
  // in order to provide a consistent ordering to the web tests.
  const SkImageInfo bufferInfo =
      snapshot.info().makeColorType(kRGBA_8888_SkColorType);
  const size_t bufferRowBytes = bufferInfo.minRowBytes();
  blink::WebArrayBuffer buffer = blink::WebArrayBuffer::Create(
      bufferInfo.computeByteSize(bufferRowBytes), 1);
  if (!snapshot.readPixels(bufferInfo, buffer.Data(), bufferRowBytes, 0, 0)) {
    // We only expect readPixels to fail for null bitmaps.
    DCHECK(snapshot.isNull());
  }

  argv[2] = blink::WebArrayBufferConverter::ToV8Value(
      &buffer, context->Global(), isolate);

  PostV8CallbackWithArgs(std::move(callback), base::size(argv), argv);
}

void TestRunnerForSpecificView::CopyImageAtAndCapturePixelsAsyncThen(
    int x,
    int y,
    v8::Local<v8::Function> callback) {
  v8::UniquePersistent<v8::Function> persistent_callback(
      blink::MainThreadIsolate(), callback);
  CopyImageAtAndCapturePixels(
      web_view()->MainFrame()->ToWebLocalFrame(), x, y,
      base::BindOnce(&TestRunnerForSpecificView::RunJSCallbackWithBitmap,
                     weak_factory_.GetWeakPtr(),
                     std::move(persistent_callback)));
}

void TestRunnerForSpecificView::GetManifestThen(
    v8::Local<v8::Function> callback) {
  if (!web_view()->MainFrame()->IsWebLocalFrame()) {
    CHECK(false) << "This function cannot be called if the main frame is not a "
                    "local frame.";
  }

  v8::UniquePersistent<v8::Function> persistent_callback(
      blink::MainThreadIsolate(), callback);

  delegate()->FetchManifest(
      web_view(),
      base::BindOnce(&TestRunnerForSpecificView::GetManifestCallback,
                     weak_factory_.GetWeakPtr(),
                     std::move(persistent_callback)));
}

void TestRunnerForSpecificView::GetManifestCallback(
    v8::UniquePersistent<v8::Function> callback,
    const blink::WebURL& manifest_url,
    const blink::Manifest& manifest) {
  PostV8CallbackWithArgs(std::move(callback), 0, nullptr);
}

void TestRunnerForSpecificView::GetBluetoothManualChooserEvents(
    v8::Local<v8::Function> callback) {
  return delegate()->GetBluetoothManualChooserEvents(base::BindOnce(
      &TestRunnerForSpecificView::GetBluetoothManualChooserEventsCallback,
      weak_factory_.GetWeakPtr(),
      v8::UniquePersistent<v8::Function>(blink::MainThreadIsolate(),
                                         callback)));
}

void TestRunnerForSpecificView::GetBluetoothManualChooserEventsCallback(
    v8::UniquePersistent<v8::Function> callback,
    const std::vector<std::string>& events) {
  // Build the V8 context.
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      GetLocalMainFrame()->MainWorldScriptContext();
  if (context.IsEmpty())
    return;
  v8::Context::Scope context_scope(context);

  // Convert the argument.
  v8::Local<v8::Value> arg;
  if (!gin::TryConvertToV8(isolate, events, &arg))
    return;

  // Call the callback.
  PostV8CallbackWithArgs(std::move(callback), 1, &arg);
}

void TestRunnerForSpecificView::SetBluetoothFakeAdapter(
    const std::string& adapter_name,
    v8::Local<v8::Function> callback) {
  delegate()->SetBluetoothFakeAdapter(
      adapter_name, CreateClosureThatPostsV8Callback(callback));
}

void TestRunnerForSpecificView::SetBluetoothManualChooser(bool enable) {
  delegate()->SetBluetoothManualChooser(enable);
}

void TestRunnerForSpecificView::SendBluetoothManualChooserEvent(
    const std::string& event,
    const std::string& argument) {
  delegate()->SendBluetoothManualChooserEvent(event, argument);
}

void TestRunnerForSpecificView::SetBackingScaleFactor(
    double value,
    v8::Local<v8::Function> callback) {
  delegate()->SetDeviceScaleFactor(value);

  // TODO(oshima): remove this callback argument when all platforms are migrated
  // to use-zoom-for-dsf by default
  v8::UniquePersistent<v8::Function> global_callback(blink::MainThreadIsolate(),
                                                     callback);
  v8::Local<v8::Value> arg = v8::Boolean::New(
      blink::MainThreadIsolate(), delegate()->IsUseZoomForDSFEnabled());
  PostV8CallbackWithArgs(std::move(global_callback), 1, &arg);
}

void TestRunnerForSpecificView::EnableUseZoomForDSF(
    v8::Local<v8::Function> callback) {
  delegate()->EnableUseZoomForDSF();
  PostV8Callback(callback);
}

void TestRunnerForSpecificView::SetColorProfile(
    const std::string& name,
    v8::Local<v8::Function> callback) {
  delegate()->SetDeviceColorSpace(name);
  PostV8Callback(callback);
}

void TestRunnerForSpecificView::DispatchBeforeInstallPromptEvent(
    const std::vector<std::string>& event_platforms,
    v8::Local<v8::Function> callback) {
  delegate()->DispatchBeforeInstallPromptEvent(
      event_platforms,
      base::BindOnce(
          &TestRunnerForSpecificView::DispatchBeforeInstallPromptCallback,
          weak_factory_.GetWeakPtr(),
          v8::UniquePersistent<v8::Function>(blink::MainThreadIsolate(),
                                             callback)));
}

void TestRunnerForSpecificView::DispatchBeforeInstallPromptCallback(
    v8::UniquePersistent<v8::Function> callback,
    bool canceled) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context =
      GetLocalMainFrame()->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Value> arg;
  arg = v8::Boolean::New(isolate, canceled);

  PostV8CallbackWithArgs(std::move(callback), 1, &arg);
}

void TestRunnerForSpecificView::RunIdleTasks(v8::Local<v8::Function> callback) {
  delegate()->RunIdleTasks(CreateClosureThatPostsV8Callback(callback));
}

void TestRunnerForSpecificView::SetTabKeyCyclesThroughElements(
    bool tab_key_cycles_through_elements) {
  web_view()->SetTabKeyCyclesThroughElements(tab_key_cycles_through_elements);
}

void TestRunnerForSpecificView::ExecCommand(gin::Arguments* args) {
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
  web_view()->FocusedFrame()->ExecuteCommand(
      blink::WebString::FromUTF8(command), blink::WebString::FromUTF8(value));
}

bool TestRunnerForSpecificView::IsCommandEnabled(const std::string& command) {
  return web_view()->FocusedFrame()->IsCommandEnabled(
      blink::WebString::FromUTF8(command));
}

bool TestRunnerForSpecificView::HasCustomPageSizeStyle(int page_index) {
  // TODO(dcheng): This class has many implicit assumptions that the frames it
  // operates on are always local.
  blink::WebFrame* frame = web_view()->MainFrame();
  if (!frame || frame->IsWebRemoteFrame())
    return false;
  return frame->ToWebLocalFrame()->HasCustomPageSizeStyle(page_index);
}

void TestRunnerForSpecificView::ForceRedSelectionColors() {
  blink::SetSelectionColors(0xffee0000, 0xff00ee00, 0xff000000, 0xffc0c0c0);
}

void TestRunnerForSpecificView::SetPageVisibility(
    const std::string& new_visibility) {
  content::PageVisibilityState visibility;
  if (new_visibility == "visible") {
    visibility = content::PageVisibilityState::kVisible;
  } else if (new_visibility == "hidden") {
    visibility = content::PageVisibilityState::kHidden;
  } else {
    return;
  }

  // As would the browser via IPC, set visibility on the RenderWidget then on
  // the Page.
  // TODO(danakj): This should set visibility on all RenderWidgets not just the
  // main frame.
  // TODO(danakj): This should set visible on the RenderWidget not just the
  // LayerTreeView.
  main_frame_render_widget()->layer_tree_view()->SetVisible(
      visibility == content::PageVisibilityState::kVisible);
  web_view_test_proxy_->ApplyPageVisibilityState(visibility,
                                                 /*initial_setting=*/false);
}

void TestRunnerForSpecificView::SetTextDirection(
    const std::string& direction_name) {
  // Map a direction name to a WebTextDirection value.
  blink::WebTextDirection direction;
  if (direction_name == "auto")
    direction = blink::kWebTextDirectionDefault;
  else if (direction_name == "rtl")
    direction = blink::kWebTextDirectionRightToLeft;
  else if (direction_name == "ltr")
    direction = blink::kWebTextDirectionLeftToRight;
  else
    return;

  web_view()->FocusedFrame()->SetTextDirection(direction);
}

void TestRunnerForSpecificView::AddWebPageOverlay() {
  web_view()->SetMainFrameOverlayColor(SK_ColorCYAN);
}

void TestRunnerForSpecificView::RemoveWebPageOverlay() {
  web_view()->SetMainFrameOverlayColor(SK_ColorTRANSPARENT);
}

void TestRunnerForSpecificView::SetHighlightAds(bool enabled) {
  web_view()->GetSettings()->SetHighlightAds(enabled);
}

void TestRunnerForSpecificView::ForceNextWebGLContextCreationToFail() {
  web_view()->ForceNextWebGLContextCreationToFail();
}

void TestRunnerForSpecificView::ForceNextDrawingBufferCreationToFail() {
  web_view()->ForceNextDrawingBufferCreationToFail();
}

void TestRunnerForSpecificView::SetWindowIsKey(bool value) {
  web_view_test_proxy_->test_interfaces()->GetTestRunner()->SetFocus(web_view(),
                                                                     value);
}

void TestRunnerForSpecificView::DidAcquirePointerLock() {
  DidAcquirePointerLockInternal();
}

void TestRunnerForSpecificView::DidNotAcquirePointerLock() {
  DidNotAcquirePointerLockInternal();
}

void TestRunnerForSpecificView::DidLosePointerLock() {
  DidLosePointerLockInternal();
}

void TestRunnerForSpecificView::SetPointerLockWillFailSynchronously() {
  pointer_lock_planned_result_ = PointerLockWillFailSync;
}

void TestRunnerForSpecificView::SetPointerLockWillRespondAsynchronously() {
  pointer_lock_planned_result_ = PointerLockWillRespondAsync;
}

void TestRunnerForSpecificView::DidAcquirePointerLockInternal() {
  pointer_locked_ = true;
  web_view()->MainFrameWidget()->DidAcquirePointerLock();

  // Reset planned result to default.
  pointer_lock_planned_result_ = PointerLockWillSucceed;
}

void TestRunnerForSpecificView::DidNotAcquirePointerLockInternal() {
  DCHECK(!pointer_locked_);
  pointer_locked_ = false;
  web_view()->MainFrameWidget()->DidNotAcquirePointerLock();

  // Reset planned result to default.
  pointer_lock_planned_result_ = PointerLockWillSucceed;
}

void TestRunnerForSpecificView::DidLosePointerLockInternal() {
  bool was_locked = pointer_locked_;
  pointer_locked_ = false;
  if (was_locked)
    web_view()->MainFrameWidget()->DidLosePointerLock();
}

void TestRunnerForSpecificView::SetDomainRelaxationForbiddenForURLScheme(
    bool forbidden,
    const std::string& scheme) {
  web_view()->SetDomainRelaxationForbidden(forbidden,
                                           blink::WebString::FromUTF8(scheme));
}

v8::Local<v8::Value>
TestRunnerForSpecificView::EvaluateScriptInIsolatedWorldAndReturnValue(
    int32_t world_id,
    const std::string& script) {
  blink::WebScriptSource source(blink::WebString::FromUTF8(script));
  // This relies on the iframe focusing itself when it loads. This is a bit
  // sketchy, but it seems to be what other tests do.
  v8::Local<v8::Value> value =
      web_view()->FocusedFrame()->ExecuteScriptInIsolatedWorldAndReturnValue(
          world_id, source);
  if (!value.IsEmpty())
    return value;
  return v8::Local<v8::Value>();
}

void TestRunnerForSpecificView::EvaluateScriptInIsolatedWorld(
    int32_t world_id,
    const std::string& script) {
  blink::WebScriptSource source(blink::WebString::FromUTF8(script));
  web_view()->FocusedFrame()->ExecuteScriptInIsolatedWorld(world_id, source);
}

void TestRunnerForSpecificView::SetIsolatedWorldInfo(
    int32_t world_id,
    v8::Local<v8::Value> security_origin,
    v8::Local<v8::Value> content_security_policy) {
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
    info.security_origin =
        blink::WebSecurityOrigin::CreateFromString(V8StringToWebString(
            blink::MainThreadIsolate(), security_origin.As<v8::String>()));
  }

  if (content_security_policy->IsString()) {
    info.content_security_policy = V8StringToWebString(
        blink::MainThreadIsolate(), content_security_policy.As<v8::String>());
  }

  // Clear the document->isolated world CSP mapping.
  web_view()->FocusedFrame()->ClearIsolatedWorldCSPForTesting(world_id);

  web_view()->FocusedFrame()->SetIsolatedWorldInfo(world_id, info);
}

void TestRunner::InsertStyleSheet(const std::string& source_code) {
  blink::WebLocalFrame::FrameForCurrentContext()
      ->GetDocument()
      .InsertStyleSheet(blink::WebString::FromUTF8(source_code));
}

bool TestRunnerForSpecificView::FindString(
    const std::string& search_text,
    const std::vector<std::string>& options_array) {
  bool match_case = true;
  bool forward = true;
  bool find_next = true;
  bool wrap_around = false;
  for (const std::string& option : options_array) {
    if (option == "CaseInsensitive")
      match_case = false;
    else if (option == "Backwards")
      forward = false;
    else if (option == "StartInSelection")
      find_next = false;
    else if (option == "WrapAround")
      wrap_around = true;
  }

  blink::WebLocalFrame* frame = GetLocalMainFrame();
  const bool find_result = frame->FindForTesting(
      0, blink::WebString::FromUTF8(search_text), match_case, forward,
      find_next, false /* force */, wrap_around);
  return find_result;
}

std::string TestRunnerForSpecificView::SelectionAsMarkup() {
  return GetLocalMainFrame()->SelectionAsMarkup().Utf8();
}

void TestRunnerForSpecificView::SetViewSourceForFrame(const std::string& name,
                                                      bool enabled) {
  blink::WebFrame* target_frame =
      GetLocalMainFrame()->FindFrameByName(blink::WebString::FromUTF8(name));
  if (target_frame) {
    CHECK(target_frame->IsWebLocalFrame())
        << "This function requires that the target frame is a local frame.";
    target_frame->ToWebLocalFrame()->EnableViewSourceMode(enabled);
  }
}

blink::WebLocalFrame* TestRunnerForSpecificView::GetLocalMainFrame() {
  if (!web_view()->MainFrame()->IsWebLocalFrame()) {
    // Hitting the check below uncovers a new scenario that requires OOPIF
    // support in the web tests harness.
    CHECK(false) << "This function cannot be called if the main frame is not a "
                    "local frame.";
  }
  return web_view()->MainFrame()->ToWebLocalFrame();
}

WebWidgetTestProxy* TestRunnerForSpecificView::main_frame_render_widget() {
  return static_cast<WebWidgetTestProxy*>(web_view_test_proxy_->GetWidget());
}

blink::WebView* TestRunnerForSpecificView::web_view() {
  return web_view_test_proxy_->webview();
}

WebTestDelegate* TestRunnerForSpecificView::delegate() {
  return web_view_test_proxy_->delegate();
}

}  // namespace test_runner
