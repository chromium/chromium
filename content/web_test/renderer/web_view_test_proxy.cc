// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/web_view_test_proxy.h"

#include <stddef.h>
#include <stdint.h>

#include "content/web_test/common/web_test_string_util.h"
#include "content/web_test/renderer/test_runner.h"
#include "content/web_test/renderer/web_frame_test_proxy.h"
#include "net/base/filename_util.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

WebViewTestProxy::WebViewTestProxy(AgentSchedulingGroup& agent_scheduling_group,
                                   CompositorDependencies* compositor_deps,
                                   const mojom::CreateViewParams& params,
                                   TestRunner* test_runner)
    : RenderViewImpl(agent_scheduling_group, compositor_deps, params),
      test_runner_(test_runner) {
  test_runner_->AddRenderView(this);
}

WebViewTestProxy::~WebViewTestProxy() {
  test_runner_->RemoveRenderView(this);
}

blink::WebView* WebViewTestProxy::CreateView(
    blink::WebLocalFrame* creator,
    const blink::WebURLRequest& request,
    const blink::WebWindowFeatures& features,
    const blink::WebString& frame_name,
    blink::WebNavigationPolicy policy,
    network::mojom::WebSandboxFlags sandbox_flags,
    const blink::FeaturePolicyFeatureState& opener_feature_state,
    const blink::SessionStorageNamespaceId& session_storage_namespace_id) {
  if (test_runner_->ShouldDumpNavigationPolicy()) {
    test_runner_->PrintMessage(
        "Default policy for createView for '" +
        web_test_string_util::URLDescription(request.Url()) + "' is '" +
        web_test_string_util::WebNavigationPolicyToString(policy) + "'\n");
  }

  if (!test_runner_->CanOpenWindows())
    return nullptr;

  if (test_runner_->ShouldDumpCreateView()) {
    test_runner_->PrintMessage(
        std::string("createView(") +
        web_test_string_util::URLDescription(request.Url()) + ")\n");
  }
  return RenderViewImpl::CreateView(creator, request, features, frame_name,
                                    policy, sandbox_flags, opener_feature_state,
                                    session_storage_namespace_id);
}

void WebViewTestProxy::PrintPage(blink::WebLocalFrame* frame) {
  // This is using the main frame for the size, but maybe it should be using the
  // frame's size.
  blink::WebSize page_size_in_pixels =
      GetMainRenderFrame()->GetLocalRootRenderWidget()->GetWebWidget()->Size();
  if (page_size_in_pixels.IsEmpty())
    return;
  blink::WebPrintParams print_params(page_size_in_pixels);
  frame->PrintBegin(print_params, blink::WebNode());
  frame->PrintEnd();
}

blink::WebString WebViewTestProxy::AcceptLanguages() {
  return blink::WebString::FromUTF8(test_runner_->GetAcceptLanguages());
}

void WebViewTestProxy::Reset() {
  accessibility_controller_.Reset();
  // |text_input_controller_| doesn't have any state to reset.

  // Resets things on the WebView that TestRunnerBindings can modify.
  test_runner_->ResetWebView(this);
}

void WebViewTestProxy::Install(blink::WebLocalFrame* frame) {
  accessibility_controller_.Install(frame);
  text_input_controller_.Install(frame);
}

blink::WebString WebViewTestProxy::GetAbsoluteWebStringFromUTF8Path(
    const std::string& utf8_path) {
  base::FilePath path = base::FilePath::FromUTF8Unsafe(utf8_path);
  if (!path.IsAbsolute()) {
    GURL base_url =
        net::FilePathToFileURL(test_config_.current_working_directory.Append(
            FILE_PATH_LITERAL("foo")));
    net::FileURLToFilePath(base_url.Resolve(utf8_path), &path);
  }
  return blink::FilePathToWebString(path);
}

void WebViewTestProxy::SetTestConfiguration(
    mojom::WebTestRunTestConfigurationPtr params,
    bool starting_test) {
  test_config_ = std::move(*params);
  // Sets this view as being part of the main test window. The main test window
  // doesn't change, so once set this remains always true.
  is_main_window_ = true;

  test_runner_->SetTestIsRunning(true);

  std::string spec = GURL(test_config_.test_url).spec();
  size_t path_start = spec.rfind("web_tests/");
  if (path_start != std::string::npos)
    spec = spec.substr(path_start);

  bool is_devtools_test =
      spec.find("/devtools/") != std::string::npos ||
      spec.find("/inspector-protocol/") != std::string::npos;
  if (is_devtools_test)
    test_runner_->SetDumpConsoleMessages(false);

  // In protocol mode (see TestInfo::protocol_mode), we dump layout only when
  // requested by the test. In non-protocol mode, we dump layout by default
  // because the layout may be the only interesting thing to the user while
  // we don't dump non-human-readable binary data. In non-protocol mode, we
  // still generate pixel results (though don't dump them) to let the renderer
  // execute the same code regardless of the protocol mode, e.g. for ease of
  // debugging a web test issue.
  if (!test_config_.protocol_mode)
    test_runner_->SetShouldDumpAsLayout(true);

  // For http/tests/loading/, which is served via httpd and becomes /loading/.
  if (spec.find("/loading/") != std::string::npos)
    test_runner_->SetShouldDumpFrameLoadCallbacks(true);

  if (spec.find("/external/wpt/") != std::string::npos ||
      spec.find("/external/csswg-test/") != std::string::npos ||
      spec.find("://web-platform.test") != std::string::npos ||
      spec.find("/harness-tests/wpt/") != std::string::npos)
    test_runner_->SetIsWebPlatformTestsMode();

  GetWebView()->GetSettings()->SetV8CacheOptions(
      is_devtools_test ? blink::mojom::V8CacheOptions::kNone
                       : blink::mojom::V8CacheOptions::kDefault);

  if (starting_test) {
    DCHECK(GetMainRenderFrame());
    // If focus was in a child frame, it gets lost when we navigate to the next
    // test, but we want to start with focus in the main frame for every test.
    // Focus is controlled by the renderer, so we must do the reset here.
    GetWebView()->SetFocusedFrame(GetMainRenderFrame()->GetWebFrame());
  }
}

}  // namespace content
