// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/logger/cpp/fidl.h>

#include <cstring>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/test_log_listener_safe.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "fuchsia/base/context_provider_test_connector.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/engine/web_engine_integration_test_base.h"

namespace {

constexpr char kLogTestPageFileName[] = "console_logging.html";
constexpr char kWebEngineLogTag[] = "web_engine_exe";
constexpr char kNormalizedLineNumber[] = "12345";
constexpr char kNormalizedPortNumber[] = "678";

// Replaces the line number in frame_impl.cc with kNormalizedLineNumber and
// the port with kNormalizedPortNumber to enable reliable comparison of
// console log messages.
std::string NormalizeConsoleLogMessage(base::StringPiece original) {
  size_t line_number_begin = original.find("(") + 1;
  size_t close_parenthesis = original.find(")", line_number_begin);
  std::string normalized = original.as_string().replace(
      line_number_begin, close_parenthesis - line_number_begin,
      kNormalizedLineNumber);

  const char kSchemePortColon[] = "http://127.0.0.1:";
  size_t port_begin =
      normalized.find(kSchemePortColon) + strlen(kSchemePortColon);
  size_t path_begin = normalized.find("/", port_begin);
  return normalized.replace(port_begin, path_begin - port_begin,
                            kNormalizedPortNumber);
}

}  // namespace

class WebEngineIntegrationLoggingTest : public WebEngineIntegrationTestBase {
 protected:
  WebEngineIntegrationLoggingTest()
      : WebEngineIntegrationTestBase(),
        isolated_archivist_service_dir_(
            StartIsolatedArchivist(archivist_controller_.NewRequest())),
        binding_(&test_log_listener_) {}

  void SetUp() override {
    WebEngineIntegrationTestBase::SetUp();
    StartWebEngineForLoggingTest(
        base::CommandLine(base::CommandLine::NO_PROGRAM));

    logger_ = isolated_archivist_service_dir_.Connect<fuchsia::logger::Log>();
    logger_.set_error_handler([&](zx_status_t status) {
      ZX_LOG(ERROR, status) << "fuchsia.logger.Log disconnected";
      ADD_FAILURE();
      wait_for_message_loop_.Quit();
    });
  }

  // Starts WebEngine without redirecting its logs.
  void StartWebEngineForLoggingTest(base::CommandLine command_line) {
    web_context_provider_ = cr_fuchsia::ConnectContextProviderForLoggingTest(
        web_engine_controller_.NewRequest(), std::move(command_line));
    web_context_provider_.set_error_handler(
        [](zx_status_t status) { ADD_FAILURE(); });
  }

  // Starts an isolated instance of Archivist to receive and dump log statements
  // via the fuchsia.logger.Log* APIs.
  fidl::InterfaceHandle<fuchsia::io::Directory> StartIsolatedArchivist(
      fidl::InterfaceRequest<fuchsia::sys::ComponentController>
          component_controller_request) {
    const char kArchivistUrl[] =
        "fuchsia-pkg://fuchsia.com/archivist-for-embedding#meta/"
        "archivist-for-embedding.cmx";

    fuchsia::sys::LaunchInfo launch_info;
    launch_info.url = kArchivistUrl;

    fidl::InterfaceHandle<fuchsia::io::Directory> archivist_services_dir;
    launch_info.directory_request =
        archivist_services_dir.NewRequest().TakeChannel();

    auto launcher = base::ComponentContextForProcess()
                        ->svc()
                        ->Connect<fuchsia::sys::Launcher>();
    launcher->CreateComponent(std::move(launch_info),
                              std::move(component_controller_request));

    return archivist_services_dir;
  }

  // Returns a CreateContextParams that has an isolated LogSink service from
  // |isolated_archivist_service_dir_|.
  fuchsia::web::CreateContextParams ContextParamsWithIsolatedLogSink() {
    // Use a FilteredServiceDirectory in order to inject an isolated service.
    fuchsia::web::CreateContextParams create_params =
        ContextParamsWithFilteredServiceDirectory();

    EXPECT_EQ(filtered_service_directory_->outgoing_directory()
                  ->RemovePublicService<fuchsia::logger::LogSink>(),
              ZX_OK);

    EXPECT_EQ(
        filtered_service_directory_->outgoing_directory()->AddPublicService(
            std::make_unique<vfs::Service>(
                [this](zx::channel channel, async_dispatcher_t* dispatcher) {
                  isolated_archivist_service_dir_.Connect(
                      fuchsia::logger::LogSink::Name_, std::move(channel));
                }),
            fuchsia::logger::LogSink::Name_),
        ZX_OK);

    return create_params;
  }

  // Checks whether the expected log line has been received yet,
  // and invokes DumpLogsSafe() if not. It passes itself as the completion
  // callback, so that when the call completes it can check again for the
  // expected message and re-invoke DumpLogsSafe(), or quit the loop, as
  // appropriate.
  void DumpLogs() {
    // Look for kLogTestPageFileName because that string is unique to the
    // console logs generated by these tests.
    if (test_log_listener_.DidReceiveString(kLogTestPageFileName,
                                            &logged_message_)) {
      wait_for_message_loop_.Quit();
      return;
    }

    std::unique_ptr<fuchsia::logger::LogFilterOptions> options =
        std::make_unique<fuchsia::logger::LogFilterOptions>();
    options->tags = {kWebEngineLogTag};

    test_log_listener_.set_on_dump_logs_done(base::BindOnce(
        &WebEngineIntegrationLoggingTest::DumpLogs, base::Unretained(this)));
    logger_->DumpLogsSafe(binding_.NewBinding(), std::move(options));
  }

  void LoadLogTestPage() {
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        navigation_controller_.get(), fuchsia::web::LoadUrlParams(),
        embedded_test_server_.GetURL(std::string("/") + kLogTestPageFileName)
            .spec()));
  }

  fuchsia::sys::ComponentControllerPtr archivist_controller_;
  sys::ServiceDirectory isolated_archivist_service_dir_;

  fuchsia::logger::LogPtr logger_;
  base::TestLogListenerSafe test_log_listener_;
  fidl::Binding<fuchsia::logger::LogListenerSafe> binding_;

  fuchsia::logger::LogMessage logged_message_;

  base::RunLoop wait_for_message_loop_;
};

// Verifies that calling messages from console.debug() calls go to the Fuchsia
// system log when the script log level is set to DEBUG.
TEST_F(WebEngineIntegrationLoggingTest, SetJavaScriptLogLevel_DEBUG) {
  CreateContextAndFrame(ContextParamsWithIsolatedLogSink());

  // Enable all logging.
  frame_->SetJavaScriptLogLevel(fuchsia::web::ConsoleLogLevel::DEBUG);

  LoadLogTestPage();
  navigation_listener_->RunUntilTitleEquals("ended");

  // Start the first DumpLogs() call.
  DumpLogs();

  // Run until kLogMessage is received.
  wait_for_message_loop_.Run();

  EXPECT_EQ(logged_message_.severity,
            static_cast<int32_t>(fuchsia::logger::LogLevelFilter::INFO));
  ASSERT_EQ(logged_message_.tags.size(), 1u);
  EXPECT_EQ(logged_message_.tags[0], kWebEngineLogTag);
  EXPECT_EQ(NormalizeConsoleLogMessage(logged_message_.msg),
            "[frame_impl.cc(" + std::string(kNormalizedLineNumber) +
                ")] debug:http://127.0.0.1:" + kNormalizedPortNumber +
                "/console_logging.html:8 "
                ": This is a debug() message.");
}
