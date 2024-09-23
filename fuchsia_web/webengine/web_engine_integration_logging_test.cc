// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fidl/fuchsia.logger/cpp/fidl.h>

#include <cstring>
#include <optional>
#include <string_view>

#include "base/containers/contains.h"
#include "base/fuchsia/test_log_listener_safe.h"
#include "base/strings/stringprintf.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/webengine/test/context_provider_for_test.h"
#include "fuchsia_web/webengine/test/isolated_archivist.h"
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
std::string NormalizeConsoleLogMessage(std::string_view original) {
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
      : isolated_archivist_(
            *filtered_service_directory().outgoing_directory()) {}

  ~WebEngineIntegrationLoggingTest() override {
    // We're about to shut down the realm; unbind to unhook the error handler.
    frame_.Unbind();
    context_.Unbind();
  }

  void StartWebEngine(base::CommandLine command_line) override {
    context_provider_.emplace(
        ContextProviderForTest::Create(std::move(command_line)));
    context_provider_->ptr().set_error_handler(
        [](zx_status_t status) { FAIL() << zx_status_get_string(status); });
  }

  fuchsia::web::ContextProvider* GetContextProvider() override {
    return context_provider_->get();
  }

  fidl::Client<fuchsia_logger::Log>& log() { return isolated_archivist_.log(); }

  IsolatedArchivist isolated_archivist_;
  std::optional<ContextProviderForTest> context_provider_;
};

// Verifies that calling messages from console.debug() calls go to the Fuchsia
// system log when the script log level is set to DEBUG.
TEST_F(WebEngineIntegrationLoggingTest, SetJavaScriptLogLevel_DEBUG) {
  StartWebEngine(base::CommandLine(base::CommandLine::NO_PROGRAM));
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
  std::optional<fuchsia_logger::LogMessage> logged_message =
      log_listener.RunUntilMessageReceived(kLogTestPageDebugMessage);

  ASSERT_TRUE(logged_message.has_value());

  // console.debug() should map to Fuchsia's DEBUG log severity.
  EXPECT_EQ(logged_message->severity(),
            static_cast<int32_t>(fuchsia_logger::LogLevelFilter::kDebug));

  // Verify that the Frame's |debug_name| is amongst the log message tags.
  EXPECT_FALSE(logged_message->tags().empty());
  EXPECT_TRUE(base::Contains(logged_message->tags(), kFrameLogTag));

  // Verify that the message is formatted as expected.
  EXPECT_EQ(NormalizeConsoleLogMessage(logged_message->msg()),
            base::StringPrintf("[http://127.0.0.1:%s/console_logging.html(8)] "
                               "This is a debug() message.",
                               kNormalizedPortNumber));
}
