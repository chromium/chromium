// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/test_timeouts.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

static void PrintUsage() {
  printf("run_testserver --doc-root=relpath\n"
         "               [--http|--https|--ws|--wss|--ftp]\n"
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

  net::SpawnedTestServer::Type server_type;
  if (command_line->HasSwitch("http")) {
    server_type = net::SpawnedTestServer::TYPE_HTTP;
  } else if (command_line->HasSwitch("https")) {
    server_type = net::SpawnedTestServer::TYPE_HTTPS;
  } else if (command_line->HasSwitch("ws")) {
    server_type = net::SpawnedTestServer::TYPE_WS;
  } else if (command_line->HasSwitch("wss")) {
    server_type = net::SpawnedTestServer::TYPE_WSS;
  } else if (command_line->HasSwitch("ftp")) {
    server_type = net::SpawnedTestServer::TYPE_FTP;
  } else {
    // If no scheme switch is specified, select http or https scheme.
    // TODO(toyoshim): Remove this estimation.
    if (command_line->HasSwitch("ssl-cert"))
      server_type = net::SpawnedTestServer::TYPE_HTTPS;
    else
      server_type = net::SpawnedTestServer::TYPE_HTTP;
  }

  net::SpawnedTestServer::SSLOptions ssl_options;
  if (command_line->HasSwitch("ssl-cert")) {
    if (!net::SpawnedTestServer::UsingSSL(server_type)) {
      printf("Error: --ssl-cert is specified on non-secure scheme\n");
      PrintUsage();
      return -1;
    }
    std::string cert_option = command_line->GetSwitchValueASCII("ssl-cert");
    if (cert_option == "ok") {
      ssl_options.server_certificate =
          net::SpawnedTestServer::SSLOptions::CERT_OK;
    } else if (cert_option == "mismatched-name") {
      ssl_options.server_certificate =
          net::SpawnedTestServer::SSLOptions::CERT_MISMATCHED_NAME;
    } else if (cert_option == "expired") {
      ssl_options.server_certificate =
          net::SpawnedTestServer::SSLOptions::CERT_EXPIRED;
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

  std::unique_ptr<net::SpawnedTestServer> test_server;
  if (net::SpawnedTestServer::UsingSSL(server_type)) {
    test_server.reset(
        new net::SpawnedTestServer(server_type, ssl_options, doc_root));
  } else {
    test_server.reset(new net::SpawnedTestServer(server_type, doc_root));
  }

  if (!test_server->Start()) {
    printf("Error: failed to start test server. Exiting.\n");
    return -1;
  }

  if (!base::DirectoryExists(test_server->document_root())) {
    printf("Error: invalid doc root: \"%s\" does not exist!\n",
        base::UTF16ToUTF8(
            test_server->document_root().LossyDisplayName()).c_str());
    return -1;
  }

  printf("testserver running at %s (type ctrl+c to exit)\n",
         test_server->host_port_pair().ToString().c_str());

  base::RunLoop().Run();
  return 0;
}
