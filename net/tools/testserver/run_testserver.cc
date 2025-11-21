// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <memory>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/logging/logging_settings.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/test_timeouts.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/install_default_websocket_handlers.h"

static void PrintUsage() {
  printf(
      "run_testserver --doc-root=relpath\n"
      "               [--http|--https|--ws|--wss]\n"
      "               [--ssl-cert=ok|mismatched-name|expired]\n");
  printf("(NOTE: relpath should be relative to the 'src' directory.\n");
}

int main(int argc, const char* argv[]) {
  base::AtExitManager at_exit_manager;
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);

  // Process command line
  base::CommandLine::Init(argc, argv);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file_path = FILE_PATH_LITERAL("testserver.log");
  if (!logging::InitLogging(settings)) {
    printf("Error: could not initialize logging. Exiting.\n");
    return -1;
  }

  TestTimeouts::Initialize();

  if (command_line->GetSwitches().empty() ||
      command_line->HasSwitch("help")) {
    PrintUsage();
    return -1;
  }

  // Default to HTTP.
  net::EmbeddedTestServer::Type embedded_test_server_type =
      net::EmbeddedTestServer::TYPE_HTTP;
  bool enable_websockets = false;

  if (command_line->HasSwitch("http")) {
    embedded_test_server_type = net::EmbeddedTestServer::TYPE_HTTP;
  } else if (command_line->HasSwitch("https")) {
    embedded_test_server_type = net::EmbeddedTestServer::TYPE_HTTPS;
  } else if (command_line->HasSwitch("ws")) {
    embedded_test_server_type = net::EmbeddedTestServer::TYPE_HTTP;
    enable_websockets = true;
  } else if (command_line->HasSwitch("wss")) {
    embedded_test_server_type = net::EmbeddedTestServer::TYPE_HTTPS;
    enable_websockets = true;
  } else {
    // If no scheme switch is specified, select https if "ssl-cert" is
    // specified.
    // TODO(toyoshim): Remove this estimation.
    if (command_line->HasSwitch("ssl-cert")) {
      embedded_test_server_type = net::EmbeddedTestServer::TYPE_HTTPS;
    }
  }

  net::EmbeddedTestServer::ServerCertificate server_certificate;
  if (command_line->HasSwitch("ssl-cert")) {
    if (embedded_test_server_type != net::EmbeddedTestServer::TYPE_HTTPS) {
      printf("Error: --ssl-cert is specified on non-secure scheme\n");
      PrintUsage();
      return -1;
    }
    std::string cert_option = command_line->GetSwitchValueASCII("ssl-cert");
    if (cert_option == "ok") {
      server_certificate = net::EmbeddedTestServer::CERT_OK;
    } else if (cert_option == "mismatched-name") {
      server_certificate = net::EmbeddedTestServer::CERT_MISMATCHED_NAME;
    } else if (cert_option == "expired") {
      server_certificate = net::EmbeddedTestServer::CERT_EXPIRED;
    } else {
      printf("Error: --ssl-cert has invalid value %s\n", cert_option.c_str());
      PrintUsage();
      return -1;
    }
  }

  base::FilePath doc_root = command_line->GetSwitchValuePath("doc-root");
  if (doc_root.empty()) {
    printf("Error: --doc-root must be specified\n");
    PrintUsage();
    return -1;
  }

  base::FilePath full_path =
      net::test_server::EmbeddedTestServer::GetFullPathFromSourceDirectory(
          doc_root);
  if (!base::DirectoryExists(full_path)) {
    printf("Error: invalid doc root: \"%s\" does not exist!\n",
           base::UTF16ToUTF8(full_path.LossyDisplayName()).c_str());
    return -1;
  }

  net::EmbeddedTestServer embedded_test_server(embedded_test_server_type);
  if (embedded_test_server_type == net::EmbeddedTestServer::TYPE_HTTPS) {
    embedded_test_server.SetSSLConfig(server_certificate);
  }

  embedded_test_server.AddDefaultHandlers(doc_root);
  if (enable_websockets) {
    net::test_server::InstallDefaultWebSocketHandlers(&embedded_test_server);
  }

  if (!embedded_test_server.Start()) {
    printf("Error: failed to start embedded test server. Exiting.\n");
    return -1;
  }

  printf("Embedded test server running at %s (type ctrl+c to exit)\n",
         embedded_test_server.host_port_pair().ToString().c_str());

  base::RunLoop().Run();
  return 0;
}
