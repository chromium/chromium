// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_WEB_TEST_BLINK_TEST_RUNNER_H_
#define CONTENT_SHELL_RENDERER_WEB_TEST_BLINK_TEST_RUNNER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "content/public/common/page_state.h"
#include "content/shell/common/web_test/web_test.mojom.h"
#include "content/shell/renderer/web_test/layout_dump.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "url/origin.h"
#include "v8/include/v8.h"

namespace content {
class WebFrameTestProxy;
class WebViewTestProxy;

// An instance of this class is attached to each RenderView in each renderer
// process during a web test. It handles IPCs (forwarded from
// WebTestRenderFrameObserver) from the browser to manage the web test state
// machine.
class BlinkTestRunner {
 public:
  explicit BlinkTestRunner(WebViewTestProxy* web_view_test_proxy);
  ~BlinkTestRunner();

  // Message handlers forwarded by WebTestRenderFrameObserver.
  void OnSetTestConfiguration(mojom::WebTestRunTestConfigurationPtr params);
  void OnReplicateTestConfiguration(
      mojom::WebTestRunTestConfigurationPtr params);
  void DidCommitNavigationInMainFrame(WebFrameTestProxy* main_frame);
  void OnResetRendererAfterWebTest();

  const mojom::WebTestRunTestConfiguration& test_config() const {
    return test_config_;
  }

 private:
  // Helper reused by OnSetTestConfiguration and OnReplicateTestConfiguration.
  void ApplyTestConfiguration(mojom::WebTestRunTestConfigurationPtr params);

  mojo::AssociatedRemote<mojom::WebTestControlHost>&
  GetWebTestControlHostRemote();
  mojo::AssociatedRemote<mojom::WebTestClient>& GetWebTestClientRemote();

  WebViewTestProxy* const web_view_test_proxy_;

  mojom::WebTestRunTestConfiguration test_config_;

  bool waiting_for_reset_navigation_to_about_blank_ = false;

  DISALLOW_COPY_AND_ASSIGN(BlinkTestRunner);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_WEB_TEST_BLINK_TEST_RUNNER_H_
