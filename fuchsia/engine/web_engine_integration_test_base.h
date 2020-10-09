// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_WEB_ENGINE_INTEGRATION_TEST_BASE_H_
#define FUCHSIA_ENGINE_WEB_ENGINE_INTEGRATION_TEST_BASE_H_

#include <fuchsia/mediacodec/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/sys/cpp/component_context.h>

#include <string>

#include "base/command_line.h"
#include "base/fuchsia/filtered_service_directory.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/task_environment.h"
#include "fuchsia/base/test_navigation_listener.h"
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

  static fuchsia::web::ContentDirectoryProvider
  CreateTestDataDirectoryProvider();

  void StartWebEngine(base::CommandLine command_line);

  fuchsia::web::CreateContextParams DefaultContextParams() const;

  fuchsia::web::CreateContextParams DefaultContextParamsWithTestData() const;
  fuchsia::web::CreateContextParams ContextParamsWithFilteredServiceDirectory();

  // Populates |navigation_listener_| with a TestNavigationListener and adds it
  // to |frame|, enabling tests to monitor the state of the Frame.
  // May only be called once.
  void CreateNavigationListener(fuchsia::web::FramePtr* frame);

  // Populates |navigation_controller_| with a NavigationController for |frame|.
  // May only be called once.
  void AddNavigationControllerAndListenerToFrame(fuchsia::web::FramePtr* frame);

  // Populates |context_| with a Context with |params|.
  void CreateContext(fuchsia::web::CreateContextParams context_params);

  // Returns a new Frame created from |context_|.
  fuchsia::web::FramePtr CreateFrame();

  // Returns a new Frame with |frame_params| created from |context_|.
  fuchsia::web::FramePtr CreateFrameWithParams(
      fuchsia::web::CreateFrameParams frame_params);

  // Populates |context_| with a Context with |context_params|, |frame_| with a
  // new Frame, |navigation_controller_| with a NavigationController request for
  // |frame_|, and navigation_listener_| with a TestNavigationListener that is
  // added to |frame|.
  void CreateContextAndFrame(fuchsia::web::CreateContextParams context_params);

  // Same as CreateContextAndFrame() but uses |frame_params| to create the
  // Frame.
  void CreateContextAndFrameWithParams(
      fuchsia::web::CreateContextParams context_params,
      fuchsia::web::CreateFrameParams frame_params);

  void CreateContextAndExpectError(fuchsia::web::CreateContextParams params,
                                   zx_status_t expected_error);

  void CreateContextAndFrameAndLoadUrl(fuchsia::web::CreateContextParams params,
                                       const GURL& url);

  void LoadUrlWithUserActivation(base::StringPiece url);

  void GrantPermission(fuchsia::web::PermissionType type,
                       const std::string& origin);

  std::string ExecuteJavaScriptWithStringResult(base::StringPiece script);

  double ExecuteJavaScriptWithDoubleResult(base::StringPiece script);

  bool ExecuteJavaScriptWithBoolResult(base::StringPiece script);

 protected:
  const base::test::TaskEnvironment task_environment_;

  fidl::InterfaceHandle<fuchsia::sys::ComponentController>
      web_engine_controller_;
  fuchsia::web::ContextProviderPtr web_context_provider_;

  net::EmbeddedTestServer embedded_test_server_;

  fuchsia::web::ContextPtr context_;
  fuchsia::web::FramePtr frame_;
  fuchsia::web::NavigationControllerPtr navigation_controller_;

  std::unique_ptr<cr_fuchsia::TestNavigationListener> navigation_listener_;
  std::unique_ptr<fidl::Binding<fuchsia::web::NavigationEventListener>>
      navigation_listener_binding_;

  std::unique_ptr<base::fuchsia::FilteredServiceDirectory>
      filtered_service_directory_;
};

#endif  // FUCHSIA_ENGINE_WEB_ENGINE_INTEGRATION_TEST_BASE_H_
