// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/web_test/web_view_test_proxy.h"

#include <stddef.h>
#include <stdint.h>

#include "content/shell/common/web_test/web_test_string_util.h"
#include "content/shell/renderer/web_test/blink_test_runner.h"
#include "content/shell/renderer/web_test/mock_screen_orientation_client.h"
#include "content/shell/renderer/web_test/test_runner.h"
#include "content/shell/renderer/web_test/web_frame_test_proxy.h"
#include "net/base/filename_util.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

WebViewTestProxy::WebViewTestProxy(CompositorDependencies* compositor_deps,
                                   const mojom::CreateViewParams& params,
                                   TestRunner* test_runner)
    : RenderViewImpl(compositor_deps, params), test_runner_(test_runner) {
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
  frame->PrintBegin(print_params);
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
    GURL base_url = net::FilePathToFileURL(
        blink_test_runner_.test_config().current_working_directory.Append(
            FILE_PATH_LITERAL("foo")));
    net::FileURLToFilePath(base_url.Resolve(utf8_path), &path);
  }
  return blink::FilePathToWebString(path);
}

}  // namespace content
