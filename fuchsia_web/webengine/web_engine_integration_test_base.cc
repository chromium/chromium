// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/web_engine_integration_test_base.h"

#include <lib/fdio/directory.h>
#include <zircon/status.h>

#include <string_view>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/path_service.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "net/test/embedded_test_server/default_handlers.h"

namespace {

fuchsia::web::ContentDirectoryProvider CreateTestDataDirectoryProvider() {
  fuchsia::web::ContentDirectoryProvider provider;
  provider.set_name("testdata");
  base::FilePath pkg_path;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &pkg_path));
  provider.set_directory(base::OpenDirectoryHandle(
      pkg_path.AppendASCII("fuchsia_web/webengine/test/data")));
  return provider;
}

}  // namespace

WebEngineIntegrationTestBase::WebEngineIntegrationTestBase()
    : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
      filtered_service_directory_(std::make_shared<sys::ServiceDirectory>(
          base::OpenDirectoryHandle(base::FilePath("/svc")))) {
  // Push all services from /svc to the filtered service directory.
  // Calling stat() in /svc is problematic; see https://fxbug.dev/100207. Tell
  // the enumerator not to recurse, to return both files and directories, and
  // to report only the names of entries.
  base::FileEnumerator file_enum(base::FilePath("/svc"), /*recursive=*/false,
                                 base::FileEnumerator::NAMES_ONLY);
  for (auto file = file_enum.Next(); !file.empty(); file = file_enum.Next()) {
    zx_status_t status =
        filtered_service_directory_.AddService(file.BaseName().value());
    ZX_CHECK(status == ZX_OK, status) << "FilteredServiceDirectory::AddService";
  }
}

WebEngineIntegrationTestBase::~WebEngineIntegrationTestBase() = default;

void WebEngineIntegrationTestBase::SetUp() {
  embedded_test_server_.ServeFilesFromSourceDirectory(
      "fuchsia_web/webengine/test/data");
  net::test_server::RegisterDefaultHandlers(&embedded_test_server_);
  CHECK(embedded_test_server_.Start());
}

fuchsia::web::NavigationControllerPtr
WebEngineIntegrationTestBase::CreateNavigationController() {
  CHECK(frame_);
  fuchsia::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());
  return controller;
}

fuchsia::web::CreateContextParams
WebEngineIntegrationTestBase::TestContextParams() {
  fuchsia::web::CreateContextParams create_params;

  // Most integration tests require networking, to load test web content.
  create_params.set_features(fuchsia::web::ContextFeatureFlags::NETWORK);

  zx_status_t status = filtered_service_directory_.ConnectClient(
      create_params.mutable_service_directory()->NewRequest());
  ZX_CHECK(status == ZX_OK, status)
      << "FilteredServiceDirectory::ConnectClient";
  return create_params;
}

fuchsia::web::CreateContextParams
WebEngineIntegrationTestBase::TestContextParamsWithTestData() {
  fuchsia::web::CreateContextParams create_params = TestContextParams();
  create_params.mutable_content_directories()->push_back(
      CreateTestDataDirectoryProvider());
  return create_params;
}

void WebEngineIntegrationTestBase::CreateContext(
    fuchsia::web::CreateContextParams context_params) {
  CHECK(!context_);
  GetContextProvider()->Create(std::move(context_params),
                               context_.NewRequest());
  context_.set_error_handler(
      [](zx_status_t status) { FAIL() << zx_status_get_string(status); });
}

void WebEngineIntegrationTestBase::CreateContextAndFrame(
    fuchsia::web::CreateContextParams context_params) {
  CHECK(!frame_);

  CreateContext(std::move(context_params));

  context_->CreateFrame(frame_.NewRequest());
  frame_.set_error_handler(
      [](zx_status_t status) { FAIL() << zx_status_get_string(status); });

  CreateNavigationListener();
}

void WebEngineIntegrationTestBase::CreateFrameWithParams(
    fuchsia::web::CreateFrameParams frame_params) {
  CHECK(!frame_);
  CHECK(context_);

  context_->CreateFrameWithParams(std::move(frame_params), frame_.NewRequest());
  frame_.set_error_handler(
      [](zx_status_t status) { FAIL() << zx_status_get_string(status); });

  CreateNavigationListener();
}

void WebEngineIntegrationTestBase::CreateContextAndExpectError(
    fuchsia::web::CreateContextParams params,
    zx_status_t expected_error) {
  CHECK(!context_);
  GetContextProvider()->Create(std::move(params), context_.NewRequest());
  base::RunLoop run_loop;
  context_.set_error_handler([&run_loop, expected_error](zx_status_t status) {
    EXPECT_EQ(status, expected_error);
    run_loop.Quit();
  });
  run_loop.Run();
}

void WebEngineIntegrationTestBase::CreateContextAndFrameAndLoadUrl(
    fuchsia::web::CreateContextParams params,
    const GURL& url) {
  CreateContextAndFrame(std::move(params));

  // Navigate the Frame to |url| and wait for it to complete loading.
  auto navigation_controller = CreateNavigationController();
  ASSERT_TRUE(::LoadUrlAndExpectResponse(
      navigation_controller.get(), fuchsia::web::LoadUrlParams(), url.spec()));

  // Wait for the URL to finish loading.
  navigation_listener_->RunUntilUrlEquals(url);
}

void WebEngineIntegrationTestBase::LoadUrlAndExpectResponse(
    std::string_view url,
    fuchsia::web::LoadUrlParams load_url_params) {
  // Connect a new NavigationController to ensure that LoadUrl() is processed
  // after all other messages previously sent to the frame.
  fuchsia::web::NavigationControllerPtr navigation_controller;
  frame_->GetNavigationController(navigation_controller.NewRequest());
  navigation_controller.set_error_handler(
      [](zx_status_t status) { FAIL() << zx_status_get_string(status); });
  ASSERT_TRUE(::LoadUrlAndExpectResponse(navigation_controller.get(),
                                         std::move(load_url_params), url));
}

void WebEngineIntegrationTestBase::GrantPermission(
    fuchsia::web::PermissionType type,
    const std::string& origin) {
  fuchsia::web::PermissionDescriptor permission;
  permission.set_type(type);
  frame_->SetPermissionState(std::move(permission), origin,
                             fuchsia::web::PermissionState::GRANTED);
}

std::string WebEngineIntegrationTestBase::ExecuteJavaScriptWithStringResult(
    std::string_view script) {
  std::optional<base::Value> value = ExecuteJavaScript(frame_.get(), script);
  return value ? value->GetString() : std::string();
}

double WebEngineIntegrationTestBase::ExecuteJavaScriptWithDoubleResult(
    std::string_view script) {
  std::optional<base::Value> value = ExecuteJavaScript(frame_.get(), script);
  return value ? value->GetDouble() : 0.0;
}

bool WebEngineIntegrationTestBase::ExecuteJavaScriptWithBoolResult(
    std::string_view script) {
  std::optional<base::Value> value = ExecuteJavaScript(frame_.get(), script);
  return value ? value->GetBool() : false;
}

void WebEngineIntegrationTestBase::CreateNavigationListener() {
  CHECK(frame_);
  CHECK(!navigation_listener_);
  navigation_listener_ = std::make_unique<TestNavigationListener>();
  navigation_listener_binding_ =
      std::make_unique<fidl::Binding<fuchsia::web::NavigationEventListener>>(
          navigation_listener_.get());
  frame_->SetNavigationEventListener2(
      navigation_listener_binding_->NewBinding(), /*flags=*/{});
}
