// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/web_test/blink_test_runner.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/hash/md5.h"
#include "content/public/common/url_constants.h"
#include "content/shell/renderer/web_test/test_runner.h"
#include "content/shell/renderer/web_test/web_frame_test_proxy.h"
#include "content/shell/renderer/web_test/web_view_test_proxy.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

BlinkTestRunner::BlinkTestRunner(WebViewTestProxy* web_view_test_proxy)
    : web_view_test_proxy_(web_view_test_proxy) {}

BlinkTestRunner::~BlinkTestRunner() = default;

void BlinkTestRunner::DidCommitNavigationInMainFrame(
    WebFrameTestProxy* main_frame) {
  // This method is just meant to catch the about:blank navigation started in
  // ResetRendererAfterWebTest().
  if (!waiting_for_reset_navigation_to_about_blank_)
    return;

  // This would mean some other navigation was already happening when the test
  // ended, the about:blank should still be coming.
  GURL url = main_frame->GetWebFrame()->GetDocumentLoader()->GetUrl();
  if (!url.IsAboutBlank())
    return;

  waiting_for_reset_navigation_to_about_blank_ = false;

  TestRunner* test_runner = web_view_test_proxy_->GetTestRunner();

  main_frame->Reset();
  test_runner->Reset();
  // Ack to the browser (this could converted to be a mojo reply).
  GetWebTestControlHostRemote()->ResetRendererAfterWebTestDone();
}

mojo::AssociatedRemote<mojom::WebTestControlHost>&
BlinkTestRunner::GetWebTestControlHostRemote() {
  TestRunner* test_runner = web_view_test_proxy_->GetTestRunner();
  return test_runner->GetWebTestControlHostRemote();
}

mojo::AssociatedRemote<mojom::WebTestClient>&
BlinkTestRunner::GetWebTestClientRemote() {
  TestRunner* test_runner = web_view_test_proxy_->GetTestRunner();
  return test_runner->GetWebTestClientRemote();
}

void BlinkTestRunner::ApplyTestConfiguration(
    mojom::WebTestRunTestConfigurationPtr params) {
  TestRunner* test_runner = web_view_test_proxy_->GetTestRunner();

  test_config_ = std::move(*params);

  test_runner->SetTestIsRunning(true);

  std::string spec = GURL(test_config_.test_url).spec();
  size_t path_start = spec.rfind("web_tests/");
  if (path_start != std::string::npos)
    spec = spec.substr(path_start);

  bool is_devtools_test =
      spec.find("/devtools/") != std::string::npos ||
      spec.find("/inspector-protocol/") != std::string::npos;
  if (is_devtools_test)
    test_runner->SetDumpConsoleMessages(false);

  // In protocol mode (see TestInfo::protocol_mode), we dump layout only when
  // requested by the test. In non-protocol mode, we dump layout by default
  // because the layout may be the only interesting thing to the user while
  // we don't dump non-human-readable binary data. In non-protocol mode, we
  // still generate pixel results (though don't dump them) to let the renderer
  // execute the same code regardless of the protocol mode, e.g. for ease of
  // debugging a web test issue.
  if (!test_config_.protocol_mode)
    test_runner->SetShouldDumpAsLayout(true);

  // For http/tests/loading/, which is served via httpd and becomes /loading/.
  if (spec.find("/loading/") != std::string::npos)
    test_runner->SetShouldDumpFrameLoadCallbacks(true);

  if (spec.find("/external/wpt/") != std::string::npos ||
      spec.find("/external/csswg-test/") != std::string::npos ||
      spec.find("://web-platform.test") != std::string::npos ||
      spec.find("/harness-tests/wpt/") != std::string::npos)
    test_runner->SetIsWebPlatformTestsMode();

  web_view_test_proxy_->GetWebView()->GetSettings()->SetV8CacheOptions(
      is_devtools_test ? blink::WebSettings::V8CacheOptions::kNone
                       : blink::WebSettings::V8CacheOptions::kDefault);
}

void BlinkTestRunner::OnReplicateTestConfiguration(
    mojom::WebTestRunTestConfigurationPtr params) {
  ApplyTestConfiguration(std::move(params));
}

void BlinkTestRunner::OnSetTestConfiguration(
    mojom::WebTestRunTestConfigurationPtr params) {
  DCHECK(web_view_test_proxy_->GetMainRenderFrame());

  ApplyTestConfiguration(std::move(params));

  // If focus was in a child frame, it gets lost when we navigate to the next
  // test, but we want to start with focus in the main frame for every test.
  // Focus is controlled by the renderer, so we must do the reset here.
  web_view_test_proxy_->GetWebView()->SetFocusedFrame(
      web_view_test_proxy_->GetMainRenderFrame()->GetWebFrame());
}

void BlinkTestRunner::OnResetRendererAfterWebTest() {
  DCHECK(web_view_test_proxy_->GetMainRenderFrame());

  // Instead of resetting for the next test here, delay until after the
  // navigation to about:blank (this is in |DidCommitNavigationInMainFrame()|).
  // This ensures we reset settings that are set between now and the load of
  // about:blank.

  // Navigating to about:blank will make sure that no new loads are initiated
  // by the renderer.
  waiting_for_reset_navigation_to_about_blank_ = true;

  blink::WebURLRequest request{GURL(url::kAboutBlankURL)};
  request.SetMode(network::mojom::RequestMode::kNavigate);
  request.SetRedirectMode(network::mojom::RedirectMode::kManual);
  request.SetRequestContext(blink::mojom::RequestContextType::INTERNAL);
  request.SetRequestorOrigin(blink::WebSecurityOrigin::CreateUniqueOpaque());
  web_view_test_proxy_->GetMainRenderFrame()->GetWebFrame()->StartNavigation(
      request);
}

}  // namespace content
