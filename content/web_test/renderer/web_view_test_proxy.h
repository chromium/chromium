// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_WEB_VIEW_TEST_PROXY_H_
#define CONTENT_WEB_TEST_RENDERER_WEB_VIEW_TEST_PROXY_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/renderer/agent_scheduling_group.h"
#include "content/renderer/render_view_impl.h"
#include "content/web_test/common/web_test.mojom.h"
#include "content/web_test/renderer/accessibility_controller.h"
#include "content/web_test/renderer/text_input_controller.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_dom_message_event.h"
#include "third_party/blink/public/web/web_history_commit_type.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/public/web/web_widget_client.h"

namespace blink {
class WebLocalFrame;
class WebString;
class WebView;
struct WebWindowFeatures;
}  // namespace blink

namespace content {
class AccessibilityController;
class TestRunner;
class TextInputController;

// WebViewTestProxy is used to run web tests. This class is a partial fake
// implementation of RenderViewImpl that overrides the minimal necessary
// portions of RenderViewImpl to allow for use in web tests.
//
// This method of injecting test functionality is an outgrowth of legacy.
// In particular, classic dependency injection does not work easily
// because the RenderWidget class is too large with too much entangled
// state, making it hard to factor out creation points for injection.
//
// While implementing a fake via partial overriding of a class leads to
// a fragile base class problem and implicit coupling of the test code
// and production code, it is the most viable mechanism available without
// a huge refactor.
//
// Historically, the overridden functionality has been small enough to not
// cause too much trouble. If that changes, then this entire testing
// architecture should be revisited.
class WebViewTestProxy : public RenderViewImpl {
 public:
  explicit WebViewTestProxy(AgentSchedulingGroup& agent_scheduling_group,
                            CompositorDependencies* compositor_deps,
                            const mojom::CreateViewParams& params,
                            TestRunner* test_runner);

  // WebViewClient implementation.
  blink::WebView* CreateView(blink::WebLocalFrame* creator,
                             const blink::WebURLRequest& request,
                             const blink::WebWindowFeatures& features,
                             const blink::WebString& frame_name,
                             blink::WebNavigationPolicy policy,
                             network::mojom::WebSandboxFlags sandbox_flags,
                             const blink::FeaturePolicyFeatureState&,
                             const blink::SessionStorageNamespaceId&
                                 session_storage_namespace_id) override;
  void PrintPage(blink::WebLocalFrame* frame) override;
  blink::WebString AcceptLanguages() override;

  TestRunner* GetTestRunner() { return test_runner_; }
  AccessibilityController* accessibility_controller() {
    return &accessibility_controller_;
  }

  void Reset();
  void Install(blink::WebLocalFrame* frame);

  // Convert the provided relative path into an absolute path.
  blink::WebString GetAbsoluteWebStringFromUTF8Path(const std::string& path);

  // Called on each RenderView that is part of the main test window, to give
  // the test configuration, indicate to the RenderView that it is part of the
  // main window, as well as to start the test.
  void SetTestConfiguration(mojom::WebTestRunTestConfigurationPtr params,
                            bool starting_test);

  // True if the RenderView is hosting a frame tree fragment that is part of the
  // web test harness' main window.
  bool is_main_window() const { return is_main_window_; }
  const mojom::WebTestRunTestConfiguration& test_config() const {
    return test_config_;
  }

 private:
  // RenderViewImpl has no public destructor.
  ~WebViewTestProxy() override;

  TestRunner* const test_runner_;

  // True if the RenderView is hosting a frame tree fragment that is part of the
  // web test harness' main window.
  bool is_main_window_ = false;
  mojom::WebTestRunTestConfiguration test_config_;

  AccessibilityController accessibility_controller_{this};
  TextInputController text_input_controller_{this};

  DISALLOW_COPY_AND_ASSIGN(WebViewTestProxy);
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_WEB_VIEW_TEST_PROXY_H_
