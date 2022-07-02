// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/logger/cpp/fidl.h>

#include <cstring>

#include "base/containers/contains.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/test_log_listener_safe.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/webengine/test/context_provider_test_connector.h"
#include "fuchsia_web/webengine/web_engine_integration_test_base.h"

namespace {

// Name of the console logging test page.
constexpr char kLogTestPageFileName[] = "console_logging.html";
constexpr char kLogTestPageDebugMessage[] = "This is a debug() message.";

// Debug name to create Frames with, to use as their logging tag.
constexpr char kFrameLogTag[] = "Test🖼🪵";

constexpr char kNormalizedPortNumber[] = "678";

// Replaces the line number in frame_impl.cc with kNormalizedLineNumber and
// the port with kNormalizedPortNumber to enable reliable comparison of
// console log messages.
std::string NormalizeConsoleLogMessage(base::StringPiece original) {
  const char kSchemePortColon[] = "http://127.0.0.1:";
  size_t port_begin =
      original.find(kSchemePortColon) + strlen(kSchemePortColon);
  size_t path_begin = original.find("/", port_begin);
  return std::string(original).replace(port_begin, path_begin - port_begin,
                                       kNormalizedPortNumber);
}

}  // namespace

class WebEngineIntegrationLoggingTest : public WebEngineIntegrationTestBase {
 protected:
  WebEngineIntegrationLoggingTest()
      : isolated_archivist_service_dir_(
            StartIsolatedArchivist(archivist_controller_.NewRequest())) {
    // Redirect the LogSink service to an isolated archivist instance.
    zx_status_t status = filtered_service_directory()
                             .outgoing_directory()
                             ->RemovePublicService<fuchsia::logger::LogSink>();
    ZX_CHECK(status == ZX_OK, status) << "RemovePublicService";

    status =
        filtered_service_directory().outgoing_directory()->AddPublicService(
            fidl::InterfaceRequestHandler<fuchsia::logger::LogSink>(
                [this](auto request) {
                  isolated_archivist_service_dir_.Connect(std::move(request));
                }));
    ZX_CHECK(status == ZX_OK, status) << "AddPublicService";
  }

  void SetUp() override {
    WebEngineIntegrationTestBase::SetUp();
    StartWebEngineForLoggingTest(
        base::CommandLine(base::CommandLine::NO_PROGRAM));

    log_ = isolated_archivist_service_dir_.Connect<fuchsia::logger::Log>();
  }

  fuchsia::logger::Log* log() { return log_.get(); }

 private:
  // Starts WebEngine without redirecting its logs.
  void StartWebEngineForLoggingTest(base::CommandLine command_line) {
    web_context_provider_ = ConnectContextProviderForLoggingTest(
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

  fuchsia::sys::ComponentControllerPtr archivist_controller_;
  sys::ServiceDirectory isolated_archivist_service_dir_;

  fuchsia::logger::LogPtr log_;
};

// Verifies that calling messages from console.debug() calls go to the Fuchsia
// system log when the script log level is set to DEBUG.
TEST_F(WebEngineIntegrationLoggingTest, SetJavaScriptLogLevel_DEBUG) {
  base::SimpleTestLogListener log_listener;
  log_listener.ListenToLog(log(), nullptr);

  // Create the Context & Frame with all log severities enabled.
  CreateContext(TestContextParams());
  fuchsia::web::CreateFrameParams frame_params;
  frame_params.set_debug_name(kFrameLogTag);
  CreateFrameWithParams(std::move(frame_params));
  frame_->SetJavaScriptLogLevel(fuchsia::web::ConsoleLogLevel::DEBUG);

  // Navigate to the test page, which will emit console logging.
  ASSERT_NO_FATAL_FAILURE(LoadUrlAndExpectResponse(
      embedded_test_server_.GetURL(std::string("/") + kLogTestPageFileName)
          .spec()));
  navigation_listener()->RunUntilTitleEquals("ended");

  // Run until the message passed to console.debug() is received.
  absl::optional<fuchsia::logger::LogMessage> logged_message =
      log_listener.RunUntilMessageReceived(kLogTestPageDebugMessage);

  ASSERT_TRUE(logged_message.has_value());

  // console.debug() should map to Fuchsia's DEBUG log severity.
  EXPECT_EQ(logged_message->severity,
            static_cast<int32_t>(fuchsia::logger::LogLevelFilter::DEBUG));

  // Verify that the Frame's |debug_name| is amongst the log message tags.
  EXPECT_FALSE(logged_message->tags.empty());
  EXPECT_TRUE(base::Contains(logged_message->tags, kFrameLogTag));

  // Verify that the message is formatted as expected.
  EXPECT_EQ(NormalizeConsoleLogMessage(logged_message->msg),
            base::StringPrintf("[http://127.0.0.1:%s/console_logging.html(8)] "
                               "This is a debug() message.",
                               kNormalizedPortNumber));
}
