// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_WEB_ENGINE_INTEGRATION_TEST_BASE_H_
#define FUCHSIA_WEB_WEBENGINE_WEB_ENGINE_INTEGRATION_TEST_BASE_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/sys/cpp/component_context.h>

#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/fuchsia/filtered_service_directory.h"
#include "base/test/task_environment.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

class WebEngineIntegrationTestBase : public testing::Test {
 public:
  WebEngineIntegrationTestBase();
  ~WebEngineIntegrationTestBase() override;
  WebEngineIntegrationTestBase(const WebEngineIntegrationTestBase&) = delete;
  WebEngineIntegrationTestBase& operator=(const WebEngineIntegrationTestBase&) =
      delete;

  void SetUp() override;

  virtual void StartWebEngine(base::CommandLine command_line) = 0;
  virtual fuchsia::web::ContextProvider* GetContextProvider() = 0;

  // Returns the FilteredServiceDirectory used by DefaultContextParams(), which
  // is initially configured to provide all of the calling process' services.
  // Tests may use this to override or remove services for testing.
  base::FilteredServiceDirectory& filtered_service_directory() {
    return filtered_service_directory_;
  }

  // Returns a TestNavigationListener bound to the current |frame_|.
  TestNavigationListener* navigation_listener() const {
    CHECK(navigation_listener_);
    return navigation_listener_.get();
  }

  // Returns the fuchsia.web.Context.
  fuchsia::web::Context* context() const {
    CHECK(context_);
    return context_.get();
  }

  // Returns a new NavigationController connected to |frame_|. Calls made via
  // the NavigationController are not processed in-order with calls made to the
  // |frame_|. Tests should therefore (re-)connect a NavigationController after
  // any Frame configuration calls have been issued, to ensure that they are
  // processed before any navigation requests.
  fuchsia::web::NavigationControllerPtr CreateNavigationController();

  // Returns CreateContextParams with |service_directory| connected to the
  // |filtered_service_directory()| (see above).
  fuchsia::web::CreateContextParams TestContextParams();

  // Returns a default CreateContextParams with a "testdata" content-directory
  // configured.
  fuchsia::web::CreateContextParams TestContextParamsWithTestData();

  // Populates |context_| with a Context with |params|, and attaches an error-
  // handler that invokes ADD_FAILURE().
  // May be called at most once.
  void CreateContext(fuchsia::web::CreateContextParams context_params);

  // Populates |frame_| with a Frame with |frame_params|, attaches an error-
  // handler that invokes ADD_FAILURE(), and connects |navigation_listener()|.
  // May be called at most once.
  void CreateFrameWithParams(fuchsia::web::CreateFrameParams frame_params);

  // Populates |context_| with a Context with |context_params| and |frame_| with
  // a new Frame.
  // TODO(crbug.com/40761737): Audit callers and replace them with calls to
  // CreateContext()+CreateFrameWithParams(), or context()->CreateFrame(),
  // depending on what each test is intended to verify.
  void CreateContextAndFrame(fuchsia::web::CreateContextParams context_params);

  void CreateContextAndExpectError(fuchsia::web::CreateContextParams params,
                                   zx_status_t expected_error);

  // TODO(crbug.com/40761737): Replace this with a LoadUrl() call that can be
  // preceded by CreateContext()+CreateFrameWithParams().
  void CreateContextAndFrameAndLoadUrl(fuchsia::web::CreateContextParams params,
                                       const GURL& url);

  void LoadUrlAndExpectResponse(
      std::string_view url,
      fuchsia::web::LoadUrlParams load_url_params = {});

  void GrantPermission(fuchsia::web::PermissionType type,
                       const std::string& origin);

  std::string ExecuteJavaScriptWithStringResult(std::string_view script);

  double ExecuteJavaScriptWithDoubleResult(std::string_view script);

  bool ExecuteJavaScriptWithBoolResult(std::string_view script);

 protected:
  const base::test::TaskEnvironment task_environment_;

  net::EmbeddedTestServer embedded_test_server_;

  fuchsia::web::ContextPtr context_;
  fuchsia::web::FramePtr frame_;

 private:
  void CreateNavigationListener();

  std::unique_ptr<TestNavigationListener> navigation_listener_;
  std::unique_ptr<fidl::Binding<fuchsia::web::NavigationEventListener>>
      navigation_listener_binding_;

  base::FilteredServiceDirectory filtered_service_directory_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_WEB_ENGINE_INTEGRATION_TEST_BASE_H_
