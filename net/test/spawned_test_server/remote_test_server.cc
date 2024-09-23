// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/spawned_test_server/remote_test_server.h"

#include <stdint.h>

#include <limits>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/test/spawned_test_server/remote_test_server_spawner_request.h"
#include "url/gurl.h"

namespace net {

namespace {

// Please keep in sync with dictionary SERVER_TYPES in testserver.py
std::string GetServerTypeString(BaseTestServer::Type type) {
  switch (type) {
    case BaseTestServer::TYPE_WS:
    case BaseTestServer::TYPE_WSS:
      return "ws";
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return std::string();
}

#if !BUILDFLAG(IS_FUCHSIA)
// Returns platform-specific path to the config file for the test server.
base::FilePath GetTestServerConfigFilePath() {
  base::FilePath dir;
#if BUILDFLAG(IS_ANDROID)
  base::PathService::Get(base::DIR_ANDROID_EXTERNAL_STORAGE, &dir);
#else
  base::PathService::Get(base::DIR_TEMP, &dir);
#endif
  return dir.AppendASCII("net-test-server-config");
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

// Reads base URL for the test server spawner. That URL is used to control the
// test server.
std::string GetSpawnerUrlBase() {
#if BUILDFLAG(IS_FUCHSIA)
  std::string spawner_url_base(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "remote-test-server-spawner-url-base"));
  LOG_IF(FATAL, spawner_url_base.empty())
      << "--remote-test-server-spawner-url-base missing from command line";
  return spawner_url_base;
#else   // BUILDFLAG(IS_FUCHSIA)
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath config_path = GetTestServerConfigFilePath();

  if (!base::PathExists(config_path))
    return "";

  std::string config_json;
  if (!ReadFileToString(config_path, &config_json))
    LOG(FATAL) << "Failed to read " << config_path.value();

  std::optional<base::Value> config = base::JSONReader::Read(config_json);
  if (!config)
    LOG(FATAL) << "Failed to parse " << config_path.value();

  std::string* result = config->GetDict().FindString("spawner_url_base");
  if (!result)
    LOG(FATAL) << "spawner_url_base is not specified in the config";

  return *result;
#endif  // BUILDFLAG(IS_FUCHSIA)
}

}  // namespace

RemoteTestServer::RemoteTestServer(Type type,
                                   const base::FilePath& document_root)
    : BaseTestServer(type), io_thread_("RemoteTestServer IO Thread") {
  if (!Init(document_root))
    NOTREACHED_IN_MIGRATION();
}

RemoteTestServer::RemoteTestServer(Type type,
                                   const SSLOptions& ssl_options,
                                   const base::FilePath& document_root)
    : BaseTestServer(type, ssl_options),
      io_thread_("RemoteTestServer IO Thread") {
  if (!Init(document_root))
    NOTREACHED_IN_MIGRATION();
}

RemoteTestServer::~RemoteTestServer() {
  Stop();
}

bool RemoteTestServer::StartInBackground() {
  DCHECK(!started());
  DCHECK(!start_request_);

  std::optional<base::Value::Dict> arguments_dict = GenerateArguments();
  if (!arguments_dict)
    return false;

  arguments_dict->Set("on-remote-server", base::Value());

  // Append the 'server-type' argument which is used by spawner server to
  // pass right server type to Python test server.
  arguments_dict->Set("server-type", GetServerTypeString(type()));

  // Generate JSON-formatted argument string.
  std::string arguments_string;
  base::JSONWriter::Write(*arguments_dict, &arguments_string);
  if (arguments_string.empty())
    return false;

  start_request_ = std::make_unique<RemoteTestServerSpawnerRequest>(
      io_thread_.task_runner(), GetSpawnerUrl("start"), arguments_string);

  return true;
}

bool RemoteTestServer::BlockUntilStarted() {
  DCHECK(start_request_);

  std::string server_data_json;
  bool request_result = start_request_->WaitForCompletion(&server_data_json);
  start_request_.reset();
  if (!request_result)
    return false;

  // Parse server_data_json.
  if (server_data_json.empty() ||
      !SetAndParseServerData(server_data_json, &remote_port_)) {
    LOG(ERROR) << "Could not parse server_data: " << server_data_json;
    return false;
  }

  SetPort(remote_port_);

  return SetupWhenServerStarted();
}

bool RemoteTestServer::Stop() {
  DCHECK(!start_request_);

  if (remote_port_) {
    std::unique_ptr<RemoteTestServerSpawnerRequest> kill_request =
        std::make_unique<RemoteTestServerSpawnerRequest>(
            io_thread_.task_runner(),
            GetSpawnerUrl(base::StringPrintf("kill?port=%d", remote_port_)),
            std::string());

    if (!kill_request->WaitForCompletion(nullptr))
      LOG(FATAL) << "Failed stopping RemoteTestServer";

    remote_port_ = 0;
  }

  CleanUpWhenStoppingServer();

  return true;
}

// On Android, the document root in the device is not the same as the document
// root in the host machine where the test server is launched. So prepend
// DIR_SRC_TEST_DATA_ROOT here to get the actual path of document root on the
// Android device.
base::FilePath RemoteTestServer::GetDocumentRoot() const {
  base::FilePath src_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir);
  return src_dir.Append(document_root());
}

bool RemoteTestServer::Init(const base::FilePath& document_root) {
  if (document_root.IsAbsolute())
    return false;

  spawner_url_base_ = GetSpawnerUrlBase();

  bool thread_started = io_thread_.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));
  CHECK(thread_started);

  // Unlike LocalTestServer, RemoteTestServer passes relative paths to the test
  // server. The test server fails on empty strings in some configurations.
  base::FilePath fixed_root = document_root;
  if (fixed_root.empty())
    fixed_root = base::FilePath(base::FilePath::kCurrentDirectory);
  SetResourcePath(fixed_root, base::FilePath()
                                  .AppendASCII("net")
                                  .AppendASCII("data")
                                  .AppendASCII("ssl")
                                  .AppendASCII("certificates"));
  return true;
}

GURL RemoteTestServer::GetSpawnerUrl(const std::string& command) const {
  CHECK(!spawner_url_base_.empty());
  std::string url = spawner_url_base_ + "/" + command;
  GURL result = GURL(url);
  CHECK(result.is_valid()) << url;
  return result;
}

}  // namespace net
