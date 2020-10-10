// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/web_engine_integration_test_base.h"

#include <lib/fdio/directory.h>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/process_context.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "fuchsia/base/context_provider_test_connector.h"
#include "fuchsia/base/frame_test_util.h"
#include "net/test/embedded_test_server/default_handlers.h"

WebEngineIntegrationTestBase::WebEngineIntegrationTestBase()
    : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

WebEngineIntegrationTestBase::~WebEngineIntegrationTestBase() = default;

void WebEngineIntegrationTestBase::SetUp() {
  embedded_test_server_.ServeFilesFromSourceDirectory(
      "fuchsia/engine/test/data");
  net::test_server::RegisterDefaultHandlers(&embedded_test_server_);
  ASSERT_TRUE(embedded_test_server_.Start());
}

// static
fuchsia::web::ContentDirectoryProvider
WebEngineIntegrationTestBase::CreateTestDataDirectoryProvider() {
  fuchsia::web::ContentDirectoryProvider provider;
  provider.set_name("testdata");
  base::FilePath pkg_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &pkg_path));
  provider.set_directory(base::OpenDirectoryHandle(
      pkg_path.AppendASCII("fuchsia/engine/test/data")));
  return provider;
}

void WebEngineIntegrationTestBase::StartWebEngine(
    base::CommandLine command_line) {
  web_context_provider_ = cr_fuchsia::ConnectContextProvider(
      web_engine_controller_.NewRequest(), std::move(command_line));
  web_context_provider_.set_error_handler(
      [](zx_status_t status) { ADD_FAILURE(); });
}

fuchsia::web::CreateContextParams
WebEngineIntegrationTestBase::DefaultContextParams() const {
  fuchsia::web::CreateContextParams create_params;
  auto directory =
      base::OpenDirectoryHandle(base::FilePath(base::kServiceDirectoryPath));
  EXPECT_TRUE(directory.is_valid());
  create_params.set_service_directory(std::move(directory));
  return create_params;
}

fuchsia::web::CreateContextParams
WebEngineIntegrationTestBase::DefaultContextParamsWithTestData() const {
  fuchsia::web::CreateContextParams create_params = DefaultContextParams();

  fuchsia::web::ContentDirectoryProvider provider;
  provider.set_name("testdata");
  base::FilePath pkg_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &pkg_path));
  provider.set_directory(base::OpenDirectoryHandle(
      pkg_path.AppendASCII("fuchsia/engine/test/data")));

  create_params.mutable_content_directories()->emplace_back(
      std::move(provider));

  return create_params;
}

fuchsia::web::CreateContextParams
WebEngineIntegrationTestBase::ContextParamsWithFilteredServiceDirectory() {
  filtered_service_directory_ =
      std::make_unique<base::fuchsia::FilteredServiceDirectory>(
          base::ComponentContextForProcess()->svc().get());
  fidl::InterfaceHandle<fuchsia::io::Directory> svc_dir;
  filtered_service_directory_->ConnectClient(svc_dir.NewRequest());

  // Push all services from /svc to the service directory.
  base::FileEnumerator file_enum(base::FilePath("/svc"), false,
                                 base::FileEnumerator::FILES);
  for (auto file = file_enum.Next(); !file.empty(); file = file_enum.Next()) {
    filtered_service_directory_->AddService(file.BaseName().value().c_str());
  }

  fuchsia::web::CreateContextParams create_params;
  create_params.set_service_directory(std::move(svc_dir));
  return create_params;
}

void WebEngineIntegrationTestBase::CreateNavigationListener(
    fuchsia::web::FramePtr* frame) {
  DCHECK(!navigation_listener_);
  navigation_listener_ = std::make_unique<cr_fuchsia::TestNavigationListener>();
  navigation_listener_binding_ =
      std::make_unique<fidl::Binding<fuchsia::web::NavigationEventListener>>(
          navigation_listener_.get());
  (*frame)->SetNavigationEventListener(
      navigation_listener_binding_->NewBinding());
}

void WebEngineIntegrationTestBase::AddNavigationControllerAndListenerToFrame(
    fuchsia::web::FramePtr* frame) {
  DCHECK(!navigation_controller_);

  (*frame)->GetNavigationController(navigation_controller_.NewRequest());
  navigation_controller_.set_error_handler(
      [](zx_status_t status) { ADD_FAILURE(); });

  CreateNavigationListener(frame);
}

void WebEngineIntegrationTestBase::CreateContext(
    fuchsia::web::CreateContextParams context_params) {
  DCHECK(!context_);
  web_context_provider_->Create(std::move(context_params),
                                context_.NewRequest());
  context_.set_error_handler([](zx_status_t status) { ADD_FAILURE(); });
}

fuchsia::web::FramePtr WebEngineIntegrationTestBase::CreateFrame() {
  DCHECK(context_);
  fuchsia::web::FramePtr frame;
  context_->CreateFrame(frame.NewRequest());
  frame.set_error_handler([](zx_status_t status) { ADD_FAILURE(); });
  return frame;
}

fuchsia::web::FramePtr WebEngineIntegrationTestBase::CreateFrameWithParams(
    fuchsia::web::CreateFrameParams frame_params) {
  DCHECK(context_);
  fuchsia::web::FramePtr frame;
  context_->CreateFrameWithParams(std::move(frame_params), frame.NewRequest());
  frame.set_error_handler([](zx_status_t status) { ADD_FAILURE(); });
  return frame;
}

void WebEngineIntegrationTestBase::CreateContextAndFrame(
    fuchsia::web::CreateContextParams context_params) {
  ASSERT_FALSE(frame_);

  CreateContext(std::move(context_params));

  frame_ = CreateFrame();
  AddNavigationControllerAndListenerToFrame(&frame_);
}

void WebEngineIntegrationTestBase::CreateContextAndFrameWithParams(
    fuchsia::web::CreateContextParams context_params,
    fuchsia::web::CreateFrameParams frame_params) {
  ASSERT_FALSE(frame_);

  CreateContext(std::move(context_params));

  frame_ = CreateFrameWithParams(std::move(frame_params));
  AddNavigationControllerAndListenerToFrame(&frame_);
}

void WebEngineIntegrationTestBase::CreateContextAndExpectError(
    fuchsia::web::CreateContextParams params,
    zx_status_t expected_error) {
  ASSERT_FALSE(context_);
  web_context_provider_->Create(std::move(params), context_.NewRequest());
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
  fuchsia::web::LoadUrlParams load_url_params;
  ASSERT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(), std::move(load_url_params), url.spec()));

  // Wait for the URL to finish loading.
  navigation_listener_->RunUntilUrlEquals(url);
}

void WebEngineIntegrationTestBase::LoadUrlWithUserActivation(
    base::StringPiece url) {
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(),
      cr_fuchsia::CreateLoadUrlParamsWithUserActivation(), url));
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
    base::StringPiece script) {
  base::Optional<base::Value> value =
      cr_fuchsia::ExecuteJavaScript(frame_.get(), script);
  return value ? value->GetString() : std::string();
}

double WebEngineIntegrationTestBase::ExecuteJavaScriptWithDoubleResult(
    base::StringPiece script) {
  base::Optional<base::Value> value =
      cr_fuchsia::ExecuteJavaScript(frame_.get(), script);
  return value ? value->GetDouble() : 0.0;
}

bool WebEngineIntegrationTestBase::ExecuteJavaScriptWithBoolResult(
    base::StringPiece script) {
  base::Optional<base::Value> value =
      cr_fuchsia::ExecuteJavaScript(frame_.get(), script);
  return value ? value->GetBool() : false;
}
